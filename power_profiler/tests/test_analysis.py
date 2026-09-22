"""Unit tests for analysis.py — battery model, smoothing, tag tracking, duty-cycle."""
import os
import sys
import tempfile

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from analysis import (
    DEFAULT_TAGS, ROLES,
    StateTracker, detect_cycles, cycle_stats, fmt_hours, fmt_mah, fmt_mwh,
    load_tags, parse_tag_line, remaining_mah, save_tags_file, screen_on_stats,
    sleep_current, smooth_boxcar, soc_at_voltage, usable_mah, windowed_mean,
)
from data_store import region_stats

PASSED = 0
FAILED = 0


def _check(name, cond):
    global PASSED, FAILED
    if cond:
        PASSED += 1
    else:
        FAILED += 1
        print(f"  FAIL: {name}")


# ------------------------------------------------------------------ battery model
def test_soc_at_voltage():
    _check("SOC 4.2V=100%", abs(soc_at_voltage(4.2) - 100.0) < 0.1)
    _check("SOC 3.0V=0%", abs(soc_at_voltage(3.0) - 0.0) < 0.1)
    _check("SOC 3.7V~mid", 35.0 < soc_at_voltage(3.7) < 60.0)
    _check("SOC clamp low", soc_at_voltage(2.0) >= 0.0)
    _check("SOC clamp high", soc_at_voltage(5.0) <= 100.0)
    _check("SOC monotonically increasing", soc_at_voltage(3.4) < soc_at_voltage(3.8))


def test_usable_mah():
    u100 = usable_mah(100.0, 3.5)
    _check("usable 100mAh@3.5V > 0", u100 > 0)
    _check("usable 100mAh@3.5V < 100", u100 < 100.0)
    u_high = usable_mah(100.0, 3.0)
    u_low = usable_mah(100.0, 4.0)
    _check("lower cutoff -> more usable", u_high > u_low)
    _check("cutoff above full -> 0", usable_mah(100.0, 4.2) == 0.0)


def test_remaining_mah():
    r_full = remaining_mah(100.0, 3.5, 4.2)
    r_half = remaining_mah(100.0, 3.5, 3.7)
    _check("remaining at full > half", r_full > r_half)
    _check("remaining at cutoff = 0", remaining_mah(100.0, 3.5, 3.5) == 0.0)
    _check("remaining at full < capacity", r_full < 100.0)


# ------------------------------------------------------------------ smoothing
def test_smooth_boxcar():
    t = np.linspace(0, 1, 1000)
    y = np.ones(1000) * 5.0
    s = smooth_boxcar(t, y, 0.05)
    _check("smooth constant=constant", np.allclose(s, 5.0, atol=0.01))

    y2 = np.where(t < 0.5, 1.0, 9.0)
    s2 = smooth_boxcar(t, y2, 0.1)
    _check("smooth step transition is gradual", s2[300] < s2[700])
    _check("smooth preserves mean roughly", abs(np.mean(s2) - np.mean(y2)) < 0.5)

    _check("smooth tau<=0 returns y", np.array_equal(smooth_boxcar(t, y, 0.0), y))
    _check("smooth short array returns y", np.array_equal(smooth_boxcar(np.array([1.0]), np.array([5.0]), 0.1), np.array([5.0])))


def test_smooth_preserves_integral():
    """Smoothing must not change the charge integral (area under curve)."""
    rng = np.random.default_rng(42)
    t = np.linspace(0, 5, 2000)
    i_raw = 50.0 + 30.0 * rng.random(2000)
    i_smooth = smooth_boxcar(t, i_raw, 0.05)
    trapz = getattr(np, "trapezoid", None) or np.trapz
    q_raw = trapz(i_raw, t)
    q_smooth = trapz(i_smooth, t)
    _check(f"integral diff < 0.5% (got {abs(q_raw - q_smooth)/q_raw*100:.3f}%)",
           abs(q_raw - q_smooth) / q_raw < 0.005)


# ------------------------------------------------------------------ percentile / windowed
def test_percentile():
    a = np.array([1.0, 2.0, 3.0, 4.0, 5.0])
    _check("p50=3", abs(__import__("analysis").percentile(a, 50) - 3.0) < 0.01)
    _check("p05<1.5", __import__("analysis").percentile(a, 5) < 1.5)
    _check("empty=nan", __import__("analysis").percentile(np.array([]), 50) != __import__("analysis").percentile(np.array([]), 50))


def test_windowed_mean():
    t = np.array([0.0, 1.0, 2.0, 3.0, 4.0, 5.0])
    y = np.array([10.0, 10.0, 10.0, 10.0, 20.0, 20.0])
    m = windowed_mean(t, y, 2.0)
    # last 2s includes t>=3.0: values [10,20,20] -> mean 16.67
    _check("windowed last 2s mean~16.7", abs(m - 16.67) < 0.5)
    m5 = windowed_mean(t, y, 5.0)
    _check("windowed 5s mean~13.3", abs(m5 - 13.33) < 0.5)
    _check("empty=None", windowed_mean(np.array([]), np.array([]), 1.0) is None)


# ------------------------------------------------------------------ tag parsing
def test_parse_tag_line():
    tag, rest = parse_tag_line("[SLEEP] ds 10s")
    _check("tag=SLEEP", tag == "SLEEP")
    _check("rest=ds 10s", rest == "ds 10s")

    tag2, rest2 = parse_tag_line("[wifi] connected")
    _check("tag=wifi uppercased", tag2 == "WIFI")

    tag3, rest3 = parse_tag_line("no tag here")
    _check("no tag -> None", tag3 is None)

    tag4, rest4 = parse_tag_line("  [INDENTED] ok")
    _check("indented tag works", tag4 == "INDENTED")


# ------------------------------------------------------------------ tags file I/O
def test_tags_file_roundtrip():
    with tempfile.TemporaryDirectory() as td:
        path = os.path.join(td, "tags.toml")
        tags = {"SLEEP": "sleep", "WAKE": "wake", "LED": "active"}
        save_tags_file(path, tags)
        loaded = load_tags(path)
        _check("roundtrip SLEEP", loaded.get("SLEEP") == "sleep")
        _check("roundtrip WAKE", loaded.get("WAKE") == "wake")
        _check("roundtrip LED", loaded.get("LED") == "active")

        # test default creation
        path2 = os.path.join(td, "new_tags.toml")
        d = load_tags(path2)
        _check("default SLEEP exists", "SLEEP" in d)
        _check("default created file", os.path.exists(path2))


# ------------------------------------------------------------------ StateTracker
def test_state_tracker_basic():
    st = StateTracker(DEFAULT_TAGS)
    st.feed(0.0, "SLEEP", "deep sleep")
    st.feed(3.0, "WAKE", "timer")
    st.feed(3.2, "WIFI", "tx")
    st.feed(3.6, "SCREEN_ON", "refresh")
    st.feed(4.0, "SCREEN_OFF", "done")
    st.feed(4.0, "SLEEP", "deep sleep")
    ivs = st.intervals
    _check("has intervals", len(ivs) > 0)
    states = [iv["state"] for iv in ivs]
    _check("has sleep", "sleep" in states)
    _check("has active", "active" in states or "wifi" in states)
    _check("has screen", "screen" in states)

    # first sleep interval should be closed by the second sleep
    sleep_ivs = [iv for iv in ivs if iv["state"] == "sleep"]
    _check("first sleep closed", sleep_ivs[0]["t1"] is not None)
    # last sleep is open (no subsequent sleep to close it)
    _check("last sleep open", sleep_ivs[-1]["t1"] is None)


def test_state_tracker_point():
    st = StateTracker(DEFAULT_TAGS)
    r = st.feed(1.0, "CLICK", "button press")
    _check("unknown tag -> point", r == "point")
    # point tags are tagged but don't open a band — intervals stays clean
    _check("point leaves no intervals", len(st.intervals) == 0)


# ------------------------------------------------------------------ detect_cycles / cycle_stats
def test_detect_cycles():
    intervals = [
        {"state": "sleep", "t0": 0, "t1": 3, "tag": "SLEEP"},
        {"state": "active", "t0": 3, "t1": 4, "tag": "WAKE"},
        {"state": "sleep", "t0": 4, "t1": 7, "tag": "SLEEP"},
        {"state": "active", "t0": 7, "t1": 8, "tag": "WAKE"},
        {"state": "sleep", "t0": 8, "t1": 11, "tag": "SLEEP"},
        {"state": "active", "t0": 11, "t1": 12, "tag": "WAKE"},
        {"state": "sleep", "t0": 12, "t1": 15, "tag": "SLEEP"},
    ]
    cycles = detect_cycles(intervals, n_cycles=2)
    _check("detect 2 cycles", len(cycles) == 2)
    _check("cycle start < end", all(a < b for a, b in cycles))
    _check("cycles are consecutive sleeps", cycles[1][0] > cycles[0][0])


def test_cycle_stats():
    t = np.linspace(0, 10, 1000)
    i = np.where((t % 5) < 3, 0.1, 50.0)
    cycles = [(0, 5), (5, 10)]
    cs = cycle_stats(t, i, cycles)
    _check("2 cycles returned", len(cs) == 2)
    charges = [c for c, _p in cs]
    _check("charges positive", all(c > 0 for c in charges))
    _check("periods ~5s", all(abs(p - 5.0) < 0.1 for _, p in cs))


def test_cycle_stats_skip_short():
    # 1 sample window should be skipped (needs >= 2)
    t = np.array([1.0])
    i = np.array([10.0])
    cs = cycle_stats(t, i, [(1.0, 1.5)])
    _check("1-sample window skipped", len(cs) == 0)


# ------------------------------------------------------------------ screen_on_stats
def test_screen_on_stats():
    t = np.linspace(0, 10, 1000)
    i = np.ones(1000) * 50.0
    intervals = [
        {"state": "screen", "t0": 2.0, "t1": 4.0, "tag": "SCREEN_ON"},
        {"state": "screen", "t0": 6.0, "t1": None, "tag": "SCREEN_ON"},  # open, skip
    ]
    ss = screen_on_stats(t, i, intervals)
    _check("1 complete screen interval", ss["count"] == 1)
    _check("screen time ~2s", abs(ss["time_s"] - 2.0) < 0.1)
    _check("screen avg_i ~50mA", abs(ss["avg_i"] - 50.0) < 1.0)


# ------------------------------------------------------------------ sleep_current
def test_sleep_current():
    t = np.linspace(0, 10, 1000)
    i_sleep = np.ones(500) * 0.05
    i_active = np.ones(500) * 80.0
    i = np.concatenate([i_sleep, i_active])
    intervals = [
        {"state": "sleep", "t0": 0, "t1": 5, "tag": "SLEEP"},
        {"state": "active", "t0": 5, "t1": 10, "tag": "WAKE"},
    ]
    sc = sleep_current(t, i, intervals)
    _check("sleep current ~0.05", sc is not None and abs(sc - 0.05) < 0.01)


def test_sleep_current_none():
    sc = sleep_current(np.array([]), np.array([]), [])
    _check("empty -> None", sc is None)


# ------------------------------------------------------------------ formatting
def test_fmt_mah():
    _check("fmt_mah None", fmt_mah(None) == "--")
    _check("fmt_mah small", "uAh" in fmt_mah(0.001))
    _check("fmt_mah normal", "mAh" in fmt_mah(5.0))


def test_fmt_mwh():
    _check("fmt_mwh None", fmt_mwh(None) == "--")
    _check("fmt_mwh small", "uWh" in fmt_mwh(0.001))
    _check("fmt_mwh normal", "mWh" in fmt_mwh(5.0))


def test_fmt_hours():
    _check("fmt_hours None", fmt_hours(None) == "--")
    _check("fmt_hours nan", fmt_hours(float("nan")) == "--")
    _check("fmt_hours inf", fmt_hours(float("inf")) == "--")
    _check("fmt_hours negative", fmt_hours(-1) == "0 h")
    _check("fmt_hours hours", "h" in fmt_hours(5.0))
    _check("fmt_hours days", "days" in fmt_hours(72.0))
    _check("fmt_hours months", "months" in fmt_hours(8000.0))


# ------------------------------------------------------------------ integration: region_stats
def test_region_stats_basic():
    t = np.linspace(0, 5, 500)
    v = np.full(500, 3.8)
    i = np.full(500, 10.0)
    rs = region_stats(t, v, i)
    _check("region_stats n", rs["n"] == 500)
    _check("region_stats t_span", abs(rs["t_span"] - 5.0) < 0.01)
    _check("region_stats i_avg", abs(rs["i_avg"] - 10.0) < 0.01)
    _check("region_stats charge > 0", rs["charge_mah"] > 0)
    _check("region_stats i_p05", rs["i_p05"] <= rs["i_avg"])


def test_region_stats_empty():
    _check("empty -> None", region_stats(np.array([]), np.array([]), np.array([])) is None)


# ------------------------------------------------------------------ main
if __name__ == "__main__":
    print("Running analysis unit tests...")
    test_soc_at_voltage()
    test_usable_mah()
    test_remaining_mah()
    test_smooth_boxcar()
    test_smooth_preserves_integral()
    test_percentile()
    test_windowed_mean()
    test_parse_tag_line()
    test_tags_file_roundtrip()
    test_state_tracker_basic()
    test_state_tracker_point()
    test_detect_cycles()
    test_cycle_stats()
    test_cycle_stats_skip_short()
    test_screen_on_stats()
    test_sleep_current()
    test_sleep_current_none()
    test_fmt_mah()
    test_fmt_mwh()
    test_fmt_hours()
    test_region_stats_basic()
    test_region_stats_empty()
    print(f"\n{PASSED + FAILED} tests, {PASSED} passed, {FAILED} failed")
    sys.exit(0 if FAILED == 0 else 1)
