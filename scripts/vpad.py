#!/usr/bin/env python3
"""Virtual game controller for emulator tests (Linux uinput, no extra modules).

Creates an evdev gamepad with the identity of an Xbox One Wireless Controller
(bus 0x05, 045e:02e0, version 0x0903), the controller that the test Ryujinx
configuration maps to Player 1. Inputs therefore travel the real path:
evdev -> SDL3 in Ryujinx -> emulated HID -> libnx / SDL2 inside RDoom.nro.

    python3 -I scripts/vpad.py <script.txt> [--log out.txt]

Script lines (one action each, '#' comments):
    wait <seconds>
    tap <button> [hold_seconds]       buttons in Switch names: A B X Y L R ZL ZR PLUS MINUS UP DOWN LEFT RIGHT LSTICK RSTICK
    hold <button> / release <button>
    stick <L|R> <x> <y> [seconds]     x,y in -1..1; with seconds, returns to centre afterwards
    mark <text>                       written to the log with a timestamp (for screenshots)
    shot <name>                       captures ONLY the Ryujinx window running RDoom into $SHOT_DIR/<name>.png
"""
import ctypes
import fcntl
import os
import struct
import subprocess
import sys
import time

EV_SYN, EV_KEY, EV_ABS = 0x00, 0x01, 0x03
SYN_REPORT = 0
BTN = {"SOUTH": 0x130, "EAST": 0x131, "NORTH": 0x133, "WEST": 0x134, "TL": 0x136, "TR": 0x137,
       "SELECT": 0x13a, "START": 0x13b, "MODE": 0x13c, "THUMBL": 0x13d, "THUMBR": 0x13e}
ABS_X, ABS_Y, ABS_Z, ABS_RX, ABS_RY, ABS_RZ, ABS_HAT0X, ABS_HAT0Y = 0, 1, 2, 3, 4, 5, 0x10, 0x11
UI_SET_EVBIT, UI_SET_KEYBIT, UI_SET_ABSBIT = 0x40045564, 0x40045565, 0x40045567
UI_DEV_CREATE, UI_DEV_DESTROY = 0x5501, 0x5502

# Switch button -> Xbox control, following the test profile's mapping
# (Ryujinx: button_a <- B, button_b <- A, button_x <- Y, button_y <- X).
SWITCH = {
    "A": ("key", "EAST"), "B": ("key", "SOUTH"), "X": ("key", "WEST"), "Y": ("key", "NORTH"),
    "L": ("key", "TL"), "R": ("key", "TR"), "PLUS": ("key", "START"), "MINUS": ("key", "SELECT"),
    "LSTICK": ("key", "THUMBL"), "RSTICK": ("key", "THUMBR"),
    "ZL": ("trig", ABS_Z), "ZR": ("trig", ABS_RZ),
    "UP": ("hat", (ABS_HAT0Y, -1)), "DOWN": ("hat", (ABS_HAT0Y, 1)),
    "LEFT": ("hat", (ABS_HAT0X, -1)), "RIGHT": ("hat", (ABS_HAT0X, 1)),
}


class VirtualPad:
    def __init__(self):
        self.fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
        fcntl.ioctl(self.fd, UI_SET_EVBIT, EV_KEY)
        fcntl.ioctl(self.fd, UI_SET_EVBIT, EV_ABS)
        for code in sorted(BTN.values()):
            fcntl.ioctl(self.fd, UI_SET_KEYBIT, code)
        absmax = [0] * 64
        absmin = [0] * 64
        absflat = [0] * 64
        for a in (ABS_X, ABS_Y, ABS_RX, ABS_RY):
            fcntl.ioctl(self.fd, UI_SET_ABSBIT, a)
            absmin[a], absmax[a], absflat[a] = -32768, 32767, 128
        for a in (ABS_Z, ABS_RZ):
            fcntl.ioctl(self.fd, UI_SET_ABSBIT, a)
            absmin[a], absmax[a] = 0, 1023
        for a in (ABS_HAT0X, ABS_HAT0Y):
            fcntl.ioctl(self.fd, UI_SET_ABSBIT, a)
            absmin[a], absmax[a] = -1, 1
        name = b"Xbox One Wireless Controller"
        dev = struct.pack("80sHHHHi", name, 0x05, 0x045E, 0x02E0, 0x0903, 0)
        dev += struct.pack("64i", *absmax) + struct.pack("64i", *absmin)
        dev += struct.pack("64i", *([0] * 64)) + struct.pack("64i", *absflat)
        os.write(self.fd, dev)
        fcntl.ioctl(self.fd, UI_DEV_CREATE)
        time.sleep(1.0)

    def emit(self, etype, code, value):
        t = time.time()
        os.write(self.fd, struct.pack("llHHi", int(t), int((t % 1) * 1e6), etype, code, value))

    def sync(self):
        self.emit(EV_SYN, SYN_REPORT, 0)

    def set(self, button, down):
        kind, what = SWITCH[button]
        if kind == "key":
            self.emit(EV_KEY, BTN[what], 1 if down else 0)
        elif kind == "trig":
            self.emit(EV_ABS, what, 1023 if down else 0)
        else:
            axis, val = what
            self.emit(EV_ABS, axis, val if down else 0)
        self.sync()

    def stick(self, side, x, y):
        ax, ay = (ABS_X, ABS_Y) if side == "L" else (ABS_RX, ABS_RY)
        self.emit(EV_ABS, ax, int(max(-1, min(1, x)) * 32767))
        self.emit(EV_ABS, ay, int(max(-1, min(1, y)) * 32767))
        self.sync()

    def close(self):
        fcntl.ioctl(self.fd, UI_DEV_DESTROY)
        os.close(self.fd)


def find_emulator_window():
    """X11 id of the Ryujinx window running RDoom (title "Ryujinx ... - RDoom ..."), or None.
    Only this window is ever captured, so nothing else on the desktop ends up in evidence."""
    try:
        from Xlib import display
    except ImportError:
        return None
    d = display.Display()
    found = []

    def walk(w):
        try:
            name = w.get_wm_name()
            if isinstance(name, bytes):
                name = name.decode("utf-8", "replace")
            if name and name.startswith("Ryujinx") and os.environ.get("SHOT_TITLE", "RDoom") in name:
                found.append(w.id)
            for c in w.query_tree().children:
                walk(c)
        except Exception:
            pass

    walk(d.screen().root)
    d.close()
    return found[0] if found else None


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    lines = open(sys.argv[1]).read().splitlines()
    log = open(sys.argv[sys.argv.index("--log") + 1], "a") if "--log" in sys.argv else sys.stdout
    shot_dir = os.environ.get("SHOT_DIR", ".")
    pad = VirtualPad()
    t0 = time.time()

    def note(msg):
        log.write("[%7.2f] %s\n" % (time.time() - t0, msg))
        log.flush()

    note("virtual controller created")
    try:
        for raw in lines:
            line = raw.split("#", 1)[0].strip()
            if not line:
                continue
            p = line.split()
            cmd = p[0].lower()
            if cmd == "wait":
                time.sleep(float(p[1]))
            elif cmd == "tap":
                pad.set(p[1], True)
                time.sleep(float(p[2]) if len(p) > 2 else 0.12)
                pad.set(p[1], False)
                time.sleep(0.15)
                note("tap " + p[1])
            elif cmd == "hold":
                pad.set(p[1], True)
                note("hold " + p[1])
            elif cmd == "release":
                pad.set(p[1], False)
                note("release " + p[1])
            elif cmd == "stick":
                pad.stick(p[1].upper(), float(p[2]), float(p[3]))
                note("stick %s %s %s" % (p[1], p[2], p[3]))
                if len(p) > 4:
                    time.sleep(float(p[4]))
                    pad.stick(p[1].upper(), 0, 0)
            elif cmd == "mark":
                note("mark " + " ".join(p[1:]))
            elif cmd == "shot":
                out = os.path.join(shot_dir, p[1] + ".png")
                wid = find_emulator_window()
                if wid is None:
                    note("screenshot skipped: no Ryujinx/RDoom window (never captures other windows)")
                else:
                    # ImageMagick 7 sometimes rejects the legacy "import -silent" form
                    # ("missing an image filename"); try the other spellings, all
                    # of them limited to this one window.
                    for cmd in (["import", "-window", hex(wid), out],
                                ["magick", "import", "-window", hex(wid), out],
                                ["import", "-silent", "-window", hex(wid), out]):
                        r = subprocess.run(cmd, check=False, stdout=subprocess.DEVNULL,
                                           stderr=subprocess.PIPE, timeout=30)
                        if r.returncode == 0 and os.path.exists(out) and os.path.getsize(out) > 0:
                            break
                    if r.returncode != 0 or not os.path.exists(out):
                        note("screenshot FAILED (window %s): %s" % (hex(wid), r.stderr.decode(errors="replace").strip()[:200]))
                    else:
                        note("screenshot %s (window %s)" % (out, hex(wid)))
            else:
                note("unknown command: " + line)
    finally:
        pad.close()
        note("virtual controller removed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
