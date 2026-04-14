#!/usr/bin/env python3
import curses
import subprocess
import json
import os
from datetime import datetime

PV_FILE = "pv.list"
VAL_FILE = "pv_values.json"
MAX_LOG = 500


# ── Helpers ─────────────────────────────────────────

def load_pvs():
    try:
        with open(PV_FILE) as f:
            return [l.strip() for l in f if l.strip() and not l.startswith("#")]
    except:
        return []


def load_data():
    if os.path.exists(VAL_FILE):
        with open(VAL_FILE) as f:
            return json.load(f)
    return {}


def save_data(data):
    with open(VAL_FILE, "w") as f:
        json.dump(data, f, indent=2)


def run_ca(cmd):
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=5)
        return r.returncode == 0, (r.stdout + r.stderr).strip()
    except Exception as e:
        return False, str(e)


def parse_caget(out):
    try:
        parts = out.strip().split(None, 1)
        return parts[1] if len(parts) == 2 else ""
    except:
        return ""


# ── App ─────────────────────────────────────────────

class App:
    def __init__(self, scr):
        self.scr = scr
        self.reload_all()

        self.sel = 0
        self.scroll = 0
        self.log = []

        self.mode = "nav"
        self.input_buf = ""

        curses.curs_set(0)
        self.scr.keypad(True)
        self.scr.timeout(100)

    # ── Reload + width calc ─────────────────────────

    def reload_all(self):
        self.pvs = load_pvs()
        self.data = load_data()
        self.compute_widths()

    def compute_widths(self):
        # Add padding between columns
        self.pad = 2

        self.w_pv = min(max((len(pv) for pv in self.pvs), default=10), 35)

        self.w_write = max(
            (len(self.data.get(pv, {}).get("write", "0")) for pv in self.pvs),
            default=5
        )

        self.w_read = min(max(
            (len(self.data.get(pv, {}).get("read", "")) for pv in self.pvs),
            default=8
        ), 25)

        # total table width (important for boundary!)
        self.table_w = (
            self.w_pv + self.pad +
            self.w_write + self.pad +
            self.w_read + self.pad +
            4   # "RO T"
        )

    # ── Entry ────────────────────────────────────

    def entry(self, pv):
        e = self.data.setdefault(pv, {})
        return {
            "write": e.get("write", "0"),
            "read": e.get("read", ""),
            "readonly": pv.endswith("_RBV") or e.get("readonly", False),
            "tested": e.get("tested", False),
        }

    def save(self):
        save_data(self.data)

    # ── Logging ──────────────────────────────────

    def log_line(self, msg):
        ts = datetime.now().strftime("%H:%M:%S")
    
        # 🔥 split multiline safely
        lines = str(msg).splitlines() or [""]
    
        for i, line in enumerate(lines):
            prefix = f"[{ts}] " if i == 0 else " " * 13
            self.log.append(prefix + line)
    
        if len(self.log) > MAX_LOG:
            self.log = self.log[-MAX_LOG:]

    # ── Layout ───────────────────────────────────

    def layout(self):
        H, W = self.scr.getmaxyx()

        # ensure log never invades table
        split = min(self.table_w + 2, W - 20)
        return H, W, split

    # ── Drawing ──────────────────────────────────

    def draw(self):
        self.scr.erase()
        H, W, split = self.layout()
    
        # ── Draw vertical separator ─────────────────────
        for y in range(H):
            try:
                self.scr.addch(y, split, '│')
            except curses.error:
                pass
    
        # ── Header ──────────────────────────────────────
        header = (
            f"{'PV':<{self.w_pv}}{' ' * self.pad}"
            f"{'WRT':<{self.w_write}}{' ' * self.pad}"
            f"{'READ':<{self.w_read}}{' ' * self.pad}"
            f"RO T"
        )
    
        self.scr.addstr(0, 0, header[:split-1].ljust(split-1))
        self.scr.addstr(0, split + 2, "LOG")
    
        # ── PV table (STRICT CLIP) ─────────────────────
        for i in range(H - 2):
            idx = self.scroll + i
            if idx >= len(self.pvs):
                break
    
            pv = self.pvs[idx]
            e = self.entry(pv)
    
            ro = "R" if e["readonly"] else " "
            tested = "✔" if e["tested"] else " "
    
            line = (
                f"{pv:<{self.w_pv}}{' ' * self.pad}"
                f"{e['write']:<{self.w_write}}{' ' * self.pad}"
                f"{e['read']:<{self.w_read}}{' ' * self.pad}"
                f"{ro} {tested}"
            )
    
            # 🔥 HARD CLIP LEFT PANEL
            line = line[:split-1].ljust(split-1)
    
            try:
                if idx == self.sel:
                    self.scr.addstr(i + 1, 0, line, curses.A_REVERSE)
                else:
                    self.scr.addstr(i + 1, 0, line)
            except curses.error:
                pass
    
        # ── LOG panel (STRICT CLIP) ────────────────────
        log_w = W - split - 2
        visible = self.log[-(H - 2):]
    
        for i, line in enumerate(visible):
            clipped = line[:log_w].ljust(log_w)
    
            try:
                self.scr.addstr(i + 1, split + 1, " ")  # gap after divider
                self.scr.addstr(i + 1, split + 2, clipped)
            except curses.error:
                pass
    
        # ── Footer ─────────────────────────────────────
        status = f"[{self.mode}] Enter=test  e=edit  /=search  q=quit"
        self.scr.addstr(H - 1, 0, status[:W - 1])
    
        self.scr.refresh()

    # ── Test ────────────────────────────────────

    def run_test(self):
        pv = self.pvs[self.sel]
        e = self.entry(pv)

        self.log_line(f"── {pv} ──")

        # caget before
        self.log_line(f"▶ caget {pv}")
        ok, out = run_ca(["caget", pv])
        status = "✔" if ok else "✘"
        self.log_line(f"{status} {out}")
        e["read"] = parse_caget(out)

        # caput
        if not e["readonly"]:
            self.log_line(f"▶ caput {pv} {e['write']}")
            ok, out = run_ca(["caput", pv, e["write"]])
            status = "✔" if ok else "✘"
            self.log_line(f"{status} {out}")
        else:
            self.log_line("(read-only, skipping caput)")

        # PROC
        for v in ["0", "1", "0"]:
            self.log_line(f"▶ caput {pv}.PROC {v}")
            ok, out = run_ca(["caput", f"{pv}.PROC", v])
            status = "✔" if ok else "✘"
            self.log_line(f"{status} {out}")

        # caget after
        self.log_line(f"▶ caget {pv}")
        ok, out = run_ca(["caget", pv])
        status = "✔" if ok else "✘"
        self.log_line(f"{status} {out}")
        e["read"] = parse_caget(out)
        self.log_line("=========================================================")

        e["tested"] = True
        self.data[pv] = e

        # 🔥 critical fix
        self.save()
        self.reload_all()

    # ── Input ───────────────────────────────────

    def handle(self, k):
        if self.mode == "nav":
            if k in (curses.KEY_UP, ord("k")):
                self.sel = max(0, self.sel - 1)
            elif k in (curses.KEY_DOWN, ord("j")):
                self.sel = min(len(self.pvs) - 1, self.sel + 1)
            elif k == ord("q"):
                return False
            elif k in (10, 13):
                self.run_test()
            elif k == ord("e"):
                self.mode = "edit"
                self.input_buf = self.entry(self.pvs[self.sel])["write"]
            elif k == ord("/"):
                self.mode = "search"
                self.input_buf = ""

        elif self.mode == "edit":
            if k in (10, 13):
                pv = self.pvs[self.sel]
                self.data[pv]["write"] = self.input_buf
                self.save()
                self.reload_all()
                self.mode = "nav"
            elif k == 27:
                self.mode = "nav"
            elif k in (curses.KEY_BACKSPACE, 127):
                self.input_buf = self.input_buf[:-1]
            elif 32 <= k <= 126:
                self.input_buf += chr(k)

        elif self.mode == "search":
            if k in (10, 13):
                for i, pv in enumerate(self.pvs):
                    if self.input_buf.lower() in pv.lower():
                        self.sel = i
                        break
                self.mode = "nav"
            elif k == 27:
                self.mode = "nav"
            elif k in (curses.KEY_BACKSPACE, 127):
                self.input_buf = self.input_buf[:-1]
            elif 32 <= k <= 126:
                self.input_buf += chr(k)

        # scrolling
        H, _, _ = self.layout()
        if self.sel < self.scroll:
            self.scroll = self.sel
        elif self.sel >= self.scroll + H - 2:
            self.scroll = self.sel - (H - 3)

        return True

    # ── Main ───────────────────────────────────

    def run(self):
        while True:
            self.draw()
            k = self.scr.getch()
            if k == -1:
                continue
            if not self.handle(k):
                break


def main(scr):
    App(scr).run()


if __name__ == "__main__":
    curses.wrapper(main)
