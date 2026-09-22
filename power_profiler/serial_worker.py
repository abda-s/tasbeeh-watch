"""Serial reader threads for the dual-port power profiler.

Each worker owns one serial port, writes parsed rows directly to its own CSV
file (so disk I/O never blocks the GUI thread) and emits data via Qt signals.
Both workers share the same time base: ``time.monotonic() - t0`` seconds.
"""
import time

import serial
from PyQt5.QtCore import QThread, pyqtSignal


class _ReconnectingWorker(QThread):
    """Base class: open port, read, auto-reconnect on failure."""

    status_changed = pyqtSignal(str)

    def __init__(self, name, port, baud, t0, parent=None):
        super().__init__(parent)
        self._name = name
        self._port = port
        self._baud = baud
        self._t0 = t0
        self._running = True

    def stop(self):
        self._running = False

    def run(self):
        while self._running:
            try:
                self.status_changed.emit(
                    f"{self._name}: connecting {self._port} @ {self._baud} ...")
                with serial.Serial(self._port, self._baud, timeout=0.05) as ser:
                    self.status_changed.emit(f"{self._name}: connected {self._port}")
                    self._read_loop(ser)
            except (serial.SerialException, OSError) as exc:
                if self._running:
                    self.status_changed.emit(
                        f"{self._name}: disconnected ({exc}); retrying in 1 s")
                    time.sleep(1.0)
            except Exception as exc:  # never let the thread die silently
                self.status_changed.emit(
                    f"{self._name}: error ({exc}); retrying in 1 s")
                time.sleep(1.0)

    def _read_loop(self, ser):
        raise NotImplementedError


class PowerWorker(_ReconnectingWorker):
    """Reads ``voltage,current`` CSV lines from the INA226 Arduino.

    Emits batches (~25 Hz) of ``(t_rel, voltage_v, current_ma)`` tuples so the
    GUI is not flooded with one signal per sample.
    """

    batch_received = pyqtSignal(list)  # list[tuple[float, float, float]]

    def __init__(self, port, baud, csv_writer, t0, parent=None):
        super().__init__("INA226", port, baud, t0, parent)
        self._csv = csv_writer

    def _read_loop(self, ser):
        batch = []
        last_emit = time.monotonic()
        while self._running:
            raw = ser.readline()
            now = time.monotonic()
            if raw:
                parsed = self._parse(raw)
                if parsed is not None:
                    voltage, current_ma = parsed
                    t = now - self._t0
                    self._csv.write(t, voltage, current_ma)
                    batch.append((t, voltage, current_ma))
            if batch and (now - last_emit) >= 0.04:
                self.batch_received.emit(batch)
                batch = []
                last_emit = now
        if batch:
            self.batch_received.emit(batch)

    @staticmethod
    def _parse(raw):
        """Return (voltage, current_ma) or None for non-data lines."""
        try:
            line = raw.decode("ascii", errors="ignore").strip()
            if not line or "," not in line:
                return None
            v_s, i_s = line.split(",", 1)
            return float(v_s), float(i_s)
        except ValueError:
            return None


class LogWorker(_ReconnectingWorker):
    """Reads free-form debug lines from the ESP32-S3."""

    line_received = pyqtSignal(float, str)  # (t_rel, text)

    def __init__(self, port, baud, csv_writer, t0, parent=None):
        super().__init__("ESP32", port, baud, t0, parent)
        self._csv = csv_writer

    def _read_loop(self, ser):
        while self._running:
            raw = ser.readline()
            if raw:
                t = time.monotonic() - self._t0
                text = raw.decode("utf-8", errors="replace").rstrip()
                if text:
                    self._csv.write(t, text)
                    self.line_received.emit(t, text)
