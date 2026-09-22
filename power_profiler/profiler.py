#!/usr/bin/env python3
"""Dual-port power profiler.

Reads voltage/current samples from an INA226-equipped Arduino (Nano) on one
serial port and debug log lines from the ESP32-S3 on a second serial port.
Both streams share one time base, so log lines can be overlaid on the power
graph exactly where they happened.

Usage:
    python profiler.py                 # interactive port picker
    python profiler.py --list          # just list serial ports
    python profiler.py --power /dev/ttyUSB0 --logs /dev/ttyACM0
    python profiler.py --power /dev/ttyUSB0 --logs none   # power-only mode
    python profiler.py --demo          # synthetic data, no hardware
"""
import argparse
import os
import sys
import time
from datetime import datetime

import pyqtgraph as pg
from PyQt5.QtWidgets import QApplication

from analysis import load_tags
from data_store import LogCsv, PowerCsv, TimeSeriesBuffer
from ui.main_window import MainWindow

HERE = os.path.dirname(os.path.abspath(__file__))

DARK_STYLESHEET = """
QMainWindow, QDockWidget, QWidget { background: #1d1f21; color: #c5c8c6; }
QToolBar { background: #282a2e; border: 0; spacing: 6px; padding: 4px; }
QToolBar QToolButton { color: #c5c8c6; padding: 4px 8px; }
QToolBar QToolButton:checked { background: #373b41; border-radius: 3px; }
QPlainTextEdit { background: #121314; color: #b5bd68; border: 0; }
QGroupBox { border: 1px solid #373b41; margin-top: 8px; padding-top: 8px; }
QGroupBox::title { subcontrol-origin: margin; left: 6px; }
QLabel { color: #c5c8c6; }
QDoubleSpinBox { background: #282a2e; color: #c5c8c6;
                 border: 1px solid #373b41; padding: 2px; }
QStatusBar { background: #282a2e; }
"""


def list_serial_ports():
    from serial.tools import list_ports
    return list(list_ports.comports())


def print_ports(ports):
    if not ports:
        print("  (no serial ports detected)")
    for n, p in enumerate(ports):
        print(f"  [{n}] {p.device:20s} {p.description}")


def pick_port(ports, role, allow_skip=False):
    """Interactive numbered picker; 'r' rescans. Returns device path or None.

    Hides platform /dev/ttyS* entries (no USB VID) unless that filter would
    leave nothing to choose from. When allow_skip is True, 's' returns None
    (run without that stream).
    """
    usb_only = [p for p in ports if p.vid is not None or "USB" in p.device]
    if usb_only:
        ports = usb_only
    while True:
        print(f"\nSelect the port for {role}:")
        print_ports(ports)
        prompt = f"{role} port number ('r' = rescan"
        if allow_skip:
            print("  [s] skip - run without this stream (power-only mode)")
            prompt += ", 's' = skip"
        choice = input(prompt + "): ").strip()
        if choice.lower() == "r":
            ports = list_serial_ports()
            usb_only = [p for p in ports if p.vid is not None or "USB" in p.device]
            if usb_only:
                ports = usb_only
            continue
        if allow_skip and choice.lower() == "s":
            return None
        try:
            idx = int(choice)
            return ports[idx].device
        except (ValueError, IndexError):
            print("Invalid choice, try again.")


def parse_args(argv):
    ap = argparse.ArgumentParser(description="Dual-port ESP32 power profiler")
    ap.add_argument("--power", help="serial port of the INA226 Arduino")
    ap.add_argument("--logs", help="serial port of the ESP32-S3 debug output; "
                                   "pass 'none' for power-only mode")
    ap.add_argument("--baud-power", type=int, default=115200)
    ap.add_argument("--baud-logs", type=int, default=115200)
    ap.add_argument("--battery-mah", type=float, default=100.0,
                    help="battery capacity for the life estimator (default 100)")
    ap.add_argument("--outdir", help="where to write CSV logs "
                                     "(default: ./logs/<timestamp>/)")
    ap.add_argument("--list", action="store_true", help="list ports and exit")
    ap.add_argument("--demo", action="store_true",
                    help="run with synthetic data (no hardware needed)")
    ap.add_argument("--selftest", type=float, default=0.0,
                    help=argparse.SUPPRESS)  # auto-quit after N s (testing)
    return ap.parse_args(argv)


def main(argv=None):
    args = parse_args(argv if argv is not None else sys.argv[1:])

    if args.list:
        print("Detected serial ports:")
        print_ports(list_serial_ports())
        return 0

    # shared time base for both streams
    t0 = time.monotonic()
    wall0 = time.time()

    # ---- port selection (console, before the GUI starts) ----
    power_port = None
    log_port = None
    if not args.demo:
        ports = list_serial_ports()
        power_port = args.power or pick_port(ports, "the INA226 Arduino (power)")
        if args.logs and args.logs.lower() in ("none", "no", "skip"):
            log_port = None
        else:
            log_port = args.logs or pick_port(
                ports, "the ESP32-S3 (debug logs, optional)", allow_skip=True)
        if log_port is not None and power_port == log_port:
            print("ERROR: power and log ports must be different devices.")
            return 2

    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    outdir = args.outdir or os.path.join(HERE, "logs", stamp)
    os.makedirs(outdir, exist_ok=True)
    power_csv = PowerCsv(os.path.join(outdir, "power.csv"), wall0)
    log_csv_path = os.path.join(outdir, "esp32_log.csv")

    pg.setConfigOptions(antialias=False)

    app = QApplication(sys.argv)
    app.setStyleSheet(DARK_STYLESHEET)

    buf = TimeSeriesBuffer()
    tags_path = os.path.join(HERE, "tags.toml")
    win = MainWindow(buf, outdir, battery_mah=args.battery_mah,
                     tags=load_tags(tags_path), tags_path=tags_path)

    # ---- ESP32 log stream state (runtime connect/disconnect) ----
    state = {"worker": None, "csv": None}

    def _attach_log(worker):
        worker.line_received.connect(win.on_log_line)
        worker.status_changed.connect(win.set_log_status)
        state["worker"] = worker
        worker.start()

    def start_log(port, baud):
        """Create and start a real LogWorker. Returns False if busy."""
        if state["worker"] is not None:
            return False
        from serial_worker import LogWorker
        state["csv"] = LogCsv(log_csv_path, wall0, append=True)
        _attach_log(LogWorker(port, baud, state["csv"], t0))
        return True

    def stop_log():
        worker = state["worker"]
        state["worker"] = None
        if worker is not None:
            worker.stop()
            worker.wait(2000)
        csvw = state["csv"]
        state["csv"] = None
        if csvw is not None:
            csvw.close()
        win.set_log_status("ESP32: disconnected")

    # ---- power worker ----
    if args.demo:
        from demo_workers import DemoLogWorker, DemoPowerWorker
        power_worker = DemoPowerWorker(power_csv, t0)
        state["csv"] = LogCsv(log_csv_path, wall0)
        _attach_log(DemoLogWorker(state["csv"], t0))
    else:
        from serial_worker import PowerWorker
        power_worker = PowerWorker(power_port, args.baud_power, power_csv, t0)
        if log_port is not None:
            start_log(log_port, args.baud_logs)
        else:
            win.show_log_disabled()

    power_worker.batch_received.connect(win.on_power_batch)
    power_worker.status_changed.connect(win.set_power_status)

    win.set_log_connectors(
        start_log, stop_log,
        lambda: state["worker"] is not None,
        exclude_ports=[power_port] if power_port else [],
        default_baud=args.baud_logs)

    def shutdown():
        power_worker.stop()
        stop_log()

    win.on_close = shutdown

    if args.selftest > 0:
        from PyQt5.QtCore import QTimer
        QTimer.singleShot(int(args.selftest * 1000), win.close)

    power_worker.start()
    win.show()
    rc = app.exec_()

    shutdown()
    power_worker.wait(2000)
    power_csv.close()
    print(f"Data saved in {outdir}")
    return rc


if __name__ == "__main__":
    sys.exit(main())
