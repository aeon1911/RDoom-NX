#!/usr/bin/env bash
# Frame pacing at the shipped settings (vsync on, vid_maxfps 60, Ryujinx vsync "Switch" 60 Hz).
#   scripts/pacing-ryujinx.sh <profile> <label> [map] [seconds-in-level]
# Logs every frame (nx_perflog 2, first ~3000 frames) and reports the frame-time
# distribution of the level phase: mean, standard deviation, p50/p95/p99, worst,
# and how many frames missed the 16.7 ms display interval.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
PROF=$1; LABEL=$2; MAP=${3:-MAP01}; SECS=${4:-60}
EV="$ROOT/evidence/pacing/$LABEL"
SD="$ROOT/test-env/ryujinx/sdcard/switch/rdoom"
mkdir -p "$EV"; rm -f "$EV/image-check.txt"
python3 "$ROOT/scripts/test-profile.py" "$PROF" video.renderer=vulkan video.vsync=true video.fps_limit=60 \
	gameplay.warp="$MAP" gameplay.skill=3 'cvars={"nx_perflog":"2"}' > /dev/null
rm -f "$SD/perf.log"
# automap on/off once and a black-image check: see bench-ryujinx.sh
TOGGLE_AT=${TOGGLE_AT:-20}	# seconds until the level is surely on screen (big mods load longer)
printf 'wait 13\ntap PLUS 0.2\nwait %s\ntap MINUS\nwait 1\ntap MINUS\nwait 2\nshot check\nstick L 0 -1 3\nstick R 0.5 0 1\nwait %s\nshot level\n' "$TOGGLE_AT" "$SECS" > "$ROOT/build/vpad-pacing.txt"
PROFILE="$PROF" timeout $((SECS + TOGGLE_AT + 140)) "$ROOT/scripts/ryujinx-test.sh" "pacing-$LABEL" "$ROOT/build/vpad-pacing.txt" $((SECS + TOGGLE_AT + 45)) > /dev/null 2>&1
python3 "$ROOT/scripts/test-profile.py" "$PROF" 'cvars={}' > /dev/null
cp "$SD/perf.log" "$EV/perf.log" 2>/dev/null
cp "$ROOT/evidence/ryujinx/pacing-$LABEL/"{engine-session.txt,nro.sha256,check.png,level.png} "$EV/" 2>/dev/null
for shot in check level; do
	b=$(magick "$EV/$shot.png" -crop 2400x1300+60+40 -format '%[fx:mean]' info: 2>/dev/null || echo missing)
	echo "$shot.png mean brightness: $b" >> "$EV/image-check.txt"
	case "$b" in missing|0) echo "WARNING: $shot.png is black or missing" ;; esac
done
python3 - "$EV/perf.log" "$LABEL" <<'PY' | tee "$EV/result.txt"
import statistics, sys
ft = []
for l in open(sys.argv[1]):
    if l.startswith("#"):
        continue
    p = l.split("|")
    try:
        frame = float(p[0].split()[1])
        items = int(p[3].split()[0])
    except (IndexError, ValueError):
        continue
    if items > 0:                       # a level is being drawn
        ft.append(frame)
ft = ft[60:]                            # skip the first second of the level
# (automap frames draw no 3D items, so they are not counted)
if len(ft) < 100:
    print("%s: not enough level frames (%d)" % (sys.argv[2], len(ft))); sys.exit()
s = sorted(ft)
q = lambda f: s[min(len(s) - 1, int(f * len(s)))]
late = sum(1 for x in ft if x > 16.7 * 1.5)
print("%s: %d frames, mean %.2f ms (%.1f fps), stdev %.2f ms, p50 %.2f, p95 %.2f, p99 %.2f, worst %.1f ms, frames > 25 ms: %d (%.2f%%)" % (
    sys.argv[2], len(ft), statistics.mean(ft), 1000 / statistics.mean(ft), statistics.pstdev(ft), q(.5), q(.95), q(.99), s[-1], late, 100.0 * late / len(ft)))
PY
