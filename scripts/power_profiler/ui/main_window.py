"""Main window: plot on top, ESP32 log terminal below, stats docked right."""
import os
import re

import numpy as np
import pyqtgraph as pg
from PyQt5.QtCore import Qt, QTimer, pyqtSlot
from PyQt5.QtWidgets import (QAction, QCheckBox, QComboBox, QDialog,
                             QDockWidget, QDoubleSpinBox, QFileDialog,
                             QLabel, QMainWindow, QSplitter, QToolBar)

from analysis import (StateTracker, cycle_stats, detect_cycles, fmt_hours,
                      fmt_mah, fmt_mwh, parse_tag_line, remaining_mah,
                      save_tags_file, screen_on_stats, sleep_current,
                      usable_mah, windowed_mean)
from data_store import region_stats
from ui.log_terminal import LogTerminal
from ui.plot_widget import SMOOTH_PRESETS, PowerPlot
from ui.port_dialog import PortDialog
from ui.stats_panel import StatsPanel
from ui.tags_dialog import TagsDialog

REFRESH_MS = 33        # ~30 FPS GUI refresh
STATS_EVERY = 5        # update stats every N refreshes

_SPAN_RE = re.compile(r"^\s*(\d+(?:\.\d+)?)\s*([a-zA-Z]*)\s*$")
_UNIT_MULT = {"": 1, "s": 1, "sec": 1, "secs": 1, "second": 1, "seconds": 1,
              "m": 60, "min": 60, "mins": 60, "minute": 60, "minutes": 60,
              "h": 3600, "hr": 3600, "hrs": 3600, "hour": 3600, "hours": 3600}


def parse_span(text):
    """Parse a span string -> seconds (float), None for 'All', or 'invalid'."""
    s = text.strip().lower()
    if s == "all":
        return None
    m = _SPAN_RE.match(s)
    if not m:
        return "invalid"
    mult = _UNIT_MULT.get(m.group(2))
    val = float(m.group(1))
    if mult is None or val <= 0:
        return "invalid"
    return val * mult


class MainWindow(QMainWindow):
    def __init__(self, buffer, outdir, battery_mah=100.0, tags=None,
                 tags_path=None, parent=None):
        super().__init__(parent)
        self._buf = buffer
        self._outdir = outdir
        self.on_close = None  # callable set by the entry point
        self._tags = dict(tags or {})
        self._tags_path = tags_path
        self._tracker = StateTracker(self._tags)

        self._paused = False
        self._trigger_armed = False
        self._trigger_latched = True  # pass-through until user arms trigger
        self._trigger_level = 0.0

        # runtime ESP32 log connect/disconnect callbacks (set via
        # set_log_connectors by the entry point)
        self._log_connect_cb = None
        self._log_disconnect_cb = None
        self._log_is_connected = None
        self._log_exclude = []
        self._log_default_baud = 115200

        self.setWindowTitle("ESP32 Power Profiler")
        self.resize(1280, 800)

        # ---- central: plot over log terminal ----
        self._plot = PowerPlot()
        self._terminal = LogTerminal()
        split = QSplitter(Qt.Vertical)
        split.addWidget(self._plot)
        split.addWidget(self._terminal)
        split.setSizes([650, 150])
        self.setCentralWidget(split)

        # ---- right dock: statistics ----
        self._stats = StatsPanel(default_mah=battery_mah)
        self._stats.inputs_changed.connect(self._update_stats)
        dock = QDockWidget("Statistics")
        dock.setWidget(self._stats)
        dock.setFeatures(QDockWidget.DockWidgetMovable
                         | QDockWidget.DockWidgetFloatable)
        self.addDockWidget(Qt.RightDockWidgetArea, dock)

        self._build_toolbar()

        # ---- plot signal wiring (view overrides from mouse) ----
        self._plot.user_override_x.connect(
            lambda: self._set_checked_silently(self._act_follow, False))
        self._plot.user_override_y.connect(
            lambda: self._set_checked_silently(self._act_auto_y, False))
        self._plot.span_changed.connect(lambda _s: self._sync_span_text())

        # ---- status bar ----
        self._lbl_view = QLabel("")
        self._lbl_power = QLabel("INA226: --")
        self._lbl_log = QLabel("ESP32: --")
        self.statusBar().addPermanentWidget(self._lbl_view)
        self.statusBar().addPermanentWidget(self._lbl_power)
        self.statusBar().addPermanentWidget(self._lbl_log)

        # ---- refresh timer ----
        self._tick = 0
        self._timer = QTimer(self)
        self._timer.timeout.connect(self._refresh)
        self._timer.start(REFRESH_MS)

    # ---------------------------------------------------------- toolbar --
    def _build_toolbar(self):
        tb = QToolBar("Controls")
        tb.setMovable(False)
        self.addToolBar(tb)

        # ---- View group: window span + independent X/Y zoom ----
        tb.addWidget(QLabel(" Span "))
        self._cmb_span = QComboBox()
        self._cmb_span.setEditable(True)
        self._cmb_span.addItems(["2s", "5s", "10s", "30s", "60s",
                                 "5min", "All"])
        self._cmb_span.setCurrentText("10s")
        self._cmb_span.setToolTip(
            "Visible time window. Type a custom value "
            "(e.g. 7s, 12s, 1.5m, 5min, All) and press Enter.")
        self._cmb_span.activated[str].connect(self._on_span_text)
        self._cmb_span.lineEdit().returnPressed.connect(
            lambda: self._on_span_text(self._cmb_span.currentText()))
        tb.addWidget(self._cmb_span)

        act_x_out = QAction("X-", self)
        act_x_out.setToolTip("Zoom out the time axis (wider window)")
        act_x_out.triggered.connect(lambda: self._plot.zoom_x(2.0))
        tb.addAction(act_x_out)

        act_x_in = QAction("X+", self)
        act_x_in.setToolTip("Zoom in the time axis (narrower window)")
        act_x_in.triggered.connect(lambda: self._plot.zoom_x(0.5))
        tb.addAction(act_x_in)

        act_y_out = QAction("Y-", self)
        act_y_out.setToolTip("Zoom out the current axis (disables Auto Y)")
        act_y_out.triggered.connect(lambda: self._plot.zoom_y(1.4))
        tb.addAction(act_y_out)

        act_y_in = QAction("Y+", self)
        act_y_in.setToolTip("Zoom in the current axis (disables Auto Y)")
        act_y_in.triggered.connect(lambda: self._plot.zoom_y(1.0 / 1.4))
        tb.addAction(act_y_in)

        self._act_auto_y = QAction("Auto Y", self, checkable=True)
        self._act_auto_y.setChecked(True)
        self._act_auto_y.setToolTip(
            "Auto-fit Y to the visible window only (never the full history)")
        self._act_auto_y.toggled.connect(self._plot.set_auto_y)
        tb.addAction(self._act_auto_y)

        tb.addWidget(QLabel(" Smooth "))
        self._cmb_smooth = QComboBox()
        self._cmb_smooth.setEditable(False)
        self._cmb_smooth.addItems(list(SMOOTH_PRESETS.keys()))
        self._cmb_smooth.setCurrentText("Light")
        self._cmb_smooth.setToolTip(
            "Display smoothing (stats always use raw samples)")
        self._cmb_smooth.activated[str].connect(self._on_smooth)
        tb.addWidget(self._cmb_smooth)
        self._on_smooth("Light")

        tb.addSeparator()

        # ---- existing controls ----
        act_pause = QAction("Pause", self, checkable=True)
        act_pause.toggled.connect(self._on_pause)
        tb.addAction(act_pause)

        self._act_follow = QAction("Follow", self, checkable=True)
        self._act_follow.setChecked(True)
        self._act_follow.setToolTip(
            "Slide the window so its right edge tracks the newest sample")
        self._act_follow.toggled.connect(self._plot.set_follow)
        tb.addAction(self._act_follow)

        act_log = QAction("Log I", self, checkable=True)
        act_log.toggled.connect(self._plot.set_log_scale)
        tb.addAction(act_log)

        tb.addSeparator()

        self._chk_trigger = QCheckBox("Trigger >")
        self._chk_trigger.toggled.connect(self._on_trigger_toggled)
        tb.addWidget(self._chk_trigger)
        self._spin_trigger = QDoubleSpinBox()
        self._spin_trigger.setRange(0.0, 5000.0)
        self._spin_trigger.setDecimals(1)
        self._spin_trigger.setValue(50.0)
        self._spin_trigger.setSuffix(" mA")
        self._spin_trigger.valueChanged.connect(self._on_trigger_level)
        tb.addWidget(self._spin_trigger)

        tb.addSeparator()

        act_export = QAction("Export view CSV", self)
        act_export.triggered.connect(self._export_view)
        tb.addAction(act_export)

        act_clear = QAction("Clear markers", self)
        act_clear.triggered.connect(self._plot.clear_markers)
        tb.addAction(act_clear)

        act_tags = QAction("Tags...", self)
        act_tags.setToolTip("Edit marker tag -> role mapping")
        act_tags.triggered.connect(self._edit_tags)
        tb.addAction(act_tags)

        tb.addSeparator()

        self._act_esp_log = QAction("ESP log", self, checkable=True)
        self._act_esp_log.setToolTip(
            "Connect/disconnect the ESP32 debug log port at runtime")
        self._act_esp_log.toggled.connect(self._on_esp_log_toggled)
        tb.addAction(self._act_esp_log)

    # ------------------------------------------------------------ slots --
    @pyqtSlot(list)
    def on_power_batch(self, batch):
        """New power samples from the worker thread (queued connection)."""
        if self._trigger_armed and not self._trigger_latched:
            if max(b[2] for b in batch) >= self._trigger_level:
                self._trigger_latched = True
                self.statusBar().showMessage("Triggered - recording", 3000)
            else:
                return  # drop pre-trigger data from the display buffer
        self._buf.append(batch)

    @pyqtSlot(float, str)
    def on_log_line(self, t, text):
        self._terminal.add_line(t, text)
        tag, rest = parse_tag_line(text)
        if tag is not None:
            self._tracker.feed(t, tag, rest)
        self._plot.add_marker(t, text)

    @pyqtSlot(str)
    def set_power_status(self, msg):
        self._lbl_power.setText(msg)

    @pyqtSlot(str)
    def set_log_status(self, msg):
        self._lbl_log.setText(msg)

    def show_log_disabled(self):
        """Power-only mode: no ESP32 log worker was started."""
        self._lbl_log.setText("ESP32: disabled (power-only mode)")
        self._terminal.add_line(
            0.0, "ESP32 log capture disabled - use the 'ESP log' toolbar "
                 "button to connect a log port anytime.")

    # ------------------------------------------- runtime log connection --
    def set_log_connectors(self, connect_cb, disconnect_cb, is_connected_cb,
                           exclude_ports=None, default_baud=115200):
        """Inject runtime connect/disconnect callbacks from the entry point.

        connect_cb(port, baud) -> bool, disconnect_cb() -> None,
        is_connected_cb() -> bool.
        """
        self._log_connect_cb = connect_cb
        self._log_disconnect_cb = disconnect_cb
        self._log_is_connected = is_connected_cb
        self._log_exclude = list(exclude_ports or [])
        self._log_default_baud = default_baud
        # silent: setChecked would emit toggled and pop the connect dialog
        self._set_esp_action_silently(bool(is_connected_cb()))

    def _set_esp_action_silently(self, on):
        self._act_esp_log.blockSignals(True)
        self._act_esp_log.setChecked(on)
        self._act_esp_log.blockSignals(False)

    def _on_esp_log_toggled(self, on):
        if on:
            dlg = PortDialog(self, exclude=self._log_exclude,
                             default_baud=self._log_default_baud)
            if dlg.exec_() == QDialog.Accepted and dlg.selected():
                port, baud = dlg.selected()
                if not self.connect_log(port, baud):
                    self._set_esp_action_silently(False)
            else:
                self._set_esp_action_silently(False)  # cancelled
        else:
            self.disconnect_log()

    def connect_log(self, port, baud):
        """Start the ESP32 log worker on the given port. Returns success."""
        if self._log_connect_cb is None:
            return False
        if not self._log_connect_cb(port, baud):
            return False
        self._set_esp_action_silently(True)
        self._terminal.add_line(
            self._now_t(), f"--- ESP32 log connected: {port} @ {baud} ---")
        return True

    def disconnect_log(self):
        """Stop the ESP32 log worker (no-op if not connected)."""
        if self._log_disconnect_cb is None:
            return
        self._log_disconnect_cb()
        self._set_esp_action_silently(False)
        self._terminal.add_line(self._now_t(), "--- ESP32 log disconnected ---")

    def _now_t(self):
        t, _, _ = self._buf.view()
        return float(t[-1]) if t.size else 0.0

    # ----------------------------------------------------------- logic ---
    def _on_smooth(self, name):
        tau_i, tau_v = SMOOTH_PRESETS.get(name, (0.0, 0.0))
        self._plot.set_smoothing(tau_i, tau_v)

    def _edit_tags(self):
        dlg = TagsDialog(self._tags, self)
        if dlg.exec_() == QDialog.Accepted:
            self._tags = dlg.tags()
            self._tracker.set_roles(self._tags)
            if self._tags_path:
                try:
                    save_tags_file(self._tags_path, self._tags)
                except OSError as exc:
                    self.statusBar().showMessage(
                        f"Could not save tags: {exc}", 5000)

    @staticmethod
    def _set_checked_silently(action, on):
        action.blockSignals(True)
        action.setChecked(on)
        action.blockSignals(False)

    def _on_span_text(self, text):
        span = parse_span(text)
        if span == "invalid":
            self.statusBar().showMessage(
                f"Invalid span {text!r} - try e.g. 7s, 12s, 1.5m, 5min, All",
                4000)
            self._sync_span_text()  # restore the real span in the field
            return
        self._plot.set_span(span)
        self._sync_span_text()

    def _sync_span_text(self):
        span = self._plot.get_span()
        txt = "All" if span is None else f"{span:g}s"
        self._cmb_span.blockSignals(True)
        self._cmb_span.setCurrentText(txt)
        self._cmb_span.blockSignals(False)

    def _on_pause(self, paused):
        self._paused = paused
        if not paused:
            self._refresh()

    def _on_trigger_toggled(self, on):
        self._trigger_armed = on
        self._trigger_latched = not on  # when armed, wait for the threshold
        self._plot.set_trigger_level(self._spin_trigger.value() if on else None)
        if on:
            self.statusBar().showMessage(
                f"Trigger armed: waiting for I >= "
                f"{self._spin_trigger.value():.1f} mA", 5000)

    def _on_trigger_level(self, value):
        self._trigger_level = float(value)
        if self._trigger_armed:
            self._plot.set_trigger_level(value)

    def _refresh(self):
        if not self._paused:
            t, v, i = self._buf.view()
            self._plot.update_data(t, i, v)
        else:
            self._plot.refresh_view_only()  # keep pan/zoom alive while paused
        self._tick += 1
        if self._tick % STATS_EVERY == 0:
            self._update_stats()

    # ------------------------------------------------------- statistics --
    def _update_stats(self):
        t_all, v_all, i_all = self._buf.view()
        values = {}

        # --- base region stats (manual ROI or whole buffer) ---
        span = self._plot.region_span()
        if span is not None:
            t, v, i = self._buf.slice(*span)
            src = f"selection {span[0]:.2f}-{span[1]:.2f}s"
        else:
            t, v, i = t_all, v_all, i_all
            src = "whole buffer"
        base = region_stats(t, v, i)
        if base is not None:
            values.update({
                "src": src,
                "n": str(base["n"]),
                "t_span": f"{base['t_span']:.3f} s",
                "i_floor": f"{base['i_p05']:.3f} mA",
                "i_max": f"{base['i_max']:.3f} mA",
                "i_avg": f"{base['i_avg']:.3f} mA",
                "charge": fmt_mah(base["charge_mah"]),
                "energy": fmt_mwh(base["energy_mwh"]),
            })
        else:
            values["src"] = src

        # --- live voltage readings ---
        v_now = (windowed_mean(t_all, v_all, 1.0)
                 if t_all.size else None)
        v_win = (windowed_mean(t_all, v_all, self._stats.vwindow_seconds())
                 if t_all.size else None)
        if v_now is not None:
            values["v_now"] = f"{v_now:.3f} V"
        if v_win is not None:
            values["v_win"] = f"{v_win:.3f} V"

        # --- battery model ---
        cap = self._stats.capacity_mah()
        cut = self._stats.cutoff_v()
        usable = usable_mah(cap, cut)
        values["usable"] = fmt_mah(usable)
        remaining = (remaining_mah(cap, cut, v_now)
                     if v_now is not None else None)
        if remaining is not None:
            values["remaining"] = fmt_mah(remaining)

        # --- duty-cycle (marker) analysis ---
        intervals = self._tracker.intervals
        t_now = float(t_all[-1]) if t_all.size else 0.0
        self._plot.update_bands(intervals, t_now)

        cycles = detect_cycles(intervals)
        i_for_life = None
        if cycles and t_all.size:
            cs = cycle_stats(t_all, i_all, cycles)
            if cs:
                charges = np.array([c for c, _p in cs])
                periods = np.array([p for _c, p in cs])
                med_charge = float(np.median(charges))
                med_period = float(np.median(periods))
                if med_period > 0:
                    i_for_life = med_charge * 3600.0 / med_period
                    values.update({
                        "cycles": str(len(cs)),
                        "cyc_period": f"{med_period:.2f} s",
                        "cyc_charge": fmt_mah(med_charge),
                        "cyc_iavg": f"{i_for_life:.3f} mA",
                    })
            s_floor = sleep_current(t_all, i_all, intervals)
            if s_floor is not None:
                values["sleep_floor"] = f"{s_floor:.3f} mA"

        scr = (screen_on_stats(t_all, i_all, intervals)
               if t_all.size else {"avg_i": None})

        if i_for_life is None and base is not None and base["i_avg"] > 0:
            i_for_life = base["i_avg"]  # fallback: flat average
            if cycles == []:
                values["cycles"] = "0 (flat avg)"

        # --- life estimates ---
        if i_for_life is not None and i_for_life > 0:
            if remaining is not None:
                values["life_now"] = fmt_hours(remaining / i_for_life)
            values["life_full"] = fmt_hours(usable / i_for_life)
        if scr.get("avg_i") and remaining is not None:
            values["life_screen"] = fmt_hours(remaining / scr["avg_i"])

        self._stats.update_values(values)

        # --- status-bar view readout ---
        x0, x1 = self._plot.view_range()
        y0, y1 = self._plot.y_range()
        self._lbl_view.setText(
            f"win {x1 - x0:.2f}s  Y {y0:.2f}..{y1:.2f} mA  "
            f"AutoY {'On' if self._plot.is_auto_y() else 'Off'}  "
            f"Follow {'On' if self._plot.is_follow() else 'Off'}")

    def _export_view(self):
        x0, x1 = self._plot.view_range()
        t, v, i = self._buf.slice(x0, x1)
        if t.size == 0:
            self.statusBar().showMessage("Nothing in view to export", 3000)
            return
        default = os.path.join(self._outdir, "view_export.csv")
        path, _ = QFileDialog.getSaveFileName(
            self, "Export visible window", default, "CSV (*.csv)")
        if not path:
            return
        np.savetxt(path, np.column_stack([t, v, i]), delimiter=",",
                   header="t_s,voltage_v,current_ma", comments="",
                   fmt=["%.6f", "%.3f", "%.2f"])
        self.statusBar().showMessage(
            f"Exported {t.size} samples to {path}", 5000)

    # ----------------------------------------------------------- close ---
    def closeEvent(self, event):
        if callable(self.on_close):
            self.on_close()
        super().closeEvent(event)
