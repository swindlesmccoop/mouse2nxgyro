#!/usr/bin/env python3
"""Copy mouse lines from the Feather USB-C serial onto the S3 Micro-USB UART.

  python3 scripts/relay.py
  python3 scripts/relay.py --s3 /dev/ttyUSB0 --feather /dev/ttyACM0

Feather lines that start with 'M' are written to the S3 (UART0 RX). Everything
the S3 prints (ESP_LOG) is shown here — this *is* the serial monitor for the
S3; do not also run idf.py monitor (it would steal the port).

Requires: pyserial  (pip install --user pyserial)
"""
from __future__ import annotations

import argparse
import sys
import threading
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.stderr.write("need pyserial:  pip install --user pyserial\n")
    sys.exit(1)

BAUD = 115200

# CP2102N on the ESP32-S3-USB-OTG Micro-USB bridge
S3_VID_PID = {(0x10C4, 0xEA60)}
# RP2040 USB-C CDC (Raspberry Pi VID) and Adafruit VID
FEATHER_VID_PID = {
    (0x2E8A, 0x0005),
    (0x2E8A, 0x000A),
    (0x2E8A, 0x0003),
}


def classify(port: list_ports.ListPortInfo) -> str | None:
    vid, pid = port.vid, port.pid
    if vid is None or pid is None:
        return None
    pair = (vid, pid)
    if pair in S3_VID_PID:
        return "s3"
    if pair in FEATHER_VID_PID or vid == 0x239A:
        return "feather"
    return None


def autodetect() -> tuple[str, str]:
    s3 = []
    feather = []
    for p in list_ports.comports():
        kind = classify(p)
        if kind == "s3":
            s3.append(p.device)
        elif kind == "feather":
            feather.append(p.device)
    if len(s3) != 1 or len(feather) != 1:
        print("ports found:", file=sys.stderr)
        for p in list_ports.comports():
            print(f"  {p.device:12} vid={p.vid:04x} pid={p.pid:04x}  {p.description}",
                  file=sys.stderr)
        sys.stderr.write(
            "need exactly one CP2102 (S3) and one RP2040/Adafruit CDC (Feather).\n"
            "pass --s3 and --feather explicitly.\n"
        )
        sys.exit(1)
    return s3[0], feather[0]


def open_serial(path: str) -> serial.Serial:
    # dsrdtr=False: don't toggle DTR every open in a way that fights the CP2102
    # auto-reset more than once. First open may still reset the S3; that is fine.
    return serial.Serial(path, BAUD, timeout=0.05, dsrdtr=False, rtscts=False)


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

    print(f"S3      {s3_path} @ {BAUD}", flush=True)
    print(f"Feather {feather_path} @ {BAUD}", flush=True)
    print("forwarding M-lines Feather -> S3; S3 logs below. Ctrl-C to stop.\n", flush=True)

    s3 = open_serial(s3_path)
    feather = open_serial(feather_path)
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
                    # Feather debug comments ("# ...")
                    try:
                        text = line.decode("utf-8", "replace")
                    except Exception:
                        text = repr(line)
                    print(f"[feather] {text}", flush=True)

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
                try:
                    text = line.decode("utf-8", "replace")
                except Exception:
                    text = repr(line)
                print(text, flush=True)

    def heartbeat() -> None:
        while not stop.wait(5.0):
            age = time.monotonic() - stats["m_last"]
            print(
                f"[relay] {stats['m_lines']} M-lines forwarded, "
                f"last {age:.1f}s ago",
                flush=True,
            )

    threads = [
        threading.Thread(target=from_feather, daemon=True),
        threading.Thread(target=from_s3, daemon=True),
        threading.Thread(target=heartbeat, daemon=True),
    ]
    for t in threads:
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
