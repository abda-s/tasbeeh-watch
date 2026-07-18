"""Side panel: live statistics, 1S-LiPo battery model and life estimates.

The panel is intentionally dumb: MainWindow computes everything and passes a
{key: text} dict to update_values(). Inputs (capacity, cutoff, V window) are
read back via the accessors and emit inputs_changed.
"""
from PyQt5.QtCore import pyqtSignal
from PyQt5.QtWidgets import (QDoubleSpinBox, QFormLayout, QGroupBox,
                             QLabel, QVBoxLayout, QWidget)


class StatsPanel(QWidget):
    inputs_changed = pyqtSignal()

    def __init__(self, default_mah=100.0, default_cutoff_v=3.5, parent=None):
        super().__init__(parent)
        self._labels = {}
        root = QVBoxLayout(self)

        # ---- Statistics --------------------------------------------------
        grp = QGroupBox("Statistics")
        form = QFormLayout(grp)
        for key, title in (
            ("src", "Source"),
            ("n", "Samples"),
            ("t_span", "Duration"),
            ("i_floor", "I floor (p5)"),
            ("i_max", "I max"),
            ("i_avg", "I avg"),
            ("v_now", "V now"),
            ("v_win", "V avg (win)"),
            ("charge", "Charge used"),
            ("energy", "Energy used"),
        ):
            self._add_row(form, key, title)
        self.vwindow = QDoubleSpinBox()
        self.vwindow.setRange(1.0, 600.0)
        self.vwindow.setDecimals(0)
        self.vwindow.setSuffix(" s")
        self.vwindow.setValue(10.0)
        self.vwindow.setToolTip("Window for the rolling V average")
        self.vwindow.valueChanged.connect(lambda _v: self.inputs_changed.emit())
        form.addRow("V window:", self.vwindow)
        root.addWidget(grp)

        # ---- Battery -----------------------------------------------------
        bat = QGroupBox("Battery (1S LiPo model)")
        bform = QFormLayout(bat)
        self.capacity = QDoubleSpinBox()
        self.capacity.setRange(1.0, 1_000_000.0)
        self.capacity.setDecimals(1)
        self.capacity.setSuffix(" mAh")
        self.capacity.setValue(default_mah)
        self.capacity.valueChanged.connect(lambda _v: self.inputs_changed.emit())
        bform.addRow("Capacity:", self.capacity)

        self.cutoff = QDoubleSpinBox()
        self.cutoff.setRange(2.5, 4.2)
        self.cutoff.setDecimals(2)
        self.cutoff.setSingleStep(0.05)
        self.cutoff.setSuffix(" V")
        self.cutoff.setValue(default_cutoff_v)
        self.cutoff.setToolTip(
            "Voltage where the electronics brown out.\n"
            "Waveshare ESP32-S3 board: 3.5 V (ME6211C33 LDO headroom).")
        self.cutoff.valueChanged.connect(lambda _v: self.inputs_changed.emit())
        bform.addRow("Cutoff:", self.cutoff)

        for key, title in (
            ("usable", "Usable @cutoff"),
            ("remaining", "Remaining @Vnow"),
        ):
            self._add_row(bform, key, title)
        root.addWidget(bat)

        # ---- Life estimates ----------------------------------------------
        life = QGroupBox("Life estimates")
        lform = QFormLayout(life)
        for key, title in (
            ("life_now", "Device (from now)"),
            ("life_full", "Device (from full)"),
            ("life_screen", "Screen-on"),
        ):
            self._add_row(lform, key, title)
        root.addWidget(life)

        # ---- Duty cycle --------------------------------------------------
        duty = QGroupBox("Duty cycle (markers)")
        dform = QFormLayout(duty)
        for key, title in (
            ("cycles", "Cycles used"),
            ("cyc_period", "Cycle period"),
            ("cyc_charge", "Charge / cycle"),
            ("cyc_iavg", "I avg (cycle)"),
            ("sleep_floor", "Sleep floor (med)"),
        ):
            self._add_row(dform, key, title)
        root.addWidget(duty)

        hint = QLabel("Drag the white region for a manual ROI.\n"
                      "With [SLEEP]/[WAKE] markers, estimates\n"
                      "use the last full duty cycles instead.")
        hint.setWordWrap(True)
        root.addWidget(hint)
        root.addStretch(1)

    def _add_row(self, form, key, title):
        lbl = QLabel("--")
        self._labels[key] = lbl
        form.addRow(title + ":", lbl)

    # ----------------------------------------------------------- inputs --
    def capacity_mah(self):
        return float(self.capacity.value())

    def cutoff_v(self):
        return float(self.cutoff.value())

    def vwindow_seconds(self):
        return float(self.vwindow.value())

    # ----------------------------------------------------------- output --
    def update_values(self, values):
        for key, lbl in self._labels.items():
            lbl.setText(values.get(key, "--"))
