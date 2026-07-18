"""Synthetic data sources for ``--demo`` mode.

Simulates an ESP32-style duty cycle so the whole pipeline (plot, stats,
markers, CSV export) can be exercised without any hardware:

    3.0 s deep sleep (~0.08 mA) -> 0.2 s wake/sensor (~90 mA)
    -> 0.4 s Wi-Fi TX bursts (~130-310 mA) -> 0.4 s shutdown (~40 mA)
"""
import math
import random
import time

from PyQt5.QtCore import QThread, pyqtSignal

_PHASES = (
    (3.0, ["[SLEEP] entering deep sleep"]),
    (0.2, ["[WAKE] timer interrupt", "[SCREEN_ON] refresh ui"]),
    (0.4, ["[WIFI] tx burst"]),
    (0.4, ["[SCREEN_OFF] display off", "shutdown complete"]),
)
_CYCLE = sum(p[0] for p in _PHASES)


class DemoPowerWorker(QThread):
    batch_received = pyqtSignal(list)
    status_changed = pyqtSignal(str)

    def __init__(self, csv_writer, t0, parent=None):
        super().__init__(parent)
        self._csv = csv_writer
        self._t0 = t0
        self._running = True

    def stop(self):
        self._running = False

    def run(self):
        self.status_changed.emit("DEMO: synthetic power data")
        batch = []
        last_emit = time.monotonic()
        while self._running:
            now = time.monotonic()
            t = now - self._t0
            phase = t % _CYCLE
            if phase < 3.0:                       # deep sleep
                cur = 0.08 + random.uniform(-0.02, 0.02)
            elif phase < 3.2:                     # wake + sensor read
                cur = 90.0 + random.uniform(-5.0, 5.0)
            elif phase < 3.6:                     # Wi-Fi TX bursts
                cur = 130.0 + 180.0 * abs(math.sin(t * 40.0)) \
                    + random.uniform(-10.0, 10.0)
            else:                                 # shutdown ramp
                cur = 40.0 + random.uniform(-3.0, 3.0)
            volt = 3.3 - cur * 0.0008 + random.uniform(-0.005, 0.005)
            if self._csv is not None:
                self._csv.write(t, volt, cur)
            batch.append((t, volt, cur))
            if now - last_emit >= 0.04:
                self.batch_received.emit(batch)
                batch = []
                last_emit = now
            time.sleep(0.01)  # ~100 Hz sample rate
        if batch:
            self.batch_received.emit(batch)


class DemoLogWorker(QThread):
    line_received = pyqtSignal(float, str)
    status_changed = pyqtSignal(str)

    def __init__(self, csv_writer, t0, parent=None):
        super().__init__(parent)
        self._csv = csv_writer
        self._t0 = t0
        self._running = True

    def stop(self):
        self._running = False

    def run(self):
        self.status_changed.emit("DEMO: synthetic log lines")
        last_bucket = -1
        while self._running:
            t = time.monotonic() - self._t0
            phase = t % _CYCLE
            acc = 0.0
            bucket = 0
            for dur, _msgs in _PHASES:
                acc += dur
                if phase < acc:
                    break
                bucket += 1
            if bucket != last_bucket:
                last_bucket = bucket
                for text in _PHASES[bucket][1]:
                    if self._csv is not None:
                        self._csv.write(t, text)
                    self.line_received.emit(t, text)
            time.sleep(0.02)
