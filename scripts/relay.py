#!/usr/bin/env python3
"""Copy mouse lines from the Feather USB-C serial onto the S3 Micro-USB UART.

Linux only. Uses the Python stdlib (termios + sysfs) — no pyserial, no pip.

  ./scripts/dev.sh relay
  ./scripts/dev.sh relay --s3 /dev/ttyUSB0 --feather /dev/ttyACM0

Feather lines that start with 'M' are written to the S3 (UART0 RX). Everything
the S3 prints (ESP_LOG) is shown here — this *is* the serial monitor for the
S3; do not also run idf.py monitor (it would steal the port).
"""
from __future__ import annotations

import argparse
import fcntl
import glob
import os
import select
import sys
import termios
import threading
import time
from pathlib import Path

BAUD = 115200

# CP2102N on the ESP32-S3-USB-OTG Micro-USB bridge
S3_VID_PID = {(0x10C4, 0xEA60)}
# RP2040 USB-C CDC (Raspberry Pi VID) and Adafruit / Pico-SDK TinyUSB VID
FEATHER_VID_PID = {
    (0x2E8A, 0x0005),
    (0x2E8A, 0x000A),
    (0x2E8A, 0x0003),
    (0xCAFE, 0x4001),
}


def tty_vid_pid(dev: str) -> tuple[int, int] | None:
    name = os.path.basename(dev)
    start = Path(f"/sys/class/tty/{name}/device")
    if not start.exists():
        return None
    p = start.resolve()
    for _ in range(10):
        vendor = p / "idVendor"
        product = p / "idProduct"
        if vendor.is_file() and product.is_file():
            return int(vendor.read_text().strip(), 16), int(product.read_text().strip(), 16)
        if p.parent == p:
            break
        p = p.parent
    return None


def classify(dev: str) -> str | None:
    pair = tty_vid_pid(dev)
    if pair is None:
        return None
    vid, pid = pair
    if pair in S3_VID_PID:
        return "s3"
    if pair in FEATHER_VID_PID or vid in (0x239A, 0xCAFE):
        return "feather"
    return None


def candidates() -> list[str]:
    found: list[str] = []
    for pat in ("/dev/ttyUSB*", "/dev/ttyACM*"):
        found.extend(sorted(glob.glob(pat)))
    return found


def autodetect() -> tuple[str, str]:
    s3: list[str] = []
    feather: list[str] = []
    ports = candidates()
    for dev in ports:
        kind = classify(dev)
        if kind == "s3":
            s3.append(dev)
        elif kind == "feather":
            feather.append(dev)
    if len(s3) != 1 or len(feather) != 1:
        print("ports found:", file=sys.stderr)
        if not ports:
            print("  (none under /dev/ttyUSB* or /dev/ttyACM*)", file=sys.stderr)
        for dev in ports:
            pair = tty_vid_pid(dev)
            if pair is None:
                print(f"  {dev:12} vid/pid unknown", file=sys.stderr)
            else:
                print(f"  {dev:12} vid={pair[0]:04x} pid={pair[1]:04x}", file=sys.stderr)
        sys.stderr.write(
            "need exactly one CP2102 (S3) and one RP2040/Adafruit CDC (Feather).\n"
            "pass --s3 and --feather explicitly.\n"
        )
        sys.exit(1)
    return s3[0], feather[0]


class SerialPort:
    def __init__(self, path: str) -> None:
        self.path = path
        self.fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        attrs = termios.tcgetattr(self.fd)
        attrs[0] = 0
        attrs[1] = 0
        attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL | termios.B115200
        attrs[3] = 0
        # Python < 3.13 has no cfsetispeed; ispeed/ospeed are list slots 4 and 5.
        attrs[4] = termios.B115200
        attrs[5] = termios.B115200
        attrs[6][termios.VMIN] = 0
        attrs[6][termios.VTIME] = 0
        termios.tcsetattr(self.fd, termios.TCSANOW, attrs)
        fl = fcntl.fcntl(self.fd, fcntl.F_GETFL)
        fcntl.fcntl(self.fd, fcntl.F_SETFL, fl & ~os.O_NONBLOCK)

    def read(self, n: int = 256) -> bytes:
        r, _, _ = select.select([self.fd], [], [], 0.05)
        if not r:
            return b""
        try:
            data = os.read(self.fd, n)
        except OSError:
            return b""
        return data or b""

    def write(self, data: bytes) -> None:
        view = memoryview(data)
        while view:
            n = os.write(self.fd, view)
            view = view[n:]

    def close(self) -> None:
        os.close(self.fd)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--s3", help="S3 Micro-USB UART (CP2102), e.g. /dev/ttyUSB0")
    ap.add_argument("--feather", help="Feather USB-C CDC, e.g. /dev/ttyACM0")
    args = ap.parse_args()

    s3_path, feather_path = (args.s3, args.feather) if args.s3 and args.feather else autodetect()
    if args.s3:
        s3_path = args.s3
    if args.feather:
        feather_path = args.feather

    def describe(path: str) -> str:
        pair = tty_vid_pid(path)
        if pair is None:
            return path
        return f"{path}  {pair[0]:04x}:{pair[1]:04x}"

    print(f"S3      {describe(s3_path)} @ {BAUD}", flush=True)
    print(f"Feather {describe(feather_path)} @ {BAUD}", flush=True)
    print(
        "forwarding M-lines Feather -> S3; S3 logs below. Ctrl-C to stop.\n"
        "Working = [relay] count goes up when you move the mouse, S3 says "
        "'mouse: mounted', yellow LED on. If you plugged a cable after this "
        "started, Ctrl-C and run again.\n",
        flush=True,
    )

    s3 = SerialPort(s3_path)
    feather = SerialPort(feather_path)
    stop = threading.Event()
    write_lock = threading.Lock()
    stats = {"m_lines": 0, "m_last": time.monotonic()}

    def from_feather() -> None:
        buf = b""
        while not stop.is_set():
            chunk = feather.read(256)
            if not chunk:
                continue
            buf += chunk
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                line = line.replace(b"\r", b"")
                if not line:
                    continue
                if line.startswith(b"M"):
                    with write_lock:
                        s3.write(line + b"\n")
                    stats["m_lines"] += 1
                    stats["m_last"] = time.monotonic()
                else:
                    print(f"[feather] {line.decode('utf-8', 'replace')}", flush=True)

    def from_s3() -> None:
        buf = b""
        while not stop.is_set():
            chunk = s3.read(256)
            if not chunk:
                continue
            buf += chunk
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                line = line.replace(b"\r", b"")
                print(line.decode("utf-8", "replace"), flush=True)

    def heartbeat() -> None:
        while not stop.wait(5.0):
            n = stats["m_lines"]
            age = time.monotonic() - stats["m_last"]
            if n == 0:
                print(
                    "[relay] no mouse packets yet — mouse in Feather USB-A? "
                    "If you plugged it after start, Ctrl-C and rerun.",
                    flush=True,
                )
            else:
                print(
                    f"[relay] ok: {n} M-lines forwarded, last packet {age:.1f}s ago",
                    flush=True,
                )

    for t in (
        threading.Thread(target=from_feather, daemon=True),
        threading.Thread(target=from_s3, daemon=True),
        threading.Thread(target=heartbeat, daemon=True),
    ):
        t.start()
    try:
        while True:
            time.sleep(0.5)
    except KeyboardInterrupt:
        print("\n[relay] stopping", flush=True)
    finally:
        stop.set()
        s3.close()
        feather.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
