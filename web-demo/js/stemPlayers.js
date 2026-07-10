// <audio> playback: one MediaElementAudioSourceNode per element (created once, lazily, on
// first use), feeding the shared graph head. Generic over any number of keyed stems;
// starting one pauses any other that's playing.
export function createStemPlayers({ audioEls, buttons, graph, onAudioReady, onActiveChange }) {
  const sourceNodes = {};
  let active = null;
  let readyFired = false;

  function ensureSourceNode(key) {
    if (!sourceNodes[key]) {
      const ctx = graph.getContext();
      const node = ctx.createMediaElementSource(audioEls[key]);
      node.connect(graph.getHeadGain());
      sourceNodes[key] = node;
    }
  }

  function setPlayingUI(key, playing) {
    buttons[key].classList.toggle("playing", playing);
    buttons[key].setAttribute("aria-pressed", String(playing));
  }

  function pauseKey(key) {
    audioEls[key].pause();
    setPlayingUI(key, false);
  }

  async function toggle(key) {
    const el = audioEls[key];
    if (active === key && !el.paused) {
      pauseKey(key);
      active = null;
      if (onActiveChange) onActiveChange(null);
      return;
    }

    await graph.ensureStarted();
    if (!readyFired) {
      readyFired = true;
      if (onAudioReady) onAudioReady();
    }
    ensureSourceNode(key);

    if (active && active !== key) pauseKey(active);

    await el.play();
    active = key;
    setPlayingUI(key, true);
    if (onActiveChange) onActiveChange(key);
  }

  Object.keys(buttons).forEach((key) => {
    buttons[key].addEventListener("click", () => toggle(key));
  });

  return {
    getActive: () => active,
    getActiveElement: () => (active ? audioEls[active] : null),
  };
}
