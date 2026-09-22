"""Storage layer: CSV writers, in-memory time-series buffer, region stats."""
import csv
import os
from datetime import datetime

import numpy as np

# numpy 2.x renamed trapz -> trapezoid
_trapz = getattr(np, "trapezoid", None) or np.trapz


def _iso(wall0, t):
    return datetime.fromtimestamp(wall0 + t).isoformat(timespec="milliseconds")


class PowerCsv:
    """Appends power samples to CSV. Written from the serial worker thread."""

    def __init__(self, path, wall0):
        self._f = open(path, "w", newline="")
        self._w = csv.writer(self._f)
        self._w.writerow(["t_s", "iso_time", "voltage_v", "current_ma"])
        self._wall0 = wall0
        self._rows = 0

    def write(self, t, voltage, current_ma):
        self._w.writerow([f"{t:.6f}", _iso(self._wall0, t),
                          f"{voltage:.3f}", f"{current_ma:.2f}"])
        self._rows += 1
        if self._rows % 200 == 0:
            self._f.flush()

    def close(self):
        try:
            self._f.flush()
            self._f.close()
        except Exception:
            pass


class LogCsv:
    """Appends ESP32 log lines to CSV. Written from the serial worker thread.

    With append=True an existing non-empty file is continued without a
    duplicate header (used when the log port is reconnected at runtime).
    """

    def __init__(self, path, wall0, append=False):
        resume = (append and os.path.exists(path)
                  and os.path.getsize(path) > 0)
        self._f = open(path, "a" if resume else "w", newline="")
        self._w = csv.writer(self._f)
        if not resume:
            self._w.writerow(["t_s", "iso_time", "line"])
        self._wall0 = wall0

    def write(self, t, text):
        self._w.writerow([f"{t:.6f}", _iso(self._wall0, t), text])
        self._f.flush()  # low volume; flush every line for crash safety

    def close(self):
        try:
            self._f.flush()
            self._f.close()
        except Exception:
            pass


class TimeSeriesBuffer:
    """Auto-growing numpy buffer for (t, voltage, current) display data.

    The CSV on disk is always the complete record; this buffer only feeds the
    plot and the statistics. Capacity doubles when full, so memory grows with
    run length (~16 MB per 1M samples).
    """

    def __init__(self, capacity=1 << 20):
        self.t = np.empty(capacity, dtype=np.float64)
        self.v = np.empty(capacity, dtype=np.float32)
        self.i = np.empty(capacity, dtype=np.float32)
        self.n = 0

    def __len__(self):
        return self.n

    def append(self, batch):
        m = len(batch)
        if m == 0:
            return
        if self.n + m > self.t.size:
            new_cap = max(self.t.size * 2, self.n + m)
            t2 = np.empty(new_cap, dtype=np.float64)
            v2 = np.empty(new_cap, dtype=np.float32)
            i2 = np.empty(new_cap, dtype=np.float32)
            t2[:self.n] = self.t[:self.n]
            v2[:self.n] = self.v[:self.n]
            i2[:self.n] = self.i[:self.n]
            self.t, self.v, self.i = t2, v2, i2
        arr = np.asarray(batch, dtype=np.float64)  # shape (m, 3)
        self.t[self.n:self.n + m] = arr[:, 0]
        self.v[self.n:self.n + m] = arr[:, 1]
        self.i[self.n:self.n + m] = arr[:, 2]
        self.n += m

    def view(self):
        """Whole buffer as (t, v, i) array views."""
        return self.t[:self.n], self.v[:self.n], self.i[:self.n]

    def slice(self, t_start, t_end):
        """Samples with t_start <= t <= t_end as (t, v, i) array views."""
        if self.n == 0:
            empty64 = np.empty(0, dtype=np.float64)
            empty32 = np.empty(0, dtype=np.float32)
            return empty64, empty32, empty32
        a = int(np.searchsorted(self.t[:self.n], t_start))
        b = int(np.searchsorted(self.t[:self.n], t_end, side="right"))
        return self.t[a:b], self.v[a:b], self.i[a:b]


def region_stats(t, v, i):
    """Min/max/avg + charge (mAh) and energy (mWh) via trapezoid integration.

    Charge  = integral of I dt          -> mA*s / 3600 = mAh
    Energy  = integral of (V * I) dt    -> mW*s / 3600 = mWh
    """
    n = int(t.size)
    if n == 0:
        return None
    stats = {
        "n": n,
        "t_span": float(t[-1] - t[0]) if n > 1 else 0.0,
        "i_min": float(np.min(i)),
        "i_p05": float(np.percentile(i, 5)),  # robust floor (noise-proof)
        "i_max": float(np.max(i)),
        "i_avg": float(np.mean(i)),
        "v_avg": float(np.mean(v)),
        "charge_mah": 0.0,
        "energy_mwh": 0.0,
    }
    if n > 1:
        stats["charge_mah"] = float(_trapz(i.astype(np.float64), t)) / 3600.0
        power_mw = v.astype(np.float64) * i.astype(np.float64)
        stats["energy_mwh"] = float(_trapz(power_mw, t)) / 3600.0
    return stats
