#!/usr/bin/env bash
# Runs the packaged RDoom.nro in Ryujinx with an isolated data directory
# (test-env/ryujinx: copy of the user's Config.json, keys symlinked; the user's
# own Ryujinx folder is never written).
#
#   scripts/ryujinx-test.sh <run name> <vpad script|-> [seconds]
#
# - installs dist/RDoom-NX/switch into the emulated SD card
# - IWADs: Freedoom (free) + optional local test files listed in
#   test-env/local-content.txt ("iwad <path>" / "mod <path>" lines; symlinked,
#   never copied into the project or the package)
# - drives the virtual controller (scripts/vpad.py) and takes screenshots
# - collects Ryujinx output, RDoom logs and screenshots in evidence/ryujinx/<run>/
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
RUN=${1:?run name}
SCRIPT=${2:--}
SECS=${3:-90}
DATA="$ROOT/test-env/ryujinx"
SD="$DATA/sdcard/switch/rdoom"
EV="$ROOT/evidence/ryujinx/$RUN"
mkdir -p "$EV" "$SD"

# install the package (user folders such as profiles/ and saves/ are kept, so
# persistence can be tested across runs; RESET=1 starts from scratch)
if [ "${RESET:-0}" = 1 ]; then
	rm -rf "$SD"
	mkdir -p "$SD"
fi
cp -r "$ROOT/dist/RDoom-NX/switch/rdoom/." "$SD/"
mkdir -p "$SD/iwads" "$SD/mods"
for f in freedoom1.wad freedoom2.wad; do
	[ -f "$SD/iwads/$f" ] || cp "$ROOT/third_party/freedoom/freedoom-0.13.0/$f" "$SD/iwads/"
done
if [ -f "$ROOT/test-env/local-content.txt" ]; then
	while read -r kind path; do
		[ -z "${kind:-}" ] && continue
		case "$kind" in \#*) continue ;; esac
		dest="$SD/${kind}s/$(basename "$path")"
		[ -e "$dest" ] || ln -s "$path" "$dest"
	done < "$ROOT/test-env/local-content.txt"
fi

# PROFILE=<id>: make that profile the active one (the launcher remembers the
# last profile used, so tests must not rely on L/R presses from a known state)
if [ -n "${PROFILE:-}" ]; then
	python3 - "$SD/config/launcher.json" "$PROFILE" <<'PY'
import json, os, sys
p, prof = sys.argv[1], sys.argv[2]
j = json.load(open(p)) if os.path.exists(p) else {"format": "rdoom-launcher", "version": 1}
j["last_profile"] = prof
json.dump(j, open(p, "w"), indent=2)
PY
fi

NRO="$SD/RDoom.nro"
sha256sum "$NRO" > "$EV/nro.sha256"
date -u +%Y-%m-%dT%H:%M:%SZ > "$EV/started.txt"
echo "ryujinx binary: $(readlink -f "$(command -v ryujinx)") (package $(pacman -Q ryujinx 2>/dev/null))" > "$EV/environment.txt"

timeout "$SECS" ryujinx --root-data-dir "$DATA" --handheld-mode --hide-updates ${RYU_BACKEND:+--graphics-backend "$RYU_BACKEND"} "$NRO" > "$EV/ryujinx.log" 2>&1 &
RPID=$!
if [ "$SCRIPT" != "-" ]; then
	SHOT_DIR="$EV" python3 -I "$ROOT/scripts/vpad.py" "$SCRIPT" --log "$EV/input.log"
fi
wait $RPID
echo "ryujinx exit: $?" >> "$EV/environment.txt"
cp "$SD/logs/"*.log "$SD/logs/"*.txt "$EV/" 2>/dev/null
cp -r "$SD/profiles" "$EV/profiles-after" 2>/dev/null
ls "$EV"
