// Ports JUCE's dsp::NoiseGate (2ms attack / 120ms release, ratio 4:1) as configured in
// Source/PluginProcessor.cpp prepareToPlay(), built on the same two-stage ballistics
// (RMS follower -> envelope follower -> VCA) JUCE's BallisticsFilter uses internally.
// thresholdDb = cleanEff>0.001 ? -72+38*cleanEff : -120 (computed on the main thread).
const RATIO = 4;

function timeCoef(sampleRate, timeMs) {
  if (timeMs <= 0) return 0;
  return Math.exp((-2 * Math.PI * 1000) / (sampleRate * timeMs));
}

class GateProcessor extends AudioWorkletProcessor {
  static get parameterDescriptors() {
    return [{ name: "thresholdDb", defaultValue: -72, minValue: -120, maxValue: 0, automationRate: "a-rate" }];
  }

  constructor() {
    super();
    this.yRMS = [0, 0];
    this.yEnv = [0, 0];
    this.cteRmsRelease = timeCoef(sampleRate, 50);
    this.cteEnvAttack = timeCoef(sampleRate, 2);
    this.cteEnvRelease = timeCoef(sampleRate, 120);
  }

  process(inputs, outputs, parameters) {
    const input = inputs[0];
    const output = outputs[0];
    if (!input || input.length === 0) return true;
    const thresholdParam = parameters.thresholdDb;

    for (let ch = 0; ch < input.length; ch++) {
      const inCh = input[ch];
      const outCh = output[ch];
      if (!inCh || !outCh) continue;

      let yRMS = this.yRMS[ch] || 0;
      let yEnv = this.yEnv[ch] || 0;

      for (let i = 0; i < inCh.length; i++) {
        const x = inCh[i];
        const sq = x * x;
        const cteRMS = sq > yRMS ? 0 : this.cteRmsRelease;
        yRMS = sq + cteRMS * (yRMS - sq);
        const env = Math.sqrt(Math.max(0, yRMS));

        const cteEnv = env > yEnv ? this.cteEnvAttack : this.cteEnvRelease;
        yEnv = env + cteEnv * (yEnv - env);

        const thresholdDb = thresholdParam.length > 1 ? thresholdParam[i] : thresholdParam[0];
        const threshold = Math.pow(10, thresholdDb / 20);
        const gain = yEnv > threshold ? 1 : Math.pow(Math.max(yEnv, 1e-9) / threshold, RATIO - 1);

        outCh[i] = gain * x;
      }

      this.yRMS[ch] = yRMS;
      this.yEnv[ch] = yEnv;
    }
    return true;
  }
}

registerProcessor("tt-gate", GateProcessor);
