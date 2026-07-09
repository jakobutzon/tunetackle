// Ports Source/DSP.h's PitchDetector (autocorrelation) + PitchShifter (dual-tap delay-line)
// and the Tune section of Source/PluginProcessor.cpp::processBlock, verbatim.
//
// Two inputs by design (matches the real plugin, where detector.push() runs on the RAW
// pre-Clean signal while the shifter processes the POST-Clean signal):
//   input 0 = post-Clean/gate signal -> what actually gets pitch-shifted and output
//   input 1 = raw pre-Clean signal   -> detection only, mono-summed like PitchDetector::push
const RING_SIZE = 8192;
const HOP = 1024;
const WIN = 1024;

function nextPow2(n) {
  let p = 1;
  while (p < n) p *= 2;
  return p;
}

function readInterp(buf, bufSize, mask, writePos, delay) {
  const fpos = writePos - delay - 2.0;
  const ip = Math.floor(fpos);
  const fr = fpos - ip;
  const a = buf[(ip + bufSize) & mask];
  const b = buf[(ip + 1 + bufSize) & mask];
  return a + fr * (b - a);
}

class TuneProcessor extends AudioWorkletProcessor {
  static get parameterDescriptors() {
    return [{ name: "tuneEff", defaultValue: 0, minValue: 0, maxValue: 1, automationRate: "a-rate" }];
  }

  constructor() {
    super();
    this.sr = sampleRate;

    // --- detector state ---
    this.ring = new Float32Array(RING_SIZE);
    this.ringPos = 0;
    this.sinceHop = 0;
    this.rvals = new Float32Array(WIN);
    this.detFreq = 0;
    this.detConf = 0;
    this._x = new Float32Array(WIN);

    // --- shifter state (per channel, up to 2) ---
    this.windowSamples = 0.03 * this.sr;
    this.bufSize = nextPow2(Math.max(4096, Math.floor(this.sr / 4)));
    this.mask = this.bufSize - 1;
    this.shiftBuf = [new Float32Array(this.bufSize), new Float32Array(this.bufSize)];
    this.writePos = [0, 0];
    this.phase = [0, 0];

    // --- shared smoothing state (matches ratioNow/mixNow being single scalars in PluginProcessor) ---
    this.targetRatio = 1.0;
    this.ratioNow = 1.0;
    this.mixNow = 0.0;

    this._ratioBuf = null;
    this._mixBuf = null;
  }

  analyse() {
    const win = WIN;
    let start = this.ringPos - win * 2;
    while (start < 0) start += RING_SIZE;
    const x = this._x;
    for (let i = 0; i < win; i++) {
      x[i] = this.ring[(start + i * 2) % RING_SIZE];
    }

    let e0 = 0;
    for (let i = 0; i < win; i++) e0 += x[i] * x[i];
    const rms = Math.sqrt(e0 / win);
    if (rms < 3.0e-4) {
      this.detConf = 0;
      return;
    }

    const sr2 = this.sr * 0.5;
    const minLag = Math.max(2, Math.floor(sr2 / 950.0));
    const maxLag = Math.min(win - 32, Math.floor(sr2 / 65.0));

    let bestLag = 0;
    let bestVal = 0;
    for (let lag = minLag; lag <= maxLag; lag++) {
      let num = 0;
      const len = win - lag;
      for (let i = 0; i < len; i++) num += x[i] * x[i + lag];
      const r = (num / (e0 + 1e-12)) * (win / len);
      this.rvals[lag] = r;
      if (r > bestVal) {
        bestVal = r;
        bestLag = lag;
      }
    }

    if (bestLag === 0 || bestVal < 0.4) {
      this.detConf = bestVal;
      return;
    }

    for (let div = 4; div >= 2; div--) {
      const l2 = Math.round(bestLag / div);
      if (l2 >= minLag && this.rvals[l2] > 0.88 * bestVal) {
        bestLag = l2;
        bestVal = this.rvals[l2];
        break;
      }
    }

    let refined = bestLag;
    if (bestLag > minLag && bestLag < maxLag) {
      const rm = this.rvals[bestLag - 1];
      const r0 = this.rvals[bestLag];
      const rp = this.rvals[bestLag + 1];
      const denom = rm - 2 * r0 + rp;
      if (Math.abs(denom) > 1e-9) refined += (0.5 * (rm - rp)) / denom;
    }

    this.detConf = bestVal;
    this.detFreq = sr2 / refined;
  }

  shiftChannel(ch, inCh, outCh, n, ratioBuf, mixBuf) {
    const buf = this.shiftBuf[ch];
    const bufSize = this.bufSize;
    const mask = this.mask;
    const w = this.windowSamples;
    let writePos = this.writePos[ch];
    let phase = this.phase[ch];

    for (let i = 0; i < n; i++) {
      buf[writePos] = inCh[i];

      phase += 1.0 - ratioBuf[i];
      while (phase >= w) phase -= w;
      while (phase < 0) phase += w;

      let dA = phase;
      let dB = dA + w * 0.5;
      if (dB >= w) dB -= w;

      const gA = Math.sin((Math.PI * dA) / w);
      const gB = Math.sin((Math.PI * dB) / w);

      const tapA = readInterp(buf, bufSize, mask, writePos, dA);
      const tapB = readInterp(buf, bufSize, mask, writePos, dB);
      const wet = gA * tapA + gB * tapB;

      outCh[i] = inCh[i] * (1 - mixBuf[i]) + wet * mixBuf[i];

      writePos = (writePos + 1) & mask;
    }
    this.writePos[ch] = writePos;
    this.phase[ch] = phase;
  }

  // A processing exception silently and permanently kills an AudioWorkletNode's output with
  // no useful error surfaced to the page; guard against edge-case audio content (e.g. NaNs)
  // taking down the whole chain, and report the first failure back for diagnostics.
  process(inputs, outputs, parameters) {
    try {
      return this._process(inputs, outputs, parameters);
    } catch (err) {
      if (!this._errPosted) {
        this._errPosted = true;
        this.port.postMessage({ error: String((err && err.stack) || err) });
      }
      return true;
    }
  }

  _process(inputs, outputs, parameters) {
    const post = inputs[0];
    const raw = inputs[1] && inputs[1].length ? inputs[1] : post;
    const output = outputs[0];
    if (!post || post.length === 0 || !output || output.length === 0) return true;

    const n = post[0].length;
    const tuneEffArr = parameters.tuneEff;

    // 1) feed detector ring buffer from the raw (pre-Clean) signal, updating targetRatio on hop boundaries
    const nChRaw = raw.length;
    for (let i = 0; i < n; i++) {
      let s = 0;
      for (let ch = 0; ch < nChRaw; ch++) s += raw[ch][i];
      this.ring[this.ringPos] = s / Math.max(1, nChRaw);
      this.ringPos = (this.ringPos + 1) % RING_SIZE;

      if (++this.sinceHop >= HOP) {
        this.sinceHop = 0;
        this.analyse();
        this.port.postMessage({ freq: this.detFreq, conf: this.detConf });
        const tuneEff = tuneEffArr.length > 1 ? tuneEffArr[i] : tuneEffArr[0];
        if (this.detConf > 0.55 && this.detFreq > 70 && this.detFreq < 950) {
          const midi = 69 + 12 * Math.log2(this.detFreq / 440);
          const corrSemis = (Math.round(midi) - midi) * tuneEff;
          this.targetRatio = Math.min(1.12, Math.max(0.9, Math.pow(2, corrSemis / 12)));
        } else {
          this.targetRatio = 1.0;
        }
      }
    }

    // 2) per-sample ratio/mix smoothing, coefficients recomputed once per 128-sample quantum
    const tuneEffNow = tuneEffArr.length > 1 ? tuneEffArr[0] : tuneEffArr[0];
    const tau = Math.max(0.004, 0.1 - 0.096 * tuneEffNow);
    const rCoef = Math.exp(-1 / (tau * this.sr));
    const mCoef = Math.exp(-1 / (0.012 * this.sr));
    const mixTgt = tuneEffNow > 0.01 ? 1.0 : 0.0;

    if (!this._ratioBuf || this._ratioBuf.length < n) {
      this._ratioBuf = new Float32Array(n);
      this._mixBuf = new Float32Array(n);
    }
    for (let i = 0; i < n; i++) {
      this.ratioNow = this.targetRatio + (this.ratioNow - this.targetRatio) * rCoef;
      this.mixNow = mixTgt + (this.mixNow - mixTgt) * mCoef;
      this._ratioBuf[i] = this.ratioNow;
      this._mixBuf[i] = this.mixNow;
    }

    // 3) shift each channel of the post-Clean signal
    const numCh = Math.min(2, post.length);
    for (let ch = 0; ch < numCh; ch++) {
      this.shiftChannel(ch, post[ch], output[ch], n, this._ratioBuf, this._mixBuf);
    }
    for (let ch = numCh; ch < output.length; ch++) {
      output[ch].set(output[0]);
    }

    return true;
  }
}

registerProcessor("tt-tune", TuneProcessor);
