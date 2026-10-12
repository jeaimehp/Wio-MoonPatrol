#!/usr/bin/env python3
"""Read a standard MIDI file into (start_ms, end_ms, note, track) lists.

Just enough of the format for a melody: note on/off, tempo, running
status, and skipping everything else.
"""

import struct
import sys


def read_var(data, i):
    value = 0
    while True:
        b = data[i]
        i += 1
        value = (value << 7) | (b & 0x7F)
        if not b & 0x80:
            return value, i


def parse(path):
    data = open(path, "rb").read()
    assert data[:4] == b"MThd"
    _, fmt, ntracks, division = struct.unpack(">IHHH", data[4:14])
    i = 14
    events = []  # (tick, track, kind, a, b)
    for track in range(ntracks):
        assert data[i:i + 4] == b"MTrk", data[i:i + 4]
        length = struct.unpack(">I", data[i + 4:i + 8])[0]
        j, end = i + 8, i + 8 + length
        tick, status = 0, 0
        while j < end:
            delta, j = read_var(data, j)
            tick += delta
            b = data[j]
            if b == 0xFF:
                kind, j = data[j + 1], j + 2
                n, j = read_var(data, j)
                if kind == 0x51:
                    events.append((tick, track, "tempo", int.from_bytes(data[j:j + 3], "big"), 0))
                elif kind in (0x03, 0x01):
                    events.append((tick, track, "name", data[j:j + n].decode(errors="replace"), 0))
                j += n
                continue
            if b in (0xF0, 0xF7):
                n, j = read_var(data, j + 1)
                j += n
                continue
            if b & 0x80:
                status, j = b, j + 1
            hi = status & 0xF0
            if hi in (0xC0, 0xD0):
                a, j = data[j], j + 1
                events.append((tick, track, "prog" if hi == 0xC0 else "x", a, status & 0x0F))
                continue
            a, v, j = data[j], data[j + 1], j + 2
            if hi == 0x90 and v > 0:
                events.append((tick, track, "on", a, status & 0x0F))
            elif hi in (0x80, 0x90):
                events.append((tick, track, "off", a, status & 0x0F))
        i = end
    events.sort(key=lambda e: e[0])
    # Ticks to ms with tempo changes.
    tempo, last_tick, last_ms = 500000, 0, 0.0
    def to_ms(t):
        return last_ms + (t - last_tick) * tempo / division / 1000
    notes, open_notes, names = [], {}, {}
    for tick, track, kind, a, b in events:
        if kind == "tempo":
            last_ms, last_tick, tempo = to_ms(tick), tick, a
        elif kind == "name":
            names[track] = a
        elif kind == "on":
            open_notes[(track, a)] = to_ms(tick)
        elif kind == "off" and (track, a) in open_notes:
            notes.append((open_notes.pop((track, a)), to_ms(tick), a, track))
    notes.sort()
    return fmt, division, names, notes


if __name__ == "__main__":
    fmt, division, names, notes = parse(sys.argv[1])
    print("format", fmt, "division", division, "names", names)
    by_track = {}
    for n in notes:
        by_track.setdefault(n[3], []).append(n)
    for t, ns in by_track.items():
        pitches = [n[2] for n in ns]
        print(f"track {t}: {len(ns)} notes, pitch {min(pitches)}-{max(pitches)}, "
              f"{ns[0][0]:.0f}-{ns[-1][1]:.0f} ms")
        print("  first:", [(round(n[0]), round(n[1] - n[0]), n[2]) for n in ns[:16]])
