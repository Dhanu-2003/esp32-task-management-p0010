"""Read the ESP32 serial port for a fixed number of seconds and save to a file.

Never run `idf.py monitor` (interactive/endless). Use this instead:
    python scripts/read_log.py --port COM7 --seconds 20 --out boot.log
"""
import argparse
import sys

import serial


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True)
    ap.add_argument("--seconds", type=float, default=20.0)
    ap.add_argument("--out", default="boot.log")
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=1)
    import time
    end = time.time() + args.seconds
    with open(args.out, "w", encoding="utf-8", errors="replace") as f:
        while time.time() < end:
            chunk = ser.read(4096)
            if chunk:
                text = chunk.decode("utf-8", errors="replace")
                f.write(text)
                sys.stdout.write(text)
                sys.stdout.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
