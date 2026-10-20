#!/usr/bin/env python3
"""One line per perf.log summary window (every 5 s of play).

    python3 -I scripts/perf-summary.py <perf.log>

Columns: t (time of the last frame line before the summary), fps, worst frame
(ms), tic and thinkers per frame (ms), 3D scene CPU (ms), StartSound (ms), GPU
frame / 3D scene / post-processing (ms, timestamps; 0 in Ryujinx), 3D
resolution (%), half-resolution layer effects per frame (their screen area,
largest area), effects left out per frame, sound starts per second, wait for
the previous frame at frame start (ms), fence wait at frame end (ms), malloc
heap in use (MB).
"""
import re
import sys


def num(pattern, text, default=None):
    m = re.search(pattern, text)
    return float(m.group(1)) if m else default


def windows(path):
    cur, last_t = None, 0.0
    for line in open(path, errors='replace'):
        line = line.rstrip('\n')
        if not line.startswith('#'):
            try:
                last_t = float(line.split()[0])
            except (ValueError, IndexError):
                pass
            continue
        if line.startswith('# summary'):
            if cur:
                yield cur
            cur = {'t': last_t, 's': line}
        elif cur is not None:
            cur['s'] += '\n' + line
    if cur:
        yield cur


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    print("     t   fps worst  tic think scene sndst  gpuF  g3d gpost res   half(area  most)  left snd/s waitprev fence  heap")
    for w in windows(sys.argv[1]):
        s = w['s']
        fps = num(r'\(([\d.]+) fps\)', s)
        if fps is None:
            continue
        print("%6.0f %5.1f %5.0f %4.1f %5.1f %5.1f %5.1f %5.1f %4.1f %5.1f %3.0f  %5.1f(%5.2f %5.2f) %5.1f %5.0f %8.1f %5.1f %5.0f" % (
            w['t'], fps, num(r'worst ([\d.]+)', s, 0),
            num(r'per frame: tic ([\d.]+)', s, 0), num(r'thinkers ([\d.]+)\)', s, 0),
            num(r'scene ([\d.]+) \(lists', s, 0), num(r'sndstart ([\d.]+)', s, 0),
            num(r'frame ([\d.]+) ms = 3D', s, 0), num(r'3D scene ([\d.]+) \+', s, 0),
            num(r'post-processing ([\d.]+)', s, 0), num(r'resolution avg ([\d.]+)%', s, 0),
            num(r'half-resolution layer ([\d.]+)', s, 0), num(r'their area ([\d.]+)', s, 0),
            num(r'most ([\d.]+)', s, 0), num(r'effects left out per frame ([\d.]+)', s, 0),
            num(r'sound starts/s ([\d.]+)', s, 0), num(r'frame at frame start ([\d.]+)', s, 0),
            num(r'fence wait at frame end ([\d.]+)', s, 0), num(r'malloc in use ([\d.]+)', s, 0)))


if __name__ == '__main__':
    main()
