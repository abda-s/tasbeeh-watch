"""GUI integration test: demo mode + tagged lines + smoothing + stats.

Runs headless (QT_QPA_PLATFORM=offscreen) for ~12s, then checks:
  1. Tagged log lines parsed (SLEEP/WAKE/WIFI tags in tracker)
  2. Stats panel populated (non-dash values)
  3. Smoothing reduces spike variance
  4. State tracker has sleep intervals
"""
import os
import sys
import time
import traceback

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

PASSED = 0
FAILED = 0


def _check(name, cond, detail=""):
    global PASSED, FAILED
    if cond:
        PASSED += 1
    else:
        FAILED += 1
        msg = f"  FAIL: {name}"
        if detail:
            msg += f"  ({detail})"
        print(msg)


def run_test():
    from PyQt5.QtWidgets import QApplication
    from PyQt5.QtCore import QTimer

    app = QApplication.instance() or QApplication(sys.argv)

    from analysis import DEFAULT_TAGS, load_tags, StateTracker
    from data_store import TimeSeriesBuffer
    from demo_workers import DemoLogWorker, DemoPowerWorker
    from ui.main_window import MainWindow

    import tempfile

    outdir = tempfile.mkdtemp(prefix="profiler_gui_test_")
    buf = TimeSeriesBuffer()
    tags_path = os.path.join(outdir, "tags.toml")
    tags = load_tags(tags_path)

    win = MainWindow(buf, outdir, battery_mah=100.0, tags=tags, tags_path=tags_path)
    win.setWindowTitle("GUI integration test")
    win.resize(1200, 700)
    win.show()

    # start demo workers — pass None for CSV (workers check for None)
    t0 = time.monotonic()
    pw = DemoPowerWorker(None, t0)
    lw = DemoLogWorker(None, t0)
    pw.batch_received.connect(buf.append)
    lw.line_received.connect(win.on_log_line)
    pw.start()
    lw.start()

    def check_results():
        try:
            # --- check tracker has intervals ---
            tracker = win._tracker
            ivs = tracker.intervals
            _check("tracker has intervals", len(ivs) > 0,
                   f"got {len(ivs)} intervals")
            states = [iv["state"] for iv in ivs]
            _check("tracker has sleep state", "sleep" in states,
                   f"states: {set(states)}")
            _check("tracker has active/wifi state",
                   "active" in states or "wifi" in states,
                   f"states: {set(states)}")
            _check("tracker has screen state", "screen" in states,
                   f"states: {set(states)}")

            # --- check stats panel has values (not all dashes) ---
            sp = win._stats
            vals = {}
            for key, lbl in sp._labels.items():
                vals[key] = lbl.text()
            _check("stats src populated", vals.get("src", "--") != "--",
                   f"src={vals.get('src')}")
            _check("stats n populated", vals.get("n", "--") != "--",
                   f"n={vals.get('n')}")
            _check("stats i_avg populated", vals.get("i_avg", "--") != "--",
                   f"i_avg={vals.get('i_avg')}")
            _check("stats v_now populated", vals.get("v_now", "--") != "--",
                   f"v_now={vals.get('v_now')}")
            _check("stats charge populated", vals.get("charge", "--") != "--",
                   f"charge={vals.get('charge')}")
            _check("stats usable populated", vals.get("usable", "--") != "--",
                   f"usable={vals.get('usable')}")
            _check("stats remaining populated",
                   vals.get("remaining", "--") != "--",
                   f"remaining={vals.get('remaining')}")
            _check("stats life_full populated",
                   vals.get("life_full", "--") != "--",
                   f"life_full={vals.get('life_full')}")

            # --- check smoothing works ---
            plot = win._plot
            t_all, v_all, i_all = buf.view()
            _check("collected samples", len(i_all) > 100,
                   f"got {len(i_all)} samples")
            if len(i_all) > 100:
                import numpy as np
                from analysis import smooth_boxcar
                i_smooth = smooth_boxcar(t_all, i_all, 0.03)
                raw_std = float(np.std(i_all))
                smooth_std = float(np.std(i_smooth))
                _check("smoothing reduces std",
                       smooth_std < raw_std,
                       f"raw={raw_std:.2f} smooth={smooth_std:.2f}")

            # --- check duty cycle stats if cycles detected ---
            cycles_val = vals.get("cycles", "--")
            if cycles_val not in ("--", "0 (flat avg)"):
                _check("cycle count > 0", True)
                _check("cycle period populated",
                       vals.get("cyc_period", "--") != "--",
                       f"cyc_period={vals.get('cyc_period')}")
            else:
                print(f"  INFO: cycles={cycles_val} (may need more data)")

            _check("sleep floor populated",
                   vals.get("sleep_floor", "--") != "--",
                   f"sleep_floor={vals.get('sleep_floor')}")

        except Exception as e:
            _check("no exception", False, str(e))
            traceback.print_exc()
        finally:
            pw.stop()
            lw.stop()
            pw.wait(2000)
            lw.wait(2000)
            app.quit()

    # let it run for 10s to accumulate data
    QTimer.singleShot(10000, check_results)
    app.exec_()


if __name__ == "__main__":
    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    print("Running GUI integration test (~12s)...")
    run_test()
    print(f"\n{PASSED + FAILED} checks, {PASSED} passed, {FAILED} failed")
    sys.exit(0 if FAILED == 0 else 1)
