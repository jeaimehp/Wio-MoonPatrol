#!/usr/bin/env python3
"""Set the Wio's clock from this Mac.

Sends one line, `T2026-10-12 12:47:03`, at the start of a second, then
prints the NOW line the board answers with and how far it is from the
Mac's clock. Opening the port at 115200 does not reset the board.
"""

import datetime
import os
import sys
import time

import serial

PORT = os.environ.get("WIO_PORT", "/dev/cu.usbmodem124301")


def main():
    with serial.Serial(PORT, 115200, timeout=0.1) as port:
        port.reset_input_buffer()
        # Send just after a second ticks over so the board starts that
        # second fresh.
        time.sleep(1 - time.time() % 1 + 0.02)
        now = datetime.datetime.now()
        line = now.strftime("T%Y-%m-%d %H:%M:%S")
        port.write((line + "\n").encode())
        print(f"sent {line}")

        deadline = time.time() + 3
        buf = b""
        while time.time() < deadline:
            buf += port.read(256)
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                text = raw.decode(errors="replace").strip()
                if text == "CLOCK BAD":
                    print("board rejected the time", file=sys.stderr)
                    return 1
                if text.startswith("NOW "):
                    board = datetime.datetime.strptime(text[4:], "%Y-%m-%d %H:%M:%S")
                    delta = (board - datetime.datetime.now()).total_seconds()
                    print(f"board {text}  ({delta:+.1f} s from the Mac)")
                    return 0
    print("no NOW line from the board", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
