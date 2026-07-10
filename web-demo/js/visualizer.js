// Ports WaveDisplay::paint / PolishBar::paint from Source/PluginEditor.cpp:
// - 64-bar waveform history, height = clamp(5,maxH, 5 + level^0.65*(maxH-5))
// - peak-hold decay: smoothLevel = max(peak, smoothLevel*0.80), driven at 30fps (not native 60fps rAF)
// - polish meter shimmer: anim = 0.22 + 0.78*drive*(0.72+0.28*sin(phase*2+i*1.3))
const LIME = "#c6f542";
const LIME_DIM = "#6f7a36";
const BAR_GREY = "#56565f";
const HIST_LEN = 64;
const TICK_MS = 1000 / 30;

function roundRect(ctx, x, y, w, h, r) {
  const rr = Math.min(r, w / 2, h / 2);
  ctx.beginPath();
  ctx.moveTo(x + rr, y);
  ctx.arcTo(x + w, y, x + w, y + h, rr);
  ctx.arcTo(x + w, y + h, x, y + h, rr);
  ctx.arcTo(x, y + h, x, y, rr);
  ctx.arcTo(x, y, x + w, y, rr);
  ctx.closePath();
}

function fitCanvas(canvas) {
  const dpr = window.devicePixelRatio || 1;
  const rect = canvas.getBoundingClientRect();
  const w = Math.max(1, Math.round(rect.width * dpr));
  const h = Math.max(1, Math.round(rect.height * dpr));
  if (canvas.width !== w || canvas.height !== h) {
    canvas.width = w;
    canvas.height = h;
  }
}

export function createVisualizer({ waveCanvas, polishMeterCanvas, analyser = null }) {
  const waveCtx = waveCanvas.getContext("2d");
  const meterCtx = polishMeterCanvas.getContext("2d");
  const hist = new Array(HIST_LEN).fill(0);
  let currentAnalyser = analyser;
  let timeBuf = analyser ? new Float32Array(analyser.fftSize) : null;

  let smoothLevel = 0;
  let phase = 0;
  let processing = true;
  let lastTick = 0;

  function resize() {
    fitCanvas(waveCanvas);
    fitCanvas(polishMeterCanvas);
  }
  window.addEventListener("resize", resize);
  resize();

  function drawWave() {
    const w = waveCanvas.width;
    const h = waveCanvas.height;
    waveCtx.clearRect(0, 0, w, h);
    const pitch = w / HIST_LEN;
    const barW = pitch * 0.58;
    const cy = h / 2;
    const maxH = h;
    waveCtx.fillStyle = processing ? LIME : BAR_GREY;
    for (let i = 0; i < HIST_LEN; i++) {
      const v = hist[i];
      const hgt = Math.min(maxH, Math.max(5, 5 + Math.pow(v, 0.65) * (maxH - 5)));
      const x = i * pitch + (pitch - barW) / 2;
      const y = cy - hgt / 2;
      roundRect(waveCtx, x, y, barW, hgt, Math.min(barW / 2, 3.5));
      waveCtx.fill();
    }
  }

  function drawPolishMeter() {
    const w = polishMeterCanvas.width;
    const h = polishMeterCanvas.height;
    meterCtx.clearRect(0, 0, w, h);
    const baseH = [0.4, 0.62, 0.82, 1.0, 0.7, 0.48];
    const n = baseH.length;
    const bw = Math.max(4, (w / n) * 0.6);
    const gap = (w - n * bw) / (n - 1);
    const maxH = h * 0.9;
    const baseline = h;
    const drive = Math.min(1, Math.max(0, smoothLevel * 2.2));
    meterCtx.fillStyle = processing ? LIME : LIME_DIM;
    for (let i = 0; i < n; i++) {
      // resting height (0.5) keeps a clean graduated meter shape; audio adds up to the full bar height
      const anim = 0.5 + 0.5 * drive * (0.72 + 0.28 * Math.sin(phase * 2 + i * 1.3));
      const hgt = Math.max(4, maxH * baseH[i] * anim);
      roundRect(meterCtx, i * (bw + gap), baseline - hgt, bw, hgt, Math.min(bw / 2, 2.5));
      meterCtx.fill();
    }
  }

  function tick(now) {
    if (now - lastTick < TICK_MS) return false;
    lastTick = now;

    // Re-fit each frame: module scripts can run before the stylesheet applies, so the
    // constructor may have measured a zero-size canvas. fitCanvas is a no-op once stable.
    resize();

    let peak = 0;
    if (currentAnalyser) {
      currentAnalyser.getFloatTimeDomainData(timeBuf);
      for (let i = 0; i < timeBuf.length; i++) {
        const a = Math.abs(timeBuf[i]);
        if (a > peak) peak = a;
      }
    }
    smoothLevel = Math.max(peak, smoothLevel * 0.8);

    hist.shift();
    hist.push(Math.min(1, Math.max(0, smoothLevel)));

    phase += 0.25;

    drawWave();
    drawPolishMeter();
    return true;
  }

  return {
    tick,
    setProcessing(p) {
      processing = p;
    },
    setAnalyser(a) {
      currentAnalyser = a;
      timeBuf = new Float32Array(a.fftSize);
    },
    getSmoothLevel: () => smoothLevel,
  };
}
