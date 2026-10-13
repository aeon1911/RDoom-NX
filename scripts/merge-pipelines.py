#!/usr/bin/env python3
"""Merges RDoom pipeline lists (rdoom-pipelines-*.bin, written by the engine's
Vulkan backend) into the list the package carries:

    scripts/merge-pipelines.py assets/rdoom-pipelines-seed.bin <list.bin>...

Entries are compared byte for byte; at most 400 are kept (the engine's limit)."""
import struct, sys

MAGIC = 0x4c504452
out, inputs = sys.argv[1], sys.argv[2:]
size, entries, seen = None, [], set()
for path in inputs:
    data = open(path, "rb").read()
    magic, esize, count = struct.unpack("<3I", data[:12])
    if magic != MAGIC or len(data) != 12 + esize * count:
        sys.exit("%s: not a pipeline list" % path)
    if size is None:
        size = esize
    elif esize != size:
        sys.exit("%s: entry size %d, expected %d (lists from different builds)" % (path, esize, size))
    for i in range(count):
        e = data[12 + i * esize:12 + (i + 1) * esize]
        if e not in seen and len(entries) < 400:
            seen.add(e)
            entries.append(e)
with open(out, "wb") as f:
    f.write(struct.pack("<3I", MAGIC, size, len(entries)))
    f.write(b"".join(entries))
print("%s: %d pipelines" % (out, len(entries)))
