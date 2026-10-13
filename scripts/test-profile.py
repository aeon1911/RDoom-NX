#!/usr/bin/env python3
"""Edits a profile on the emulated SD for tests: test-profile.py <id> video.renderer=opengl gameplay.warp=MAP01 ..."""
import json, sys
p = "test-env/ryujinx/sdcard/switch/rdoom/profiles/%s.json" % sys.argv[1]
j = json.load(open(p))
for kv in sys.argv[2:]:
    k, v = kv.split("=", 1)
    a, b = k.split(".") if "." in k else (None, k)
    try:
        v = json.loads(v)
    except ValueError:
        pass
    (j[a] if a else j)[b] = v
json.dump(j, open(p, "w"), indent=2)
print(p, "updated")
