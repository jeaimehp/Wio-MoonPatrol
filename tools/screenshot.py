#!/usr/bin/env python3
"""Save the Wio's screen as a PNG.

    python3 tools/screenshot.py docs/title.png [I-commands ...]

Optional arguments after the file name are sent first as serial lines
(for example `IC` to press the mode button), with a pause after each, so
a screenshot can be staged. The board answers `D` with its panel
contents read back over SPI.
"""

import os
import sys
import time

import serial
from PIL import Image

PORT = os.environ.get("WIO_PORT", "/dev/cu.usbmodem124301")
W, H = 320, 240


def main(path, commands):
    with serial.Serial(PORT, 115200, timeout=2) as port:
        for cmd in commands:
            if cmd.startswith("wait"):
                time.sleep(float(cmd[4:]))
                continue
            port.write((cmd + "\n").encode())
            time.sleep(0.6)
        port.reset_input_buffer()
        port.write(b"D\n")
        while True:
            line = port.readline()
            if not line:
                print("no DUMP from the board", file=sys.stderr)
                return 1
            if line.strip() == b"DUMP 320 240":
                break
        raw = port.read(W * H * 2)
        if len(raw) != W * H * 2:
            print(f"short dump: {len(raw)} bytes", file=sys.stderr)
            return 1
    img = Image.new("RGB", (W, H))
    px = img.load()
    for i in range(W * H):
        # readRect hands back each pixel byte-swapped ("for compatibility
        # with pushRect()"), so undo that here.
        v = raw[2 * i] << 8 | raw[2 * i + 1]
        r, g, b = v >> 11, (v >> 5) & 0x3F, v & 0x1F
        px[i % W, i // W] = (r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2)
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    img.save(path)
    print(f"saved {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1], sys.argv[2:]))
