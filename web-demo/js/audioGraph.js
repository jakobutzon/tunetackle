// Builds the Web Audio graph mirroring Source/PluginProcessor.cpp::processBlock's chain:
// source -> HPF -> Gate -> Tune -> Mud EQ -> Presence EQ -> Compress+makeup -> Air ->
// Reverb(wet) + dry-of-reverb-tap -> wetGain --\
//                                                +--> outputMix -> Analyser -> destination
// source(raw, pre-Clean) ------------------> dryGain -----------/
//
// Before/After bypass is a hard, instantaneous binary switch (setValueAtTime, never a
// ramp) between dryGain and wetGain, matching the plugin's own `if (!bypassed)` early exit.
import { effectiveAmounts } from "./presets.js";

// Versioned query string: AudioWorklet modules are cached very aggressively by the browser
// (a known Chromium quirk) and often won't refetch across reloads without this bump.
const WORKLET_VERSION = "v2";
const WORKLET_MODULES = ["js/worklets/gate-processor.js", "js/worklets/tune-processor.js", "js/worklets/reverb-processor.js"].map(
  (u) => `${u}?${WORKLET_VERSION}`
);
const SMOOTH_TIME = 0.012;

export function createAudioGraph() {
  let ctx = null;
  let nodes = null;
  let bypassed = false;

  async function ensureStarted() {
    if (ctx) {
      if (ctx.state === "suspended") await ctx.resume();
      return;
    }
    ctx = new (window.AudioContext || window.webkitAudioContext)();
    for (const url of WORKLET_MODULES) await ctx.audioWorklet.addModule(url);

    const headGain = ctx.createGain(); // raw source junction (both stems feed this)
    const wetChainGate = ctx.createGain(); // silences the wet chain's input while bypassed

    const hpf = ctx.createBiquadFilter();
    hpf.type = "highpass";
    hpf.Q.value = 0.7071;

    const gateNode = new AudioWorkletNode(ctx, "tt-gate", {
      numberOfInputs: 1,
      numberOfOutputs: 1,
      channelCount: 2,
      channelCountMode: "explicit",
    });

    const tuneNode = new AudioWorkletNode(ctx, "tt-tune", {
      numberOfInputs: 2,
      numberOfOutputs: 1,
      channelCount: 2,
      channelCountMode: "explicit",
    });

    const mudEQ = ctx.createBiquadFilter();
    mudEQ.type = "peaking";
    mudEQ.frequency.value = 260;
    mudEQ.Q.value = 0.8;

    const presenceEQ = ctx.createBiquadFilter();
    presenceEQ.type = "peaking";
    presenceEQ.frequency.value = 3400;
    presenceEQ.Q.value = 0.7;

    const compressor = ctx.createDynamicsCompressor();
    compressor.knee.value = 0;
    compressor.attack.value = 0.008;
    compressor.release.value = 0.14;

    const makeupGain = ctx.createGain();

    const airShelf = ctx.createBiquadFilter();
    airShelf.type = "highshelf";
    airShelf.frequency.value = 11000;

    const reverbNode = new AudioWorkletNode(ctx, "tt-reverb", {
      numberOfInputs: 1,
      numberOfOutputs: 1,
      channelCount: 2,
      channelCountMode: "explicit",
    });
    const reverbDryTap = ctx.createGain(); // dryLevel=1.0 fixed, always passes alongside the wet reverb tail
    reverbDryTap.gain.value = 1.0;

    const wetSum = ctx.createGain();
    const dryGain = ctx.createGain();
    const wetGain = ctx.createGain();
    dryGain.gain.value = bypassed ? 1 : 0;
    wetGain.gain.value = bypassed ? 0 : 1;

    const outputMix = ctx.createGain();
    const analyser = ctx.createAnalyser();
    analyser.fftSize = 1024;

    // --- wiring ---
    headGain.connect(dryGain);
    headGain.connect(wetChainGate);
    headGain.connect(tuneNode, 0, 1); // raw pre-Clean tap for pitch DETECTION only

    wetChainGate.connect(hpf);
    hpf.connect(gateNode);
    gateNode.connect(tuneNode, 0, 0); // post-Clean signal, actually gets shifted

    tuneNode.connect(mudEQ);
    mudEQ.connect(presenceEQ);
    presenceEQ.connect(compressor);
    compressor.connect(makeupGain);
    makeupGain.connect(airShelf);

    airShelf.connect(reverbNode);
    airShelf.connect(reverbDryTap);
    reverbNode.connect(wetSum);
    reverbDryTap.connect(wetSum);

    wetSum.connect(wetGain);
    dryGain.connect(outputMix);
    wetGain.connect(outputMix);
    outputMix.connect(analyser);
    analyser.connect(ctx.destination);

    nodes = {
      headGain,
      wetChainGate,
      hpf,
      gateNode,
      tuneNode,
      mudEQ,
      presenceEQ,
      compressor,
      makeupGain,
      airShelf,
      reverbNode,
      dryGain,
      wetGain,
      analyser,
    };

    if (pendingPitchHandler) tuneNode.port.onmessage = (e) => pendingPitchHandler(e.data);
    setBypassed(bypassed);
  }

  let pendingPitchHandler = null;
  function onPitchMessage(handler) {
    pendingPitchHandler = handler;
    if (nodes) nodes.tuneNode.port.onmessage = (e) => handler(e.data);
  }

  function setParams(knobs, polish) {
    if (!nodes) return;
    const e = effectiveAmounts(knobs, polish);
    const now = ctx.currentTime;
    const t = SMOOTH_TIME;

    nodes.hpf.frequency.setTargetAtTime(40 + 90 * e.clean, now, t);
    const thresholdDb = e.clean > 0.001 ? -72 + 38 * e.clean : -120;
    nodes.gateNode.parameters.get("thresholdDb").setTargetAtTime(thresholdDb, now, t);

    nodes.tuneNode.parameters.get("tuneEff").setTargetAtTime(e.tune, now, t);

    nodes.mudEQ.gain.setTargetAtTime(-4.5 * e.eq, now, t);
    nodes.presenceEQ.gain.setTargetAtTime(4.5 * e.eq, now, t);

    nodes.compressor.threshold.setTargetAtTime(-6 - 26 * e.compress, now, t);
    nodes.compressor.ratio.setTargetAtTime(1 + 3.5 * e.compress, now, t);
    nodes.makeupGain.gain.setTargetAtTime(Math.pow(10, (8 * e.compress) / 20), now, t);

    nodes.airShelf.gain.setTargetAtTime(6 * e.air, now, t);

    nodes.reverbNode.parameters.get("spaceEff").setTargetAtTime(e.space, now, t);
  }

  function setBypassed(b) {
    bypassed = b;
    if (!nodes) return;
    const future = ctx.currentTime + 0.001;
    [nodes.dryGain.gain, nodes.wetGain.gain, nodes.wetChainGate.gain].forEach((p) => p.cancelScheduledValues(future));
    nodes.dryGain.gain.setValueAtTime(bypassed ? 1 : 0, future);
    nodes.wetGain.gain.setValueAtTime(bypassed ? 0 : 1, future);
    nodes.wetChainGate.gain.setValueAtTime(bypassed ? 0 : 1, future);
  }

  return {
    ensureStarted,
    setParams,
    setBypassed,
    onPitchMessage,
    getHeadGain: () => nodes && nodes.headGain,
    getAnalyser: () => nodes && nodes.analyser,
    getContext: () => ctx,
  };
}
