#!/usr/bin/env bash
# Reproducible in-emulator benchmark of the packaged RDoom.nro.
#   scripts/bench-ryujinx.sh <profile> <label> [map] [seconds-in-level]
# Uncapped (profile vsync off, vid_maxfps 0; Ryujinx vsync "Unbounded") so the
# numbers show cost per frame instead of the 60 Hz ceiling. Collects perf.log
# summaries of the level phase (thinkers > 0) into evidence/benchmarks/<label>/.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
PROF=$1; LABEL=$2; MAP=${3:-MAP01}; SECS=${4:-90}
EV="$ROOT/evidence/benchmarks/$LABEL"
CFG="$ROOT/test-env/ryujinx/Config.json"
SD="$ROOT/test-env/ryujinx/sdcard/switch/rdoom"
mkdir -p "$EV"; rm -f "$EV/image-check.txt"
python3 "$ROOT/scripts/test-profile.py" "$PROF" video.renderer=vulkan video.vsync=false video.fps_limit=0 \
	gameplay.warp="$MAP" gameplay.skill=3 > /dev/null
python3 - "$CFG" 1 <<'PY'
import json, sys
c = json.load(open(sys.argv[1])); c["vsync_mode"] = int(sys.argv[2]); json.dump(c, open(sys.argv[1], "w"), indent=2)
PY
rm -f "$SD/perf.log"
# Toggle the automap on and off once after the level appears, then check the
# captured image is not black (Ryujinx can show a black view after a direct warp).
# TOGGLE_AT: seconds after start until the level is surely on screen (big mods load longer)
TOGGLE_AT=${TOGGLE_AT:-20}
printf 'wait 13\ntap PLUS 0.2\nwait %s\ntap MINUS\nwait 1\ntap MINUS\nwait 2\nshot check\nwait %s\nshot level\n' "$TOGGLE_AT" "$((SECS + 36 - TOGGLE_AT))" > "$ROOT/build/vpad-bench.txt"
PROFILE="$PROF" timeout $((SECS + 120)) "$ROOT/scripts/ryujinx-test.sh" "bench-$LABEL" "$ROOT/build/vpad-bench.txt" $((SECS + 70)) > /dev/null 2>&1
python3 - "$CFG" 0 <<'PY'
import json, sys
c = json.load(open(sys.argv[1])); c["vsync_mode"] = int(sys.argv[2]); json.dump(c, open(sys.argv[1], "w"), indent=2)
PY
cp "$SD/perf.log" "$EV/perf.log" 2>/dev/null
cp "$ROOT/evidence/ryujinx/bench-$LABEL/"{engine-session.txt,last-launch.txt,check.png,level.png,nro.sha256} "$EV/" 2>/dev/null
for shot in check level; do
	b=$(magick "$EV/$shot.png" -crop 2400x1300+60+40 -format '%[fx:mean]' info: 2>/dev/null || echo missing)
	echo "$shot.png mean brightness: $b" >> "$EV/image-check.txt"
	case "$b" in missing|0) echo "WARNING: $shot.png is black or missing - the numbers below may not be valid" ;; esac
done
python3 - "$EV/perf.log" "$LABEL" <<'PY' | tee "$EV/result.txt"
import re, sys
lines = open(sys.argv[1]).read().splitlines() if len(sys.argv) > 1 else []
sums, cur = [], None
for l in lines:
    m = re.match(r"# summary: (\d+) frames, avg ([\d.]+) ms \(([\d.]+) fps\), worst ([\d.]+) ms", l)
    if m:
        cur = {"frames": int(m[1]), "avg": float(m[2]), "fps": float(m[3]), "worst": float(m[4])}
        sums.append(cur)
    m = re.search(r"per frame: tic ([\d.]+).*display ([\d.]+)", l)
    if m and cur: cur["tic"], cur["display"] = float(m[1]), float(m[2])
    m = re.search(r"per tic: ([\d.]+) ms, (\d+) thinkers", l)
    if m and cur: cur["thinkers"] = int(m[2])
lvl = [s for s in sums if s.get("thinkers", 0) > 0][1:]   # skip the first (map start) window
if not lvl:
    print("%s: no level samples" % sys.argv[2]); sys.exit()
n = sum(s["frames"] for s in lvl)
avg = sum(s["avg"] * s["frames"] for s in lvl) / n
print("%s: %d level windows, %d frames, avg %.2f ms (%.1f fps), worst %.1f ms, tic %.2f ms/frame, display %.2f ms/frame" % (
    sys.argv[2], len(lvl), n, avg, 1000 / avg, max(s["worst"] for s in lvl),
    sum(s.get("tic", 0) for s in lvl) / len(lvl), sum(s.get("display", 0) for s in lvl) / len(lvl)))
PY
