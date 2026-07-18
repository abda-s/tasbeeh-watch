"""Dialog to pick a serial port for the ESP32 log stream at runtime."""
from PyQt5.QtCore import Qt
from PyQt5.QtWidgets import (QComboBox, QDialog, QDialogButtonBox,
                             QHBoxLayout, QLabel, QListWidget,
                             QListWidgetItem, QPushButton, QVBoxLayout)

BAUDS = ("9600", "19200", "38400", "57600", "115200",
         "230400", "460800", "921600")


class PortDialog(QDialog):
    """Port list + baud selector. selected() -> (device, baud) or None."""

    def __init__(self, parent=None, exclude=None, default_baud=115200):
        super().__init__(parent)
        self.setWindowTitle("Connect ESP32 log port")
        self._exclude = set(exclude or [])

        layout = QVBoxLayout(self)
        layout.addWidget(QLabel("Serial port for the ESP32 debug log:"))
        self._list = QListWidget()
        self._list.itemDoubleClicked.connect(lambda _item: self.accept())
        layout.addWidget(self._list)

        row = QHBoxLayout()
        row.addWidget(QLabel("Baud:"))
        self._baud = QComboBox()
        self._baud.setEditable(True)
        self._baud.addItems(BAUDS)
        self._baud.setCurrentText(str(default_baud))
        row.addWidget(self._baud)
        refresh = QPushButton("Refresh")
        refresh.clicked.connect(self.refresh)
        row.addWidget(refresh)
        row.addStretch(1)
        layout.addLayout(row)

        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

        self.refresh()

    def refresh(self):
        from serial.tools import list_ports
        self._list.clear()
        ports = [p for p in list_ports.comports()
                 if p.device not in self._exclude]
        usb = [p for p in ports if p.vid is not None or "USB" in p.device]
        for p in (usb or ports):
            item = QListWidgetItem(f"{p.device}  -  {p.description}")
            item.setData(Qt.UserRole, p.device)
            self._list.addItem(item)
        if self._list.count():
            self._list.setCurrentRow(0)

    def selected(self):
        item = self._list.currentItem()
        if item is None:
            return None
        try:
            baud = int(self._baud.currentText())
        except ValueError:
            baud = 115200
        return item.data(Qt.UserRole), baud
