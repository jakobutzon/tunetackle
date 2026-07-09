// Canvas rotary knob matching Source/PluginEditor.cpp's drawRotarySlider geometry:
// 225deg -> 495deg sweep (270deg total), JUCE "clock" angle convention (0 = 12 o'clock, clockwise).
const START_DEG = 225;
const END_DEG = 495;
const LIME = "#c6f542";
const LIME_DIM = "#6f7a36";
const TRACK = "#26262d";
const POINTER_DIM = "#9a9aa2";

function clockPoint(cx, cy, r, clockAngleRad) {
  return [cx + r * Math.sin(clockAngleRad), cy - r * Math.cos(clockAngleRad)];
}

function toCanvasAngle(clockAngleRad) {
  return clockAngleRad - Math.PI / 2;
}

export function createKnob(canvas, { min = 0, max = 100, value = 0, processing = true, onChange } = {}) {
  const ctx = canvas.getContext("2d");
  const state = { min, max, value, processing };
  let dragging = false;
  let startClientY = 0;
  let startValue = 0;

  const W = canvas.width;
  const H = canvas.height;
  const cx = W / 2;
  const cy = H / 2;
  const radius = Math.min(W, H) / 2 - 6;

  function sliderPos() {
    return (state.value - state.min) / (state.max - state.min);
  }

  function draw() {
    ctx.clearRect(0, 0, W, H);
    const pos = sliderPos();
    const startRad = (START_DEG * Math.PI) / 180;
    const endRad = (END_DEG * Math.PI) / 180;
    const angle = startRad + pos * (endRad - startRad);
    const accent = state.processing ? LIME : LIME_DIM;

    // track arc
    ctx.beginPath();
    ctx.arc(cx, cy, radius - 2.5, toCanvasAngle(startRad), toCanvasAngle(endRad), false);
    ctx.strokeStyle = TRACK;
    ctx.lineWidth = 4.5 * (W / 84);
    ctx.lineCap = "round";
    ctx.stroke();

    // value arc
    if (pos > 0.003) {
      ctx.beginPath();
      ctx.arc(cx, cy, radius - 2.5, toCanvasAngle(startRad), toCanvasAngle(angle), false);
      ctx.strokeStyle = accent;
      ctx.lineWidth = 4.5 * (W / 84);
      ctx.lineCap = "round";
      ctx.stroke();
    }

    // knob body
    const bodyR = radius - 8;
    const grad = ctx.createRadialGradient(cx - bodyR * 0.3, cy - bodyR * 0.3, 1, cx, cy, bodyR);
    grad.addColorStop(0, "#2c2c34");
    grad.addColorStop(1, "#121216");
    ctx.beginPath();
    ctx.arc(cx, cy, bodyR, 0, Math.PI * 2);
    ctx.fillStyle = grad;
    ctx.fill();
    ctx.strokeStyle = "#35353f";
    ctx.lineWidth = 1.2 * (W / 84);
    ctx.stroke();

    // pointer
    const [x0, y0] = clockPoint(cx, cy, 0.35 * bodyR, angle);
    const [x1, y1] = clockPoint(cx, cy, 0.8 * bodyR, angle);
    ctx.beginPath();
    ctx.moveTo(x0, y0);
    ctx.lineTo(x1, y1);
    ctx.strokeStyle = state.processing ? LIME : POINTER_DIM;
    ctx.lineWidth = 3.4 * (W / 84);
    ctx.lineCap = "round";
    ctx.stroke();
  }

  function setValue(v, fire = true) {
    state.value = Math.min(state.max, Math.max(state.min, v));
    draw();
    if (fire && onChange) onChange(state.value);
  }

  function pointerDown(e) {
    dragging = true;
    startClientY = e.clientY;
    startValue = state.value;
    canvas.setPointerCapture(e.pointerId);
    e.preventDefault();
  }
  function pointerMove(e) {
    if (!dragging) return;
    const delta = startClientY - e.clientY;
    const range = state.max - state.min;
    setValue(startValue + delta * (range / 180));
  }
  function pointerUp() {
    dragging = false;
  }

  canvas.addEventListener("pointerdown", pointerDown);
  canvas.addEventListener("pointermove", pointerMove);
  canvas.addEventListener("pointerup", pointerUp);
  canvas.addEventListener("pointercancel", pointerUp);

  draw();

  return {
    setValue,
    getValue: () => state.value,
    setProcessing(p) {
      state.processing = p;
      draw();
    },
  };
}
