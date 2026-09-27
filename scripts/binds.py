#!/usr/bin/env python3
"""Map HID boot-protocol keyboard + mouse buttons using config/binds.json."""
from __future__ import annotations

import json
import math
from pathlib import Path

# USB HID keyboard usage IDs (boot protocol).
_LETTERS = "abcdefghijklmnopqrstuvwxyz"
HID_USAGE = {ch: 0x04 + i for i, ch in enumerate(_LETTERS)}
HID_USAGE.update({str((i + 1) % 10): 0x1E + i for i in range(10)})
HID_USAGE.update(
    {
        "enter": 0x28,
        "escape": 0x29,
        "backspace": 0x2A,
        "tab": 0x2B,
        "space": 0x2C,
        "minus": 0x2D,
        "equal": 0x2E,
        "lbracket": 0x2F,
        "rbracket": 0x30,
        "backslash": 0x31,
        "semicolon": 0x33,
        "quote": 0x34,
        "grave": 0x35,
        "comma": 0x36,
        "dot": 0x37,
        "slash": 0x38,
        "capslock": 0x39,
        "f1": 0x3A,
        "f2": 0x3B,
        "f3": 0x3C,
        "f4": 0x3D,
        "f5": 0x3E,
        "f6": 0x3F,
        "f7": 0x40,
        "f8": 0x41,
        "f9": 0x42,
        "f10": 0x43,
        "f11": 0x44,
        "f12": 0x45,
        "printscreen": 0x46,
        "home": 0x4A,
        "right": 0x4F,
        "left": 0x50,
        "down": 0x51,
        "up": 0x52,
        "lctrl": 0xE0,
        "lshift": 0xE1,
        "lalt": 0xE2,
        "lgui": 0xE3,
        "rctrl": 0xE4,
        "rshift": 0xE5,
        "ralt": 0xE6,
        "rgui": 0xE7,
    }
)
USAGE_NAME = {v: k for k, v in HID_USAGE.items()}

MOD_BITS = {
    "lctrl": 0x01,
    "lshift": 0x02,
    "lalt": 0x04,
    "lgui": 0x08,
    "rctrl": 0x10,
    "rshift": 0x20,
    "ralt": 0x40,
    "rgui": 0x80,
}

MOUSE_BITS = {
    "left": 1 << 0,
    "right": 1 << 1,
    "middle": 1 << 2,
    "back": 1 << 3,
    "forward": 1 << 4,
}

# Must match m2g::protocol::btn_* on the S3.
PAD_BITS = {
    "a": 1 << 0,
    "b": 1 << 1,
    "x": 1 << 2,
    "y": 1 << 3,
    "l": 1 << 4,
    "r": 1 << 5,
    "zl": 1 << 6,
    "zr": 1 << 7,
    "minus": 1 << 8,
    "plus": 1 << 9,
    "l3": 1 << 10,
    "r3": 1 << 11,
    "home": 1 << 12,
    "capture": 1 << 13,
    "dpad_up": 1 << 14,
    "dpad_down": 1 << 15,
    "dpad_left": 1 << 16,
    "dpad_right": 1 << 17,
}

STICK_AXES = ("up", "down", "left", "right")


class Binds:
    def __init__(self, path: Path) -> None:
        self.path = path
        self.mtime = -1.0
        self.stick = {"up": "w", "down": "s", "left": "a", "right": "d"}
        self.keys: dict[str, str] = {}
        self.mouse: dict[str, str] = {
            "left": "l",
            "right": "r",
            "forward": "zl",
            "back": "zr",
            "middle": "r3",
        }
        self.reload(force=True)

    def reload(self, force: bool = False) -> str | None:
        try:
            mtime = self.path.stat().st_mtime
        except OSError as e:
            return f"cannot read {self.path}: {e}"
        if not force and mtime == self.mtime:
            return None
        try:
            data = json.loads(self.path.read_text())
        except (OSError, json.JSONDecodeError) as e:
            return f"bad binds file {self.path}: {e}"
        stick = data.get("stick") or {}
        keys = data.get("keys") or {}
        mouse = data.get("mouse") or {}
        for axis in STICK_AXES:
            name = str(stick.get(axis, self.stick[axis])).lower()
            if name not in HID_USAGE and name not in MOD_BITS:
                return f"stick.{axis} unknown key {name!r}"
            self.stick[axis] = name
        new_keys: dict[str, str] = {}
        for src, dst in keys.items():
            s, d = str(src).lower(), str(dst).lower()
            if s not in HID_USAGE and s not in MOD_BITS:
                return f"keys: unknown key {s!r}"
            if d not in PAD_BITS:
                return f"keys: unknown pad button {d!r}"
            new_keys[s] = d
        new_mouse: dict[str, str] = {}
        for src, dst in mouse.items():
            s, d = str(src).lower(), str(dst).lower()
            if s not in MOUSE_BITS:
                return f"mouse: unknown button {s!r}"
            if d not in PAD_BITS:
                return f"mouse: unknown pad button {d!r}"
            new_mouse[s] = d
        self.keys = new_keys
        self.mouse = new_mouse
        self.mtime = mtime
        return f"loaded {self.path}"

    def held_names(self, mods: int, keycodes: list[int]) -> set[str]:
        held: set[str] = set()
        for name, bit in MOD_BITS.items():
            if mods & bit:
                held.add(name)
        for code in keycodes:
            if code == 0:
                continue
            name = USAGE_NAME.get(code)
            if name:
                held.add(name)
        return held

    def map(self, mods: int, keycodes: list[int], mouse_btns: int) -> tuple[int, int, int]:
        held = self.held_names(mods, keycodes)
        lx = (1 if self.stick["right"] in held else 0) - (1 if self.stick["left"] in held else 0)
        ly = (1 if self.stick["up"] in held else 0) - (1 if self.stick["down"] in held else 0)
        mag = math.hypot(lx, ly)
        if mag > 1.0:
            lx /= mag
            ly /= mag
        bits = 0
        for name, action in self.keys.items():
            if name in held:
                bits |= PAD_BITS[action]
        for name, action in self.mouse.items():
            if mouse_btns & MOUSE_BITS[name]:
                bits |= PAD_BITS[action]
        return int(round(lx * 1000)), int(round(ly * 1000)), bits
