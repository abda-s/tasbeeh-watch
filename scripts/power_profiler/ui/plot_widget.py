"""Live dual-axis power plot with an oscilloscope-style windowed view.

Left axis  : current (mA), yellow.
Right axis : bus voltage (V), blue -- second ViewBox linked on X.

View model (replaces full-buffer autorange, which compressed the trace as
data accumulated -- the "shrinking" problem):

* X is a fixed-width time window owned by this widget. With Follow on, the
  window slides so its right edge tracks the newest sample. "All" shows the
  whole buffer. No full-buffer X autorange is ever used.
* With Auto Y on, the Y range is re-fit every refresh to the min/max of the
  *visible window only* (+5% padding) -- history outside the window can no
  longer flatten the deep-sleep floor. With Auto Y off, Y is frozen.
* Mouse: plain wheel = X-only zoom, Ctrl/Shift+wheel = Y-only zoom
  (ZoomableViewBox). Any manual mouse range change disarms the matching
  auto behaviour (Follow and/or Auto Y) via sigRangeChangedManually,
  oscilloscope-style. Re-enable with the toolbar toggles.
* Right-drag box zoom, left-drag pan and the right-click context menu are
  stock pyqtgraph and are preserved.
"""
import pyqtgraph as pg
from PyQt5.QtCore import Qt, pyqtSignal

from analysis import smooth_boxcar

COLOR_CURRENT = "#f0c674"
COLOR_VOLTAGE = "#81a2be"
COLOR_MARKER = "#cc6666"
COLOR_TRIGGER = "#b5bd68"

# state interval bands (RGBA) drawn behind the trace
BAND_COLORS = {
    "sleep": (58, 80, 107, 45),
    "active": (91, 140, 90, 35),
    "screen": (212, 175, 55, 40),
}
BAND_COLOR_DEFAULT = (140, 140, 140, 30)

MAX_MARKERS = 1000   # drawn markers cap; the log terminal keeps full history
MAX_BANDS = 400      # drawn state bands cap
SMOOTH_MAX_POINTS = 200_000  # above this, render raw (perf guard)
MIN_SPAN = 0.05      # seconds
MAX_SPAN = 24 * 3600.0
Y_PAD = 0.05         # auto-Y headroom
V_PAD = 0.10         # auto-Y headroom for the voltage axis

# toolbar presets: name -> (current tau s, voltage tau s)
SMOOTH_PRESETS = {
    "Off": (0.0, 0.0),
    "Light": (0.03, 0.08),
    "Medium": (0.08, 0.20),
    "Strong": (0.20, 0.50),
}


class ZoomableViewBox(pg.ViewBox):
    """ViewBox whose wheel zooms a single axis per modifier.

    plain wheel -> X (time), Ctrl/Shift+wheel -> Y (amplitude).
    Implemented via ViewBox.wheelEvent's per-axis mask.
    """

    def wheelEvent(self, ev, axis=None):
        if axis is None:
            axis = 1 if ev.modifiers() & (Qt.ControlModifier
                                          | Qt.ShiftModifier) else 0
        super().wheelEvent(ev, axis=axis)


class PowerPlot(pg.PlotWidget):
    region_changed = pyqtSignal(float, float)  # (t_start, t_end)
    user_override_x = pyqtSignal()   # user moved the time axis by mouse
    user_override_y = pyqtSignal()   # user moved the amplitude axis by mouse
    span_changed = pyqtSignal(object)  # new span seconds (None == All)

    def __init__(self, parent=None):
        self._span = 10.0        # seconds; None == All (whole buffer)
        self._follow = True
        self._auto_y = True
        self._log_scale = False
        self._smooth_i = 0.03    # display EMA-ish window (s), current
        self._smooth_v = 0.08    # display smoothing (s), voltage
        self._t = self._i = self._v = None

        vb = ZoomableViewBox()
        pi = pg.PlotItem(viewBox=vb)
        super().__init__(plotItem=pi, parent=parent)
        self.plot_item = pi

        pi.setLabel("left", "Current", units="mA", color=COLOR_CURRENT)
        pi.setLabel("bottom", "Time", units="s")
        pi.showAxis("right")
        pi.setLabel("right", "Voltage", units="V", color=COLOR_VOLTAGE)
        pi.showGrid(x=True, y=True, alpha=0.25)

        # we own both axes' ranges; disable stock autorange so nothing fights
        pi.vb.enableAutoRange(axis="x", enable=False)
        pi.vb.enableAutoRange(axis="y", enable=False)

        # ---- second ViewBox for the voltage axis (linked on X) ----
        self.vb_right = pg.ViewBox()
        pi.scene().addItem(self.vb_right)
        pi.getAxis("right").linkToView(self.vb_right)
        self.vb_right.setXLink(pi)
        self.vb_right.enableAutoRange(axis="y", enable=False)
        self._update_views()
        pi.vb.sigResized.connect(self._update_views)

        # ---- curves ----
        self.curve_i = pi.plot(pen=pg.mkPen(COLOR_CURRENT, width=2))
        self.curve_i.setDownsampling(auto=True, method="peak")
        self.curve_i.setClipToView(True)

        self.curve_v = pg.PlotDataItem(pen=pg.mkPen(COLOR_VOLTAGE, width=1))
        self.curve_v.setDownsampling(auto=True, method="peak")
        self.curve_v.setClipToView(True)
        self.vb_right.addItem(self.curve_v)

        # ---- ROI cursors (LinearRegionItem) ----
        self.region = pg.LinearRegionItem(values=(0.0, 0.0))
        self.region.setZValue(-10)
        pi.addItem(self.region, ignoreBounds=True)
        self.region.sigRegionChanged.connect(self._on_region)

        # ---- trigger level line (hidden until armed) ----
        self._trigger_line = pg.InfiniteLine(
            angle=0, movable=False,
            pen=pg.mkPen(COLOR_TRIGGER, style=Qt.DashLine))
        self._trigger_line.setVisible(False)
        pi.addItem(self._trigger_line, ignoreBounds=True)

        self._markers = []  # list[(InfiniteLine, TextItem)]
        self._bands = {}    # id(interval) -> (LinearRegionItem, (t0, t1))

        # disarm Follow/Auto-Y when the user moves the view by mouse
        vb.sigRangeChangedManually.connect(self._on_manual_range)

    # ---------------------------------------------------------- geometry --
    def _update_views(self):
        self.vb_right.setGeometry(self.plot_item.vb.sceneBoundingRect())

    # ------------------------------------------------------------- data --
    def update_data(self, t, i, v):
        self._t, self._i, self._v = t, i, v
        if t.size == 0:
            return
        self._apply_view()

    def refresh_view_only(self):
        """Re-render the current data (used while paused so pan still works)."""
        if self._t is None or self._t.size == 0:
            return
        (x0, x1), _ = self.plot_item.vb.viewRange()
        if self._auto_y:
            self._fit_y(x0, x1)
        self._render(x0, x1)

    # ---------------------------------------------------- view management --
    def _apply_view(self):
        t = self._t
        if t is None or t.size == 0:
            return
        if self._follow:
            t_latest = float(t[-1])
            if self._span is None:
                x0, x1 = float(t[0]), t_latest
            else:
                x0, x1 = t_latest - self._span, t_latest
            self.plot_item.setXRange(x0, x1, padding=0)
        (x0v, x1v), _ = self.plot_item.vb.viewRange()
        if self._auto_y:
            self._fit_y(x0v, x1v)
        self._render(x0v, x1v)

    def _render(self, x0, x1):
        """Draw only the visible slice, with optional display smoothing.

        Smoothing is display-only: statistics and Auto-Y always use raw
        samples, so charge/energy integrals are never affected.
        """
        t, i, v = self._t, self._i, self._v
        m = (t >= x0) & (t <= x1)
        if not m.any():
            return
        ts = t[m]
        isel = i[m]
        vsel = v[m]
        if ts.size <= SMOOTH_MAX_POINTS:
            if self._smooth_i > 0.0:
                isel = smooth_boxcar(ts, isel, self._smooth_i)
            if self._smooth_v > 0.0:
                vsel = smooth_boxcar(ts, vsel, self._smooth_v)
        self.curve_i.setData(ts, isel)
        self.curve_v.setData(ts, vsel)

    def _fit_y(self, x0, x1):
        """Auto-fit both Y axes to the data inside the visible X window."""
        t, i, v = self._t, self._i, self._v
        mask = (t >= x0) & (t <= x1)
        if not mask.any():
            return
        sel = i[mask]
        lo, hi = float(sel.min()), float(sel.max())
        if lo == hi:
            lo -= 0.5
            hi += 0.5
        pad = Y_PAD * (hi - lo)
        lo -= pad
        hi += pad
        if self._log_scale:
            lo = max(lo, 1e-3)  # log range must stay positive
        self.plot_item.setYRange(lo, hi, padding=0)

        vsel = v[mask]
        vlo, vhi = float(vsel.min()), float(vsel.max())
        if vlo == vhi:
            vlo -= 0.05
            vhi += 0.05
        vpad = V_PAD * (vhi - vlo)
        self.vb_right.setYRange(vlo - vpad, vhi + vpad, padding=0)

    def _on_manual_range(self, mask):
        """sigRangeChangedManually: user dragged/wheeled the view."""
        if mask[0]:  # time axis moved by mouse -> stop following
            self._follow = False
            (x0, x1), _ = self.plot_item.vb.viewRange()
            self._span = min(max(x1 - x0, MIN_SPAN), MAX_SPAN)
            self.span_changed.emit(self._span)
            self.user_override_x.emit()
        if mask[1]:  # amplitude axis moved by mouse -> stop auto-fit
            self._auto_y = False
            self.user_override_y.emit()

    # ------------------------------------------------------ view controls --
    def set_span(self, seconds):
        """Set the visible window width in seconds (None == All)."""
        self._span = seconds
        if not self._follow and seconds is not None:
            (x0, x1), _ = self.plot_item.vb.viewRange()
            mid = 0.5 * (x0 + x1)
            self.plot_item.setXRange(mid - seconds / 2.0,
                                     mid + seconds / 2.0, padding=0)
        self._apply_view()

    def get_span(self):
        return self._span

    def zoom_x(self, factor):
        """Multiply the window width by factor (>1 wider, <1 narrower)."""
        (x0, x1), _ = self.plot_item.vb.viewRange()
        width = max(x1 - x0, MIN_SPAN)
        span = min(max(width * factor, MIN_SPAN), MAX_SPAN)
        self._span = span
        if not self._follow:
            mid = 0.5 * (x0 + x1)
            self.plot_item.setXRange(mid - span / 2.0,
                                     mid + span / 2.0, padding=0)
        self._apply_view()
        self.span_changed.emit(span)

    def zoom_y(self, factor):
        """Scale the current-axis Y range about its centre (<1 zooms in).

        Disables Auto Y (a manual zoom is meaningless while auto-fit is on).
        Works in log space too: viewRange returns log coords in log mode, so
        additive scaling there == multiplicative zoom in linear units.
        """
        self._auto_y = False
        self.user_override_y.emit()
        _, (y0, y1) = self.plot_item.vb.viewRange()
        mid = 0.5 * (y0 + y1)
        half = max(0.5 * (y1 - y0) * factor, 1e-6)
        self.plot_item.setYRange(mid - half, mid + half, padding=0)

    def set_follow(self, on):
        self._follow = bool(on)
        if self._follow:
            self._apply_view()

    def set_auto_y(self, on):
        self._auto_y = bool(on)
        if self._auto_y:
            self._apply_view()

    def is_follow(self):
        return self._follow

    def is_auto_y(self):
        return self._auto_y

    def set_smoothing(self, tau_i, tau_v):
        """Display smoothing windows in seconds (0 == raw)."""
        self._smooth_i = float(tau_i)
        self._smooth_v = float(tau_v)
        self.refresh_view_only()

    # ------------------------------------------------------------ bands --
    def update_bands(self, intervals, t_now):
        """Draw state intervals as colored bands behind the trace.

        intervals: list of {"state", "t0", "t1"|None, "tag"} from
        analysis.StateTracker. Open intervals extend to t_now.
        """
        shown = intervals[-MAX_BANDS:]
        seen = set()
        for iv in shown:
            key = id(iv)
            t0 = iv["t0"]
            t1 = iv["t1"] if iv["t1"] is not None else t_now
            if t1 <= t0:
                continue
            seen.add(key)
            span = (t0, t1)
            entry = self._bands.get(key)
            if entry is None:
                color = BAND_COLORS.get(iv["state"], BAND_COLOR_DEFAULT)
                band = pg.LinearRegionItem(
                    values=span, movable=False,
                    brush=pg.mkBrush(*color),
                    pen=pg.mkPen(color[0], color[1], color[2], 120, width=1))
                band.setZValue(-20)
                band.setAcceptedMouseButtons(Qt.NoButton)
                self.plot_item.addItem(band, ignoreBounds=True)
                self._bands[key] = (band, span)
            elif entry[1] != span:
                entry[0].setRegion(span)
                self._bands[key] = (entry[0], span)
        # drop bands that fell out of the window
        for key in [k for k in self._bands if k not in seen]:
            band, _span = self._bands.pop(key)
            self.plot_item.removeItem(band)

    # ----------------------------------------------------------- region --
    def _on_region(self):
        a, b = self.region.getRegion()
        self.region_changed.emit(float(a), float(b))

    def region_span(self):
        """(t_start, t_end) if the user dragged out a region, else None."""
        a, b = self.region.getRegion()
        if abs(b - a) < 1e-9:
            return None
        return float(min(a, b)), float(max(a, b))

    # ---------------------------------------------------------- markers --
    def add_marker(self, t, text):
        line = pg.InfiniteLine(
            pos=t, angle=90, movable=False,
            pen=pg.mkPen(COLOR_MARKER, style=Qt.DashLine))
        self.plot_item.addItem(line, ignoreBounds=True)

        # label at the current top of the left axis
        (_xmin, _xmax), (_ymin, ymax) = self.plot_item.vb.viewRange()
        label = pg.TextItem(text=text[:60], color=COLOR_MARKER, anchor=(0, 1))
        label.setPos(t, ymax)
        self.plot_item.addItem(label, ignoreBounds=True)

        self._markers.append((line, label))
        if len(self._markers) > MAX_MARKERS:
            old_line, old_label = self._markers.pop(0)
            self.plot_item.removeItem(old_line)
            self.plot_item.removeItem(old_label)

    def clear_markers(self):
        for line, label in self._markers:
            self.plot_item.removeItem(line)
            self.plot_item.removeItem(label)
        self._markers.clear()

    # ----------------------------------------------------------- options --
    def set_log_scale(self, on):
        self._log_scale = bool(on)
        self.plot_item.setLogMode(x=False, y=bool(on))
        self._apply_view()

    def set_trigger_level(self, level_ma):
        if level_ma is None:
            self._trigger_line.setVisible(False)
        else:
            self._trigger_line.setPos(float(level_ma))
            self._trigger_line.setVisible(True)

    def view_range(self):
        """Currently visible (t_start, t_end)."""
        (x0, x1), _ = self.plot_item.vb.viewRange()
        return float(x0), float(x1)

    def y_range(self):
        """Current-axis visible (y_min, y_max) (log coords in log mode)."""
        _, (y0, y1) = self.plot_item.vb.viewRange()
        return float(y0), float(y1)
