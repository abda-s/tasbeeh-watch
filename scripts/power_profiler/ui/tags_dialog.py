"""Dialog to edit the marker tag -> role mapping at runtime."""
from PyQt5.QtWidgets import (QComboBox, QDialog, QDialogButtonBox,
                             QHBoxLayout, QLabel, QPushButton,
                             QTableWidget, QTableWidgetItem, QVBoxLayout)

from analysis import ROLES


class TagsDialog(QDialog):
    """Edit {TAG: role}. tags() returns the new mapping on accept."""

    def __init__(self, tags, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Marker tags")
        self.resize(420, 320)

        layout = QVBoxLayout(self)
        layout.addWidget(QLabel(
            "Tag the ESP32 prints in brackets, e.g. [SLEEP], and the role\n"
            "the profiler assigns to it (sleep/wake drive the duty cycle)."))

        self._table = QTableWidget(0, 2)
        self._table.setHorizontalHeaderLabels(["Tag", "Role"])
        self._table.horizontalHeader().setStretchLastSection(True)
        layout.addWidget(self._table)

        row = QHBoxLayout()
        add = QPushButton("Add tag")
        add.clicked.connect(lambda: self._append_row("", "point"))
        row.addWidget(add)
        rm = QPushButton("Remove selected")
        rm.clicked.connect(self._remove_selected)
        row.addWidget(rm)
        row.addStretch(1)
        layout.addLayout(row)

        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

        for tag, role in tags.items():
            self._append_row(tag, role)

    def _append_row(self, tag, role):
        r = self._table.rowCount()
        self._table.insertRow(r)
        self._table.setItem(r, 0, QTableWidgetItem(tag))
        combo = QComboBox()
        combo.addItems(ROLES)
        if role in ROLES:
            combo.setCurrentText(role)
        self._table.setCellWidget(r, 1, combo)

    def _remove_selected(self):
        for idx in sorted({i.row() for i in self._table.selectedIndexes()},
                          reverse=True):
            self._table.removeRow(idx)

    def tags(self):
        out = {}
        for r in range(self._table.rowCount()):
            item = self._table.item(r, 0)
            text = (item.text().strip().upper() if item else "")
            if not text:
                continue
            out[text] = self._table.cellWidget(r, 1).currentText()
        return out
