// Boots the real firmware (compiled to WebAssembly — see build.sh) and
// wires the page to it. No HTTP polling: this talks to the wasm module
// directly through the exported sim_web_* functions in wasm_bridge.cpp,
// which share their underlying logic with the native simulator's HTTP
// server (sim/sim_state.cpp) — neither is a re-implementation of the other.
(function () {
  const canvas = document.getElementById("watch-canvas");
  const ctx = canvas.getContext("2d", { alpha: false });
  const img = ctx.createImageData(240, 240);
  const loadingEl = document.getElementById("loading");
  const statusEl = document.getElementById("watch-status");
  const battInput = document.getElementById("battery-input");
  const battOutput = document.getElementById("battery-output");
  const reminderList = document.getElementById("reminder-list");
  const logPanel = document.getElementById("log-panel");
  const motorDot = document.getElementById("motor-dot");
  const motorLabel = document.getElementById("motor-label");

  let Module = null;
  let lastFrameSeq = -1;
  let lastLogSeq = 0;
  let reminderRows = {};   // idx -> {row, dot, timeEl, btn}

  function blit() {
    const framePtr = Module.ccall("sim_web_frame_ptr", "number", [], []);
    const seq = Module.ccall("sim_web_frame_seq", "number", [], []);
    if (seq === lastFrameSeq) return;
    lastFrameSeq = seq;
    const u16 = new Uint16Array(Module.HEAPU16.buffer, framePtr, 240 * 240);
    const d = img.data;
    for (let i = 0; i < 240 * 240; i++) {
      const v = u16[i];
      const r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
      const o = i * 4;
      d[o] = (r << 3) | (r >> 2);
      d[o + 1] = (g << 2) | (g >> 4);
      d[o + 2] = (b << 3) | (b >> 2);
      d[o + 3] = 255;
    }
    ctx.putImageData(img, 0, 0);
  }

  function ensureReminderRow(r) {
    if (reminderRows[r.i]) return reminderRows[r.i];
    const row = document.createElement("div");
    row.className = "reminder-row";
    const dot = document.createElement("span"); dot.className = "dot";
    const label = document.createElement("span"); label.className = "label";
    const timeEl = document.createElement("span"); timeEl.className = "time";
    const btn = document.createElement("button"); btn.textContent = "Fire";
    btn.onclick = () => Module.ccall("sim_web_fire", null, ["number"], [r.i]);
    row.append(dot, label, timeEl, btn);
    reminderList.appendChild(row);
    const entry = { row, dot, label, timeEl, btn };
    reminderRows[r.i] = entry;
    return entry;
  }

  function poll() {
    const jsonPtr = Module.ccall("sim_web_state_json", "number", ["number", "number"], [0, lastLogSeq]);
    const state = JSON.parse(Module.UTF8ToString(jsonPtr));

    statusEl.innerHTML =
      `<span><b>${state.time}</b></span>` +
      `<span>${state.mode}</span>` +
      `<span>batt <b>${state.battery}%</b></span>` +
      `<span>${state.screen}</span>`;

    battInput.value = state.battery;
    battOutput.textContent = state.battery + "%";

    for (const r of state.reminders) {
      const entry = ensureReminderRow(r);
      entry.row.classList.toggle("on", !!r.on);
      entry.label.textContent = r.label;
      entry.timeEl.textContent = `${String(r.h).padStart(2, "0")}:${String(r.m).padStart(2, "0")}`;
    }

    motorDot.classList.toggle("buzz", !!state.motor.level);
    motorLabel.textContent = state.motor.pin >= 0
      ? `GPIO${state.motor.pin} · ${state.motor.level ? "ON" : "off"} · ${state.motor.seq} edges`
      : "not yet toggled";

    if (state.log && state.log.lines.length) {
      const frag = document.createDocumentFragment();
      for (const line of state.log.lines) {
        if (!line) continue;
        const div = document.createElement("div");
        div.textContent = line;
        frag.appendChild(div);
      }
      logPanel.appendChild(frag);
      lastLogSeq = state.log.seq;
      while (logPanel.childNodes.length > 300) logPanel.removeChild(logPanel.firstChild);
      logPanel.scrollTop = logPanel.scrollHeight;
    }
  }

  function loop() {
    blit();
    requestAnimationFrame(loop);
  }

  // ── touch / mouse -> sim_web_touch (drag gestures = swipe navigation) ──
  function canvasPoint(e) {
    const r = canvas.getBoundingClientRect();
    const x = Math.max(0, Math.min(239, Math.round((e.clientX - r.left) / r.width * 240)));
    const y = Math.max(0, Math.min(239, Math.round((e.clientY - r.top) / r.height * 240)));
    return [x, y];
  }
  let dragging = false;
  canvas.addEventListener("pointerdown", (e) => {
    dragging = true;
    canvas.setPointerCapture(e.pointerId);
    const [x, y] = canvasPoint(e);
    Module.ccall("sim_web_touch", null, ["number", "number", "number"], [x, y, 1]);
  });
  canvas.addEventListener("pointermove", (e) => {
    if (!dragging) return;
    const [x, y] = canvasPoint(e);
    Module.ccall("sim_web_touch", null, ["number", "number", "number"], [x, y, 1]);
  });
  function release(e) {
    if (!dragging) return;
    dragging = false;
    const [x, y] = canvasPoint(e);
    Module.ccall("sim_web_touch", null, ["number", "number", "number"], [x, y, 0]);
  }
  canvas.addEventListener("pointerup", release);
  canvas.addEventListener("pointercancel", release);
  canvas.addEventListener("contextmenu", (e) => e.preventDefault());

  // ── control panel wiring ────────────────────────────────────────────
  battInput.addEventListener("input", () => {
    battOutput.textContent = battInput.value + "%";
    Module.ccall("sim_web_set_battery", null, ["number"], [parseInt(battInput.value, 10)]);
  });
  document.getElementById("btn-buzz").onclick = () => Module.ccall("sim_web_buzz", null, [], []);
  document.getElementById("btn-reboot").onclick = () => {
    logNote("power-cycling (RTC memory cleared)…");
    Module.ccall("sim_web_reboot", null, [], []);
  };
  document.getElementById("btn-factory").onclick = () => {
    if (!confirm("Factory reset: clears all saved settings and reminders. Continue?")) return;
    logNote("factory reset (NVS + RTC cleared)…");
    Module.ccall("sim_web_factory", null, [], []);
  };
  function logNote(text) {
    const div = document.createElement("div");
    div.className = "fresh";
    div.textContent = "→ " + text;
    logPanel.appendChild(div);
    logPanel.scrollTop = logPanel.scrollHeight;
  }

  // ── boot ─────────────────────────────────────────────────────────────
  WatchModule().then((mod) => {
    Module = mod;
    loadingEl.classList.add("hidden");
    setTimeout(() => loadingEl.remove(), 400);
    requestAnimationFrame(loop);
    poll();
    setInterval(poll, 200);
  }).catch((e) => {
    loadingEl.textContent = "Failed to start: " + (e && e.message ? e.message : e);
    console.error(e);
  });
})();
