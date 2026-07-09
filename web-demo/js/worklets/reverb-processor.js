// Ports JUCE's juce::Reverb (classic Freeverb: 8 parallel combs -> 4 series allpasses per
// channel), the algorithm behind Source/PluginProcessor.cpp's `juce::dsp::Reverb reverb;`.
// Constants and per-sample recursions taken directly from
// JUCE/modules/juce_audio_basics/utilities/juce_Reverb.h.
//
// Plugin's fixed Space params: roomSize=0.50+0.28*spaceEff, damping=0.45 (fixed),
// wetLevel=0.42*spaceEff, dryLevel=1.0, width=1.0. With width=1.0, wetGain2=0 (no
// cross-channel feed needed), so this worklet outputs WET ONLY -- the dry signal is
// mixed in separately by the main graph, reusing the same dry tap as the bypass switch.
const COMB_TUNINGS = [1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617];
const ALLPASS_TUNINGS = [556, 441, 341, 225];
const STEREO_SPREAD = 23;
const INPUT_GAIN = 0.015;
const DAMP_COEF = 0.45 * 0.4; // damping fixed at 0.45 in the plugin -> constant coefficient

class CombFilter {
  constructor(size) {
    this.buffer = new Float32Array(size);
    this.idx = 0;
    this.last = 0;
  }
  process(input, damp, feedback) {
    const out = this.buffer[this.idx];
    this.last = out * (1 - damp) + this.last * damp;
    this.buffer[this.idx] = input + this.last * feedback;
    this.idx = this.idx + 1 === this.buffer.length ? 0 : this.idx + 1;
    return out;
  }
}

class AllPassFilter {
  constructor(size) {
    this.buffer = new Float32Array(size);
    this.idx = 0;
  }
  process(input) {
    const bufferedValue = this.buffer[this.idx];
    this.buffer[this.idx] = input + bufferedValue * 0.5;
    const output = bufferedValue - input;
    this.idx = this.idx + 1 === this.buffer.length ? 0 : this.idx + 1;
    return output;
  }
}

class ReverbProcessor extends AudioWorkletProcessor {
  static get parameterDescriptors() {
    return [{ name: "spaceEff", defaultValue: 0, minValue: 0, maxValue: 1, automationRate: "a-rate" }];
  }

  constructor() {
    super();
    const scale = sampleRate / 44100;
    this.combs = [[], []];
    this.allpasses = [[], []];
    for (let ch = 0; ch < 2; ch++) {
      const spread = ch === 1 ? STEREO_SPREAD : 0;
      for (const t of COMB_TUNINGS) this.combs[ch].push(new CombFilter(Math.max(1, Math.floor((t + spread) * scale))));
      for (const t of ALLPASS_TUNINGS)
        this.allpasses[ch].push(new AllPassFilter(Math.max(1, Math.floor((t + spread) * scale))));
    }
  }

  process(inputs, outputs, parameters) {
    const input = inputs[0];
    const output = outputs[0];
    if (!input || input.length === 0 || !output || output.length === 0) return true;

    const spaceEffArr = parameters.spaceEff;
    const n = input[0].length;
    const numCh = Math.min(2, input.length, output.length);

    for (let i = 0; i < n; i++) {
      const spaceEff = spaceEffArr.length > 1 ? spaceEffArr[i] : spaceEffArr[0];
      const roomSize = 0.5 + 0.28 * spaceEff;
      const feedback = roomSize * 0.28 + 0.7;
      const wet = 0.42 * spaceEff * 3;

      for (let ch = 0; ch < numCh; ch++) {
        const x = input[ch][i] * INPUT_GAIN;
        let sum = 0;
        for (const c of this.combs[ch]) sum += c.process(x, DAMP_COEF, feedback);
        for (const ap of this.allpasses[ch]) sum = ap.process(sum);
        output[ch][i] = sum * wet;
      }
    }
    for (let ch = numCh; ch < output.length; ch++) output[ch].set(output[0]);
    return true;
  }
}

registerProcessor("tt-reverb", ReverbProcessor);
