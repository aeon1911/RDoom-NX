#!/usr/bin/env python3
"""Deterministic zip: mkzip.py <out.zip> <base dir> <entry>...  (entries relative to base)."""
import os
import sys
import zipfile

out, base, entries = sys.argv[1], sys.argv[2], sys.argv[3:]
files = []
for e in entries:
    p = os.path.join(base, e)
    if os.path.isdir(p):
        for dp, dn, fn in os.walk(p):
            dn.sort()
            for f in sorted(fn):
                files.append(os.path.relpath(os.path.join(dp, f), base))
    else:
        files.append(e)
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for rel in files:
        info = zipfile.ZipInfo(rel.replace(os.sep, "/"), date_time=(2026, 10, 11, 0, 0, 0))
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = 0o644 << 16
        with open(os.path.join(base, rel), "rb") as f:
            z.writestr(info, f.read())
print("%s: %d files" % (out, len(files)))
