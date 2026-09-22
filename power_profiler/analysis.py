"""Battery model, duty-cycle (marker) analysis, smoothing and formatting.

Battery model
-------------
Built-in piecewise-linear 1S-LiPo V(SoC) curve. Two capacity views:
* ``usable_mah``    -- mAh from a full charge (4.2 V) down to the cutoff
                       voltage where the electronics brown out.
* ``remaining_mah`` -- mAh from the current battery voltage down to cutoff.

Both come from the same curve, so an estimate started mid-discharge reports
time-to-cutoff, not time-from-full.

Duty-cycle (marker) analysis
----------------------------
The ESP32 prints tagged lines like ``[SLEEP] ds 10s`` on its debug UART.
``StateTracker`` turns those tags into state intervals (sleep / active /
screen / named sub-bands). ``detect_cycles`` + ``cycle_stats`` extract one
representative sleep->...->sleep cycle so the life estimate uses a steady
duty cycle instead of a whole-buffer average polluted by one-time boot
bursts. ``screen_on_stats`` powers the "how long can the screen stay lit"
estimate. With no markers present, callers fall back to the plain average.

Tag <-> role mapping lives in ``tags.toml`` (stdlib tomllib, no new deps)
and is editable at runtime via the Tags dialog.
"""
import os
import re
import tomllib

import numpy as np

_trapz = getattr(np, "trapezoid", None) or np.trapz

# --------------------------------------------------------------------------
# 1S LiPo discharge curve: (SoC %, voltage). Typical small 1S cell.
# --------------------------------------------------------------------------
_LIPO_SOC = np.array([0.0, 5.0, 10.0, 15.0, 20.0, 30.0, 40.0, 50.0,
                      60.0, 80.0, 95.0, 100.0])
_LIPO_VOLT = np.array([3.00, 3.40, 3.49, 3.55, 3.60, 3.66, 3.70, 3.74,
                       3.78, 3.90, 4.10, 4.20])

V_FULL = 4.20


def soc_at_voltage(v):
    """State of charge (%) for a battery terminal voltage (clamped 0..100)."""
    return float(np.interp(v, _LIPO_VOLT, _LIPO_SOC))


def usable_mah(nominal_mah, cutoff_v):
    """mAh deliverable from a full charge down to cutoff_v."""
    frac = (soc_at_voltage(V_FULL) - soc_at_voltage(cutoff_v)) / 100.0
    return nominal_mah * max(0.0, frac)


def remaining_mah(nominal_mah, cutoff_v, v_now):
    """mAh deliverable from the current voltage v_now down to cutoff_v."""
    frac = (soc_at_voltage(v_now) - soc_at_voltage(cutoff_v)) / 100.0
    return nominal_mah * max(0.0, frac)


# --------------------------------------------------------------------------
# Smoothing (display only -- statistics always use raw samples)
# --------------------------------------------------------------------------
def smooth_boxcar(t, y, tau_s):
    """Time-windowed moving average with edge padding (no edge droop).

    tau_s <= 0 or a window of <= 1 sample returns y unchanged.
    """
    n = int(y.size)
    if n < 3 or tau_s <= 0.0:
        return y
    dt = float(np.median(np.diff(t)))
    if dt <= 0.0:
        return y
    win = max(1, int(round(tau_s / dt)) | 1)  # odd, >= 1
    if win <= 1:
        return y
    half = win // 2
    yp = np.pad(y.astype(np.float64), half, mode="edge")
    return np.convolve(yp, np.ones(win, dtype=np.float64) / win, mode="valid")


# --------------------------------------------------------------------------
# Robust statistics helpers
# --------------------------------------------------------------------------
def percentile(a, p):
    return float(np.percentile(a, p)) if a.size else float("nan")


def windowed_mean(t, y, seconds):
    """Mean of y over the last `seconds` of the series (None if empty)."""
    if t.size == 0:
        return None
    t0 = float(t[-1]) - seconds
    m = t >= t0
    if not m.any():
        return None
    return float(np.mean(y[m]))


# --------------------------------------------------------------------------
# Tag parsing + state tracking
# --------------------------------------------------------------------------
_TAG_RE = re.compile(r"^\s*\[([A-Za-z0-9_]+)\]\s*(.*)$")

ROLES = ("sleep", "wake", "screen_on", "screen_off", "active", "point")

DEFAULT_TAGS = {
    "SLEEP": "sleep",
    "WAKE": "wake",
    "SCREEN_ON": "screen_on",
    "SCREEN_OFF": "screen_off",
    "WIFI": "active",
}

_TAGS_TEMPLATE = '''# Marker tag -> role mapping for the power profiler.
# The ESP32 prints tagged lines on its debug UART, e.g.:
#     Serial.println("[SLEEP] ds 10s");
#     Serial.println("[WAKE] timer");
#     Serial.println("[SCREEN_ON]");
#     Serial.println("[SCREEN_OFF]");
# Any line starting with [TAG] (case-insensitive) is matched against this
# file. Unknown [TAG]s and untagged lines stay plain point markers.
#
# Roles:
#   sleep       closes the previous state, opens a SLEEP band (cycle start)
#   wake        closes the previous state, opens an ACTIVE band
#   screen_on   opens a SCREEN band (screen-on life estimate)
#   screen_off  closes the SCREEN band
#   active      opens a named sub-band (e.g. WIFI) until the next tag
#   point       plain vertical marker, no band

[[tag]]
text = "SLEEP"
role = "sleep"

[[tag]]
text = "WAKE"
role = "wake"

[[tag]]
text = "SCREEN_ON"
role = "screen_on"

[[tag]]
text = "SCREEN_OFF"
role = "screen_off"

[[tag]]
text = "WIFI"
role = "active"
'''


def parse_tag_line(line):
    """Split '[TAG] rest of message' -> (TAG_UPPER, rest); else (None, line)."""
    m = _TAG_RE.match(line)
    if not m:
        return None, line
    return m.group(1).upper(), m.group(2)


def load_tags(path):
    """Load TAG->role from tags.toml; write the default file if missing."""
    if not os.path.exists(path):
        save_tags_file(path, DEFAULT_TAGS, template=True)
        return dict(DEFAULT_TAGS)
    try:
        with open(path, "rb") as f:
            data = tomllib.load(f)
    except (tomllib.TOMLDecodeError, OSError):
        return dict(DEFAULT_TAGS)
    tags = {}
    for entry in data.get("tag", []):
        text = str(entry.get("text", "")).strip().upper()
        role = str(entry.get("role", "point")).strip().lower()
        if text and role in ROLES:
            tags[text] = role
    return tags or dict(DEFAULT_TAGS)


def save_tags_file(path, tags, template=False):
    """Persist TAG->role to tags.toml."""
    if template:
        with open(path, "w") as f:
            f.write(_TAGS_TEMPLATE)
        return
    lines = ["# Marker tag -> role mapping (edited via the Tags dialog).",
             "# See README.md for the role meanings.", ""]
    for tag, role in tags.items():
        lines.append("[[tag]]")
        lines.append(f'text = "{tag}"')
        lines.append(f'role = "{role}"')
        lines.append("")
    with open(path, "w") as f:
        f.write("\n".join(lines))


class StateTracker:
    """Turn tagged log events into state intervals.

    intervals: list of {"state": str, "t0": float, "t1": float|None,
                        "tag": str}. t1 is None while the interval is open.
    """

    def __init__(self, tag_roles=None):
        self.set_roles(tag_roles or DEFAULT_TAGS)
        self.intervals = []
        self._open = {}  # state -> open interval dict

    def set_roles(self, tag_roles):
        self._roles = {k.upper(): v for k, v in tag_roles.items()}

    def feed(self, t, tag, text):
        """Process one tagged event at time t. Returns the role (or None)."""
        if tag is None:
            return None
        role = self._roles.get(tag.upper(), "point")
        if role == "sleep":
            self._close_all(t)
            self._open_interval("sleep", t, tag.upper())
        elif role == "wake":
            self._close_all(t)
            self._open_interval("active", t, tag.upper())
        elif role == "screen_on":
            self._open_interval("screen", t, tag.upper())
        elif role == "screen_off":
            self._close("screen", t)
        elif role == "active":
            # named sub-band, e.g. WIFI; closes any other open sub-band
            self._close(tag.upper(), t)
            self._open_interval(tag.lower(), t, tag.upper())
        return role

    def _open_interval(self, state, t, tag):
        if state in self._open:  # reopen: close the dangling one first
            self._close(state, t)
        iv = {"state": state, "t0": t, "t1": None, "tag": tag}
        self.intervals.append(iv)
        self._open[state] = iv

    def _close(self, state, t):
        iv = self._open.pop(state, None)
        if iv is not None and iv["t1"] is None:
            iv["t1"] = t

    def _close_all(self, t):
        for state in list(self._open):
            self._close(state, t)


# --------------------------------------------------------------------------
# Duty-cycle / screen-on analysis
# --------------------------------------------------------------------------
def detect_cycles(intervals, n_cycles=3):
    """(t_start, t_end) of the last n_cycles complete SLEEP->SLEEP cycles."""
    sleeps = [iv for iv in intervals if iv["state"] == "sleep"]
    cycles = [(a["t0"], b["t0"]) for a, b in zip(sleeps, sleeps[1:])]
    return cycles[-n_cycles:]


def cycle_stats(t, i, cycles):
    """Per-cycle (charge_mah, period_s); skips windows with < 2 samples."""
    out = []
    for t0, t1 in cycles:
        m = (t >= t0) & (t <= t1)
        if m.sum() < 2:
            continue
        charge = _trapz(i[m].astype(np.float64), t[m]) / 3600.0
        out.append((charge, t1 - t0))
    return out


def screen_on_stats(t, i, intervals):
    """Totals over all complete SCREEN intervals + average current."""
    tot_charge = 0.0
    tot_time = 0.0
    count = 0
    for iv in intervals:
        if iv["state"] != "screen" or iv["t1"] is None:
            continue
        m = (t >= iv["t0"]) & (t <= iv["t1"])
        if m.sum() < 2:
            continue
        tot_charge += _trapz(i[m].astype(np.float64), t[m]) / 3600.0
        tot_time += iv["t1"] - iv["t0"]
        count += 1
    avg_i = (tot_charge * 3600.0 / tot_time) if tot_time > 0 else None
    return {"count": count, "charge_mah": tot_charge,
            "time_s": tot_time, "avg_i": avg_i}


def sleep_current(t, i, intervals):
    """Median current across SLEEP intervals (robust deep-sleep floor)."""
    vals = []
    for iv in intervals:
        if iv["state"] != "sleep":
            continue
        t1 = iv["t1"] if iv["t1"] is not None else float(t[-1])
        m = (t >= iv["t0"]) & (t <= t1)
        if m.any():
            vals.append(float(np.median(i[m])))
    return float(np.median(vals)) if vals else None


# --------------------------------------------------------------------------
# Formatting helpers (shared by the stats panel)
# --------------------------------------------------------------------------
def fmt_mah(x):
    if x is None:
        return "--"
    if abs(x) < 0.01:
        return f"{x * 1000.0:.2f} uAh"
    return f"{x:.4f} mAh"


def fmt_mwh(x):
    if x is None:
        return "--"
    if abs(x) < 0.01:
        return f"{x * 1000.0:.2f} uWh"
    return f"{x:.4f} mWh"


def fmt_hours(hours):
    if hours is None or hours != hours or hours == float("inf"):
        return "--"
    if hours < 0:
        return "0 h"
    if hours < 48.0:
        return f"{hours:.1f} h"
    if hours < 24.0 * 60.0:
        return f"{hours / 24.0:.1f} days"
    return f"{hours / 24.0 / 30.0:.1f} months"
