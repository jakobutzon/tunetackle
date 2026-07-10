// First-run guided tour: press play -> press After -> try the knobs. Shown once per
// browser (localStorage), auto-advances on the actual action rather than a "Next" button.
const STORAGE_KEY = "tt-onboarded";

export function createOnboarding({ stage, targets }) {
  if (localStorage.getItem(STORAGE_KEY)) {
    return { notify() {} };
  }

  let step = 0;
  let highlighted = null;

  const tip = document.createElement("div");
  tip.className = "coach-tip";
  tip.hidden = true;
  stage.appendChild(tip);

  function clearHighlight() {
    if (highlighted) highlighted.classList.remove("coach-highlight");
    highlighted = null;
  }

  function position(target, placement) {
    const stageRect = stage.getBoundingClientRect();
    const r = target.getBoundingClientRect();
    const tipRect = tip.getBoundingClientRect();
    const top = placement === "above" ? r.top - stageRect.top - tipRect.height - 16 : r.bottom - stageRect.top + 16;
    let left = r.left - stageRect.left + r.width / 2 - tipRect.width / 2;
    left = Math.max(8, Math.min(left, stageRect.width - tipRect.width - 8));
    tip.style.top = `${top}px`;
    tip.style.left = `${left}px`;
  }

  function show(target, text, placement) {
    clearHighlight();
    tip.textContent = text;
    tip.className = `coach-tip ${placement}`;
    tip.hidden = false;
    target.classList.add("coach-highlight");
    highlighted = target;
    requestAnimationFrame(() => position(target, placement));
  }

  function finish() {
    clearHighlight();
    tip.remove();
    window.removeEventListener("resize", reposition);
    localStorage.setItem(STORAGE_KEY, "1");
    step = -1;
  }

  function reposition() {
    if (highlighted) position(highlighted, tip.classList.contains("above") ? "above" : "below");
  }
  window.addEventListener("resize", reposition);

  function notify(event) {
    if (step === 0 && event === "vocal-played") {
      step = 1;
      show(targets.after, "Now press After to hear TuneTackle", "below");
    } else if (step === 1 && event === "after-pressed") {
      step = 2;
      show(targets.knobs, "Try the knobs, presets, and Polish — that's it!", "above");
      setTimeout(() => {
        if (step === 2) finish();
      }, 6000);
    } else if (step === 2 && event === "knob-touched") {
      finish();
    }
  }

  show(targets.vocal, "Press play to hear your raw vocal", "below");
  return { notify };
}
