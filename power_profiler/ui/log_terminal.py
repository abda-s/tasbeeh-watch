"""Scrolling ESP32 log terminal panel."""
from PyQt5.QtGui import QFont
from PyQt5.QtWidgets import QPlainTextEdit


class LogTerminal(QPlainTextEdit):
    MAX_BLOCKS = 20000  # ring-buffer of lines kept on screen

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setReadOnly(True)
        self.setMaximumBlockCount(self.MAX_BLOCKS)
        font = QFont("DejaVu Sans Mono", 9)
        font.setStyleHint(QFont.Monospace)
        self.setFont(font)

    def add_line(self, t, text):
        self.appendPlainText(f"[{t:12.3f}s] {text}")
        bar = self.verticalScrollBar()
        bar.setValue(bar.maximum())  # auto-scroll to newest
