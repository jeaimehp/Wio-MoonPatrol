#!/usr/bin/env python3
"""Copy the PlatformIO binary to the Wio UF2 bootloader.

SAM-BA does not write this board. If a sketch is running, a 1200 baud
open drops it into the bootloader and /Volumes/Arduino appears. If that
volume is already mounted, the file is copied immediately.
"""

import os
import struct
import sys
import time

PORT = os.environ.get("WIO_PORT", "/dev/cu.usbmodem124301")
VOLUME = os.environ.get("WIO_VOLUME", "/Volumes/Arduino")
BIN = os.environ.get(
    "WIO_BIN", ".pio/build/seeed_wio_terminal/firmware.bin"
)
BASE = 0x4000
FAMILY = 0x55114460


def to_uf2(blob):
    if len(blob) % 256:
        blob += b"\xff" * (256 - len(blob) % 256)
    blocks = len(blob) // 256
    out = bytearray()
    for i in range(blocks):
        chunk = blob[i * 256 : (i + 1) * 256]
        header = struct.pack(
            "<IIIIIIII",
            0x0A324655,
            0x9E5D5157,
            0x00002000,
            BASE + i * 256,
            256,
            i,
            blocks,
            FAMILY,
        )
        pad = b"\x00" * (512 - 32 - 256 - 4)
        out += header + chunk + pad + struct.pack("<I", 0x0AB16F30)
    return out


def touch_bootloader():
    import serial

    try:
        ser = serial.Serial(PORT, 1200)
        ser.close()
    except Exception as exc:
        print(f"1200 baud touch skipped: {exc}")


def wait_volume(seconds=20):
    deadline = time.time() + seconds
    while time.time() < deadline:
        if os.path.isdir(VOLUME):
            return True
        time.sleep(0.4)
    return False


def main():
    if not os.path.isfile(BIN):
        print(f"missing {BIN}", file=sys.stderr)
        return 1
    if not os.path.isdir(VOLUME):
        print(f"touching {PORT}")
        touch_bootloader()
        if not wait_volume():
            print(f"{VOLUME} did not appear", file=sys.stderr)
            return 1
    blob = open(BIN, "rb").read()
    path = os.path.join(VOLUME, "moonpatrol.uf2")
    # The volume shows up a moment before macOS lets us write to it.
    for attempt in range(15):
        try:
            open(path, "wb").write(to_uf2(blob))
            break
        except PermissionError:
            if attempt == 14:
                raise
            time.sleep(0.4)
    print(f"wrote {path} ({len(blob)} bytes at {BASE:#x})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
