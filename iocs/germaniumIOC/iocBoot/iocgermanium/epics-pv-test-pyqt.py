#!/usr/bin/env python3
"""
epics-pv-test-pyqt.py — EPICS PV Test Tool (PyQt GUI)
──────────────────────────────────────────────────────────────────
A graphical tool for testing and validating EPICS Process Variables (PVs).

Features:
- Load PV list from file (pv.list)
- Table-based interface with per-PV controls:
    • Write value (for caput)
    • Read value (parsed from caget output)
    • Read-only flag (auto-enabled for *_RBV PVs)
    • Tested flag (user-controlled and auto-updated)
- Interactive GUI:
    • Mouse-driven selection and editing
    • Search and locate PVs
    • Sort by PV name, Read-only, or Tested status
- Test execution sequence per PV:
    1. caget pv
    2. caput pv <value>          (if not read-only)
    3. caput pv.PROC 0
    4. caput pv.PROC 1
    5. caput pv.PROC 0
    6. caget pv
- Real-time logging panel showing command execution and results
- Persistent storage of PV settings in pv_values.json

Notes:
- Designed for EPICS environments with command-line tools (caget, caput)
- Handles multi-word string PV values correctly
- Read column updates automatically after each test step
- Table supports sorting and dynamic resizing for large PV sets

Usage:
    ./epics-pv-test-pyqt.py [pv.list]

Dependencies:
    - Python 3
    - PyQt5
    - EPICS command-line tools in PATH (caget, caput)

Author:
    Ji Li <liji@bnl.gov>
"""
import sys
import time
import subprocess
import json
import os
from datetime import datetime

from PyQt5.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QHBoxLayout,
    QPushButton, QTextEdit, QTableWidget,
    QTableWidgetItem, QHeaderView, QStyledItemDelegate,
    QStyle, QStyleOptionButton, QLineEdit, QLabel
)
from PyQt5.QtCore import Qt, QTimer
from PyQt5.QtGui import QFont, QFontMetrics

PV_FILE = "pv.list"
VAL_FILE = "pv_values.json"
CA_DELAY = 1000


# ── Sortable item (fix checkbox sorting) ───────────────────────
class SortableItem(QTableWidgetItem):
    def __lt__(self, other):
        self_data = self.data(Qt.UserRole)
        other_data = other.data(Qt.UserRole)

        if self_data is not None and other_data is not None:
            return self_data < other_data

        return self.text() < other.text()


# ── Centered checkbox delegate ─────────────────────────────────
class CenteredCheckboxDelegate(QStyledItemDelegate):
    def paint(self, painter, option, index):
        value = index.data(Qt.CheckStateRole)
        if value is None:
            super().paint(painter, option, index)
            return

        checkbox = QStyleOptionButton()
        checkbox.state |= QStyle.State_Enabled

        if value == Qt.Checked:
            checkbox.state |= QStyle.State_On
        else:
            checkbox.state |= QStyle.State_Off

        style = option.widget.style() if option.widget else QApplication.style()
        rect = style.subElementRect(QStyle.SE_CheckBoxIndicator, checkbox, option.widget)

        checkbox.rect = rect
        checkbox.rect.moveCenter(option.rect.center())

        style.drawControl(QStyle.CE_CheckBox, checkbox, painter)

    def editorEvent(self, event, model, option, index):
        if not (index.flags() & Qt.ItemIsUserCheckable):
            return False

        if event.type() == event.MouseButtonRelease:
            current = index.data(Qt.CheckStateRole)
            new = Qt.Unchecked if current == Qt.Checked else Qt.Checked
            model.setData(index, new, Qt.CheckStateRole)
            return True

        if event.type() == event.KeyPress:
            if event.key() in (Qt.Key_Space, Qt.Key_Return, Qt.Key_Enter):
                current = index.data(Qt.CheckStateRole)
                new = Qt.Unchecked if current == Qt.Checked else Qt.Checked
                model.setData(index, new, Qt.CheckStateRole)
                return True

        return False


# ── Helpers ────────────────────────────────────────────────────
def load_pvs():
    try:
        with open(PV_FILE) as f:
            return [l.strip() for l in f if l.strip() and not l.startswith('#')]
    except:
        return []


def run_ca(cmd):
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=5)
        return r.returncode == 0, (r.stdout + r.stderr).strip()
    except Exception as e:
        return False, str(e)


def parse_caget(output):
    try:
        parts = output.strip().split(None, 1)
        if len(parts) == 2:
            return parts[1].strip()
        return ""
    except:
        return ""


# ── Main App ───────────────────────────────────────────────────
class App(QWidget):
    COL_PV = 0
    COL_WRITE = 1
    COL_READ = 2
    COL_RO = 3
    COL_TESTED = 4

    def __init__(self):
        super().__init__()
        self.setWindowTitle("EPICS PV Test Tool")

        self.search_index = -1
        self.data = self.load_values()

        # Table
        self.table = QTableWidget()
        self.table.setColumnCount(5)
        self.table.setHorizontalHeaderLabels(
            ["PV", "Write", "Read", "Read-Only", "Tested"]
        )
        self.table.setSortingEnabled(True)

        vh = self.table.verticalHeader()
        vh.setMinimumWidth(50)
        vh.setDefaultAlignment(Qt.AlignCenter)

        for col in range(self.table.columnCount()):
            self.table.horizontalHeaderItem(col).setTextAlignment(Qt.AlignCenter)

        header = self.table.horizontalHeader()
        header.setSectionResizeMode(self.COL_PV, QHeaderView.Fixed)
        header.setSectionResizeMode(self.COL_WRITE, QHeaderView.ResizeToContents)
        header.setSectionResizeMode(self.COL_READ, QHeaderView.Stretch)
        header.setSectionResizeMode(self.COL_RO, QHeaderView.ResizeToContents)
        header.setSectionResizeMode(self.COL_TESTED, QHeaderView.ResizeToContents)

        delegate = CenteredCheckboxDelegate(self.table)
        self.table.setItemDelegateForColumn(self.COL_RO, delegate)
        self.table.setItemDelegateForColumn(self.COL_TESTED, delegate)

        # Log
        self.log = QTextEdit()
        self.log.setReadOnly(True)

        # Buttons
        self.test_btn = QPushButton("Test Selected")
        self.test_all_btn = QPushButton("Test All")
        self.reload_btn = QPushButton("Reload")
        self.quit_btn = QPushButton("Quit")

        # Search
        self.search_box = QLineEdit()
        self.search_box.setPlaceholderText("Search PV...")
        self.find_btn = QPushButton("Find Next")

        self.setFont(QFont("Courier", 11))
        self.log.setFont(QFont("Courier", 10))

        # Layout
        main = QVBoxLayout()
        top = QHBoxLayout()
        top.addWidget(self.table, 3)
        top.addWidget(self.log, 2)

        btns = QHBoxLayout()
        btns.addWidget(QLabel("Find:"))
        btns.addWidget(self.search_box)
        btns.addWidget(self.find_btn)
        btns.addStretch()
        btns.addWidget(self.test_btn)
        btns.addWidget(self.test_all_btn)
        btns.addWidget(self.reload_btn)
        btns.addWidget(self.quit_btn)

        main.addLayout(top)
        main.addLayout(btns)
        self.setLayout(main)

        # Signals
        self.reload_btn.clicked.connect(self.reload_pvs)
        self.test_btn.clicked.connect(self.test_selected)
        self.test_all_btn.clicked.connect(self.test_all)
        self.quit_btn.clicked.connect(self.close)
        self.table.itemChanged.connect(self.on_item_changed)

        self.find_btn.clicked.connect(self.find_next)
        self.search_box.returnPressed.connect(self.find_next)
        self.search_box.textChanged.connect(lambda: setattr(self, "search_index", -1))

        self.reload_pvs()

    # ── Data ─────────────────────────────────
    def load_values(self):
        if os.path.exists(VAL_FILE):
            with open(VAL_FILE) as f:
                return json.load(f)
        return {}

    def save_values(self):
        with open(VAL_FILE, "w") as f:
            json.dump(self.data, f, indent=2)

    # ── Table ────────────────────────────────
    def reload_pvs(self):
        pvs = load_pvs()

        self.table.setSortingEnabled(False)
        self.table.blockSignals(True)
        self.table.setRowCount(len(pvs))

        for row, pv in enumerate(pvs):
            entry = self.data.setdefault(pv, {})
            is_rbv = pv.endswith("_RBV")

            entry = {
                "write": entry.get("write", "0"),
                "read": entry.get("read", ""),
                "readonly": True if is_rbv else entry.get("readonly", False),
                "tested": entry.get("tested", False),
            }
            self.data[pv] = entry

            pv_item = QTableWidgetItem(pv)
            pv_item.setFlags(pv_item.flags() & ~Qt.ItemIsEditable)
            pv_item.setTextAlignment(Qt.AlignLeft | Qt.AlignVCenter)
            pv_item.setToolTip(pv)
            self.table.setItem(row, self.COL_PV, pv_item)

            write_item = QTableWidgetItem(entry["write"])
            write_item.setTextAlignment(Qt.AlignCenter)
            self.table.setItem(row, self.COL_WRITE, write_item)

            read_item = QTableWidgetItem(entry["read"])
            read_item.setFlags(read_item.flags() & ~Qt.ItemIsEditable)
            read_item.setTextAlignment(Qt.AlignCenter)
            read_item.setToolTip(entry["read"])
            self.table.setItem(row, self.COL_READ, read_item)

            ro_item = SortableItem()
            ro_item.setCheckState(Qt.Checked if entry["readonly"] else Qt.Unchecked)
            ro_item.setData(Qt.UserRole, 1 if entry["readonly"] else 0)
            ro_item.setFlags(Qt.ItemIsUserCheckable | Qt.ItemIsEnabled)
            self.table.setItem(row, self.COL_RO, ro_item)

            tested_item = SortableItem()
            tested_item.setCheckState(Qt.Checked if entry["tested"] else Qt.Unchecked)
            tested_item.setData(Qt.UserRole, 1 if entry["tested"] else 0)
            tested_item.setFlags(Qt.ItemIsUserCheckable | Qt.ItemIsEnabled)
            self.table.setItem(row, self.COL_TESTED, tested_item)

        self.table.blockSignals(False)
        self.table.setSortingEnabled(True)

        # PV width
        fm = QFontMetrics(self.table.font())
        px = max((fm.horizontalAdvance(pv) for pv in pvs), default=150) + 20
        self.table.setColumnWidth(self.COL_PV, min(px, 600))

        self.save_values()

    # ── REQUIRED FUNCTIONS (previously missing) ───────────────
    def on_item_changed(self, item):
        row = item.row()
        pv = self.table.item(row, self.COL_PV).text()
        entry = self.data.setdefault(pv, {})

        if item.column() == self.COL_WRITE:
            entry["write"] = item.text()
            item.setTextAlignment(Qt.AlignCenter)

        elif item.column() == self.COL_RO:
            val = item.checkState() == Qt.Checked
            entry["readonly"] = val
            item.setData(Qt.UserRole, 1 if val else 0)

        elif item.column() == self.COL_TESTED:
            val = item.checkState() == Qt.Checked
            entry["tested"] = val
            item.setData(Qt.UserRole, 1 if val else 0)

        self.save_values()

    def update_read(self, row, pv, val):
        item = self.table.item(row, self.COL_READ)
        if item:
            item.setText(val)
            item.setToolTip(val)
            item.setTextAlignment(Qt.AlignCenter)

        if pv in self.data:
            self.data[pv]["read"] = val

    def set_checked(self, row, col, state):
        item = self.table.item(row, col)
        if item:
            item.setCheckState(Qt.Checked if state else Qt.Unchecked)

    # ── Search ───────────────────────────────
    def find_next(self):
        text = self.search_box.text().strip().lower()
        if not text:
            return

        row_count = self.table.rowCount()
        start = self.search_index + 1

        for i in range(row_count):
            row = (start + i) % row_count
            pv = self.table.item(row, self.COL_PV).text().lower()

            if text in pv:
                self.search_index = row
                self.table.selectRow(row)
                self.table.scrollToItem(
                    self.table.item(row, self.COL_PV),
                    QTableWidget.PositionAtCenter
                )
                return

        self.log_line(f"No match: {text}")

    # ── Logging ─────────────────────────────
    def log_line(self, text):
        ts = datetime.now().strftime('%H:%M:%S')
        self.log.append(f"[{ts}] {text}")

    # ── Testing ─────────────────────────────
    def test_selected(self):
        rows = sorted(set(i.row() for i in self.table.selectedIndexes()))
        if not rows:
            self.log_line("No rows selected")
            return
        self.run_sequence(rows)

    def test_all(self):
        self.run_sequence(list(range(self.table.rowCount())))

    def run_sequence(self, rows):
        self.test_rows = rows
        self.current = 0
        self.timer = QTimer()
        self.timer.timeout.connect(self.run_step)
        self.timer.start(CA_DELAY)

    def run_step(self):
        if self.current >= len(self.test_rows):
            self.log_line("Done")
            self.log_line("=======================================================")
            self.timer.stop()
            return

        row = self.test_rows[self.current]
        pv = self.table.item(row, self.COL_PV).text()
        entry = self.data[pv]

        write_val = entry["write"]
        readonly = entry["readonly"]

        self.log_line(f"── {pv} ──")

        self.log_line(f"▶ caget {pv}")
        ok, out = run_ca(['caget', pv])
        self.log_line(("✔" if ok else "✘") + " " + out)
        self.update_read(row, pv, parse_caget(out))

        if not readonly:
            self.log_line(f"▶ caput {pv} {write_val}")
            ok, out = run_ca(['caput', pv, write_val])
            self.log_line(("✔" if ok else "✘") + " " + out)

#        for val in ['0', '1', '0']:
#            self.log_line(f"▶ caput {pv}.PROC {val}")
#            ok, out = run_ca(['caput', f"{pv}.PROC", val])
#            self.log_line(("✔" if ok else "✘") + " " + out)

        self.log_line(f"▶ caget {pv}")
        ok, out = run_ca(['caget', pv])
        self.log_line(("✔" if ok else "✘") + " " + out)
        self.update_read(row, pv, parse_caget(out))

        time.sleep(0.1)

        self.log_line(f"▶ caget {pv}")
        ok, out = run_ca(['caget', pv])
        self.log_line(("✔" if ok else "✘") + " " + out)
        self.update_read(row, pv, parse_caget(out))

        self.set_checked(row, self.COL_TESTED, True)
        self.data[pv]["tested"] = True
        self.save_values()

        self.current += 1


if __name__ == "__main__":
    app = QApplication(sys.argv)
    w = App()

    screen = app.primaryScreen().availableGeometry()
    w.resize(1200, int(screen.height() * 0.8))

    w.show()
    sys.exit(app.exec_())
