import { PRESETS, DEFAULT_PRESET_INDEX, KNOB_IDS } from "./presets.js";
import { createKnob } from "./knob.js";
import { createVisualizer } from "./visualizer.js";
import { createAudioGraph } from "./audioGraph.js";
import { createStemPlayers } from "./stemPlayers.js";

// Krumhansl-Schmuckler key profiles, ported verbatim from
// Source/PluginProcessor.cpp::updateKeyEstimate().
const MAJOR_PROFILE = [6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88];
const MINOR_PROFILE = [6.33, 2.68, 3.52, 5.38, 2.6, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17];
const NOTE_NAMES = ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"];

const $ = (sel) => document.querySelector(sel);

const pluginEl = $("#plugin");
const presetSelect = $("#presetSelect");
const btnBefore = $("#btnBefore");
const btnAfter = $("#btnAfter");
const waveCanvas = $("#waveCanvas");
const polishMeterCanvas = $("#polishMeter");
const polishSlider = $("#polishSlider");
const polishValue = $("#polishValue");
const statusLabel = $("#statusLabel");
const stemLabel = $("#stemLabel");
const timeText = $("#timeText");
const keyText = $("#keyText");
const dbText = $("#dbText");
const maleAudio = $("#maleAudio");
const femaleAudio = $("#femaleAudio");
const maleStemBtn = $("#maleStem");
const femaleStemBtn = $("#femaleStem");

const defaults = PRESETS[DEFAULT_PRESET_INDEX];
const knobs = { tune: defaults.tune, clean: defaults.clean, eq: defaults.eq, compress: defaults.compress, air: defaults.air, space: defaults.space };
let polish = defaults.polish;
let bypassed = false;
let pcWeights = new Array(12).fill(0);
let keyIndex = -1;

const graph = createAudioGraph();
// Created up front (with no analyser yet) so the waveform + polish meter render a resting
// state immediately, matching the plugin UI; the live analyser is attached on first play.
const visualizer = createVisualizer({ waveCanvas, polishMeterCanvas });

// --- preset dropdown ---
PRESETS.forEach((p, i) => {
  const opt = document.createElement("option");
  opt.value = String(i);
  opt.textContent = p.name;
  presetSelect.appendChild(opt);
});
presetSelect.value = String(DEFAULT_PRESET_INDEX);
presetSelect.addEventListener("change", () => {
  const preset = PRESETS[+presetSelect.value];
  KNOB_IDS.forEach((id) => {
    knobs[id] = preset[id];
    knobWidgets[id].setValue(preset[id], false);
    updateKnobText(id, preset[id]);
  });
  polish = preset.polish;
  applyPolishUI(polish);
  graph.setParams(knobs, polish);
});

// --- knobs ---
const knobWidgets = {};
function updateKnobText(id, value) {
  document.querySelector(`[data-value-for="${id}"]`).textContent = `${Math.round(value)}%`;
}
KNOB_IDS.forEach((id) => {
  const canvas = document.querySelector(`.knob-canvas[data-param="${id}"]`);
  knobWidgets[id] = createKnob(canvas, {
    min: 0,
    max: 100,
    value: knobs[id],
    processing: !bypassed,
    onChange(v) {
      knobs[id] = v;
      updateKnobText(id, v);
      graph.setParams(knobs, polish);
    },
  });
});

// --- polish slider ---
function applyPolishUI(v) {
  polishSlider.value = String(v);
  polishSlider.style.setProperty("--fill", `${v}%`);
  polishValue.textContent = `${Math.round(v)}%`;
}
applyPolishUI(polish);
polishSlider.addEventListener("input", () => {
  polish = +polishSlider.value;
  applyPolishUI(polish);
  graph.setParams(knobs, polish);
});

// --- before/after ---
function setBypassed(b) {
  bypassed = b;
  pluginEl.classList.toggle("bypassed", b);
  btnBefore.classList.toggle("active", b);
  btnAfter.classList.toggle("active", !b);
  statusLabel.textContent = b ? "BYPASSED" : "PROCESSING";
  KNOB_IDS.forEach((id) => knobWidgets[id].setProcessing(!b));
  if (visualizer) visualizer.setProcessing(!b);
  graph.setBypassed(b);
}
btnBefore.addEventListener("click", () => setBypassed(true));
btnAfter.addEventListener("click", () => setBypassed(false));

// --- key estimate (Krumhansl-Schmuckler), fed by the tune worklet's per-hop pitch messages ---
function correlate(profile, rot) {
  let sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0;
  for (let i = 0; i < 12; i++) {
    const x = pcWeights[(i + rot) % 12];
    const y = profile[i];
    sx += x; sy += y; sxx += x * x; syy += y * y; sxy += x * y;
  }
  const denom = Math.sqrt((12 * sxx - sx * sx) * (12 * syy - sy * sy));
  return denom > 1e-9 ? (12 * sxy - sx * sy) / denom : 0;
}
function handlePitchMessage({ freq, conf }) {
  if (!(conf > 0.55 && freq > 70 && freq < 950)) return;
  const midi = 69 + 12 * Math.log2(freq / 440);
  let pc = Math.round(midi) % 12;
  if (pc < 0) pc += 12;

  let total = 0;
  for (let i = 0; i < 12; i++) {
    pcWeights[i] *= 0.995;
    total += pcWeights[i];
  }
  pcWeights[pc] += conf;
  total += conf;
  if (total < 8) return;

  let best = -1, bestR = -2;
  for (let k = 0; k < 12; k++) {
    const rMaj = correlate(MAJOR_PROFILE, k);
    const rMin = correlate(MINOR_PROFILE, k);
    if (rMaj > bestR) { bestR = rMaj; best = k; }
    if (rMin > bestR) { bestR = rMin; best = k + 12; }
  }
  keyIndex = best;
}
function keyDisplayString() {
  if (keyIndex < 0) return "KEY: —";
  return `KEY: ${NOTE_NAMES[keyIndex % 12]} ${keyIndex < 12 ? "MAJOR" : "MINOR"}`;
}

// --- stem players ---
let activeStemKey = null;
const stemPlayers = createStemPlayers({
  audioEls: { male: maleAudio, female: femaleAudio },
  buttons: { male: maleStemBtn, female: femaleStemBtn },
  graph,
  onAudioReady() {
    visualizer.setAnalyser(graph.getAnalyser());
    graph.setParams(knobs, polish);
    graph.setBypassed(bypassed);
    graph.onPitchMessage(handlePitchMessage);
  },
  onActiveChange(key) {
    activeStemKey = key;
    stemLabel.textContent = key ? `${key.toUpperCase()} VOCALS` : "NONE SELECTED";
  },
});

// --- 30fps UI loop (visualizer, time/key/dB readouts) ---
function formatTime(t) {
  const mins = Math.floor(t / 60);
  const secs = Math.floor(t % 60);
  const tenths = Math.floor((t * 10) % 10);
  return `${String(mins).padStart(2, "0")}:${String(secs).padStart(2, "0")}.${tenths}`;
}
function loop(now) {
  requestAnimationFrame(loop);
  const updated = visualizer.tick(now);
  if (!updated) return;

  const el = activeStemKey ? (activeStemKey === "male" ? maleAudio : femaleAudio) : null;
  timeText.textContent = formatTime(el ? el.currentTime : 0);
  keyText.textContent = keyDisplayString();

  const level = visualizer.getSmoothLevel();
  const db = 20 * Math.log10(Math.max(level, 1e-3));
  dbText.textContent = db <= -59.9 ? "-∞ dB" : `${db.toFixed(1)} dB`;
}

visualizer.setProcessing(!bypassed);
requestAnimationFrame(loop);
