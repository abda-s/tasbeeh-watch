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
  const timeInput = document.getElementById("time-input");
  const reminderList = document.getElementById("reminder-list");
  const logPanel = document.getElementById("log-panel");
  const motorDot = document.getElementById("motor-dot");
  const motorLabel = document.getElementById("motor-label");
  const sleepOverlay = document.getElementById("sleep-overlay");
  const sleepOverlayText = document.getElementById("sleep-overlay-text");
  const vibeRings = document.getElementById("vibe-rings");
  const tapHint = document.getElementById("tap-hint");

  let Module = null;
  let lastFrameSeq = -1;
  let lastLogSeq = 0;
  let lastMotorLevel = false;
  let reminderRows = {};   // idx -> {row, dot, timeEl, btn}

  // One expanding ring per motor ON-edge -- mirrors the real backlight-off
  // behavior (driven by state.bl, not guessed) the sleep overlay uses:
  // this is driven by state.motor.level, not by clicking a button, so it
  // fires equally whether a reminder triggers it or the test button does.
  function spawnVibeRing() {
    const ring = document.createElement("div");
    ring.className = "vibe-ring";
    ring.addEventListener("animationend", () => ring.remove());
    setTimeout(() => ring.remove(), 1200);   // fallback if animationend never fires (reduced-motion edge cases)
    vibeRings.appendChild(ring);
  }

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

    const toggle = document.createElement("input");
    toggle.type = "checkbox"; toggle.className = "rem-toggle";
    toggle.setAttribute("aria-label", "Enabled");

    const label = document.createElement("span"); label.className = "label";

    const timeEl = document.createElement("input");
    timeEl.type = "time"; timeEl.step = 60; timeEl.className = "rem-time";

    const btn = document.createElement("button"); btn.textContent = "Fire";
    btn.onclick = () => Module.ccall("sim_web_fire", null, ["number"], [r.i]);

    // Both the toggle and the time field push through the same
    // sim_web_set_reminder(idx, h, m, enabled) -- each just sends its own
    // change plus whatever the other control's current value already is.
    function pushEdit() {
      const [h, m] = timeEl.value ? timeEl.value.split(":").map(Number) : [r.h, r.m];
      Module.ccall("sim_web_set_reminder", null, ["number", "number", "number", "number"],
        [r.i, h, m, toggle.checked ? 1 : 0]);
    }
    toggle.addEventListener("change", pushEdit);
    timeEl.addEventListener("change", pushEdit);

    row.append(toggle, label, timeEl, btn);
    reminderList.appendChild(row);
    const entry = { row, toggle, label, timeEl, btn };
    reminderRows[r.i] = entry;
    return entry;
  }

  function poll() {
    const jsonPtr = Module.ccall("sim_web_state_json", "number", ["number", "number"], [0, lastLogSeq]);
    const state = JSON.parse(Module.UTF8ToString(jsonPtr));

    // mode is "awake" / "light" / "deep" -- awake keeps the chip's default
    // look, the two sleep states get their own color (see poll()'s sleep
    // overlay update below, which uses the exact same mode value).
    const modeClass = state.mode === "light" ? "mode-light" : state.mode === "deep" ? "mode-deep" : "";
    statusEl.innerHTML =
      `<span><b>${state.time}</b></span>` +
      `<span class="${modeClass}">${state.mode}</span>` +
      `<span>batt <b>${state.battery}%</b></span>` +
      `<span>${state.screen}</span>`;

    battInput.value = state.battery;
    battInput.style.setProperty("--pct", state.battery + "%");
    battOutput.textContent = state.battery + "%";

    if (document.activeElement !== timeInput) timeInput.value = state.time.slice(0, 5);

    for (const r of state.reminders) {
      const entry = ensureReminderRow(r);
      entry.row.classList.toggle("on", !!r.on);
      entry.label.textContent = r.label;
      entry.toggle.checked = !!r.on;
      // Don't stomp on a field the user is actively editing -- poll() runs
      // every 200ms and would otherwise reset it mid-interaction.
      if (document.activeElement !== entry.timeEl) {
        entry.timeEl.value = `${String(r.h).padStart(2, "0")}:${String(r.m).padStart(2, "0")}`;
      }
    }

    motorDot.classList.toggle("buzz", !!state.motor.level);
    motorLabel.textContent = state.motor.pin >= 0
      ? `GPIO${state.motor.pin} · ${state.motor.level ? "ON" : "off"} · ${state.motor.seq} edges`
      : "not yet toggled";
    if (state.motor.level && !lastMotorLevel) spawnVibeRing();   // rising edge only, one ring per pulse
    lastMotorLevel = !!state.motor.level;

    // Backlight off = the panel shows nothing on real hardware, regardless
    // of what's still sitting in GRAM -- without this the canvas just
    // freezes on the last frame during sleep and looks like it hung.
    // Also require mode != awake, not just bl === 0: the backlight briefly
    // passes through 0 during boot's own fade-in (gamma-corrected, not
    // instant), which would otherwise flash this overlay for real while
    // the firmware was awake the entire time.
    // Light and deep sleep are different states in the real firmware (deep
    // is only entered under <5% battery lockdown -- try the slider), so
    // the overlay says which one it actually is instead of one generic
    // "asleep".
    const isDeep = state.mode === "deep";
    sleepOverlay.classList.toggle("on", state.bl === 0 && state.mode !== "awake");
    sleepOverlay.classList.toggle("deep", isDeep);
    sleepOverlayText.textContent = isDeep ? "screen off · deep sleep" : "screen off · light sleep";

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
  // Plays a couple of pulses right after boot to signal "this responds to
  // touch" without relying on someone reading the hint text first, then
  // gets out of the way for good -- on its own timeout, or immediately the
  // moment there's a real touch to react to.
  let tapHintDismissed = false;
  function dismissTapHint() {
    if (tapHintDismissed) return;
    tapHintDismissed = true;
    tapHint.classList.remove("show");
  }

  let dragging = false;
  canvas.addEventListener("pointerdown", (e) => {
    dismissTapHint();
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
    battInput.style.setProperty("--pct", battInput.value + "%");
    battOutput.textContent = battInput.value + "%";
    Module.ccall("sim_web_set_battery", null, ["number"], [parseInt(battInput.value, 10)]);
  });
  document.getElementById("btn-set-time").onclick = () => {
    if (!timeInput.value) return;
    const [h, m] = timeInput.value.split(":").map(Number);
    Module.ccall("sim_web_set_time", null, ["number", "number"], [h, m]);
  };
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
    // Give the boot screens a moment to settle before drawing attention to
    // the watch face, then let it play out (3 pulses x 1.4s) and self-dismiss.
    setTimeout(() => tapHint.classList.add("show"), 700);
    setTimeout(dismissTapHint, 700 + 3 * 1400);
  }).catch((e) => {
    loadingEl.textContent = "Failed to start: " + (e && e.message ? e.message : e);
    console.error(e);
  });
})();
