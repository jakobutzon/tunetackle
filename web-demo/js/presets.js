// Verbatim port of Source/PluginProcessor.cpp getPresets() + the Polish eff() scaling formula.
export const PRESETS = [
  { name: "Radio Ready",    tune:  0, clean: 80, eq: 80, compress: 80, air: 80, space: 50, polish: 100 },
  { name: "Natural",        tune: 12, clean: 40, eq: 15, compress: 20, air: 10, space: 18, polish: 55 },
  { name: "Pop Sheen",      tune: 45, clean: 60, eq: 40, compress: 55, air: 65, space: 45, polish: 80 },
  { name: "Trap Hard Tune", tune: 95, clean: 65, eq: 45, compress: 60, air: 50, space: 30, polish: 85 },
  { name: "R&B Smooth",     tune: 60, clean: 55, eq: 30, compress: 40, air: 45, space: 55, polish: 75 },
  { name: "Rock Grit",      tune: 20, clean: 45, eq: 50, compress: 65, air: 25, space: 35, polish: 70 },
  { name: "Podcast Voice",  tune:  0, clean: 70, eq: 45, compress: 60, air: 20, space:  5, polish: 65 },
  { name: "Intimate Booth", tune: 15, clean: 50, eq: 20, compress: 30, air: 35, space: 70, polish: 60 },
];

export const DEFAULT_PRESET_INDEX = 0;

export const KNOB_IDS = ["tune", "clean", "eq", "compress", "air", "space"];

// mul = clamp(polish/100/0.72, 0, 1.25); eff(v) = clamp(v/100*mul, 0, 1)
export function polishMul(polish0to100) {
  const polish = polish0to100 / 100;
  return Math.min(1.25, Math.max(0, polish / 0.72));
}

export function eff(value0to100, mul) {
  return Math.min(1, Math.max(0, (value0to100 / 100) * mul));
}

// Computes all six effective (0..1) amounts from raw knob values (0..100) + polish (0..100).
export function effectiveAmounts(knobs, polish) {
  const mul = polishMul(polish);
  const out = {};
  for (const id of KNOB_IDS) out[id] = eff(knobs[id], mul);
  return out;
}
