#!/usr/bin/env bash
# RDoom automated tests that run on the build machine.
#   scripts/test.sh
# 1. launcher core unit tests (host, ASan+UBSan): profiles, discovery,
#    persistence, corrupt-file recovery, dependencies, launch plan, INI.
# 2. launcher UI smoke test (host): every screen renders, a launch plan comes out.
# 3. no-overclock check (linked symbols + sources).
# 4. package integrity (dist/).
# Emulator tests (vktest.nro / RDoom.nro in Ryujinx) are in scripts/ryujinx-test.sh.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT="$ROOT/build/test-results"
mkdir -p "$OUT"
status=0
result() { printf '%-34s %s\n' "$1" "$2" | tee -a "$OUT/summary.txt"; }
: > "$OUT/summary.txt"

cmake -S "$ROOT/tests" -B "$ROOT/build/host-tests" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
	-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" > "$OUT/host-configure.log" 2>&1 \
	&& ninja -C "$ROOT/build/host-tests" > "$OUT/host-build.log" 2>&1
if [ $? -ne 0 ]; then
	result "host test build" "FAIL (see build/test-results/host-build.log)"
	exit 1
fi

if "$ROOT/build/host-tests/launcher_tests" "$ROOT/build/host-tests/scratch" \
	"$ROOT/engine/wadsrc_extra/static/iwadinfo.txt" "$ROOT/third_party/freedoom/freedoom-0.13.0" > "$OUT/launcher_tests.log" 2>&1; then
	result "launcher core unit tests" "PASS ($(tail -1 "$OUT/launcher_tests.log"))"
else
	result "launcher core unit tests" "FAIL ($(tail -1 "$OUT/launcher_tests.log"))"; status=1
fi

# UI smoke: synthetic SD card with only free/synthetic content
SD="$ROOT/build/test-results/ui-sd"
rm -rf "$SD" && mkdir -p "$SD/iwads" "$SD/mods"
cp "$ROOT/third_party/freedoom/freedoom-0.13.0/freedoom2.wad" "$SD/iwads/"
if "$ROOT/build/host-tests/ui_preview" "$SD" "$ROOT/assets/build" "$ROOT/engine/wadsrc_extra/static/iwadinfo.txt" "$OUT/ui" > "$OUT/ui_preview.log" 2>&1 \
	&& [ "$(ls "$OUT/ui"/*.png | wc -l)" -ge 12 ] && grep -q "launch argv:.*-iwad" "$OUT/ui_preview.log"; then
	result "launcher UI smoke test" "PASS ($(ls "$OUT/ui"/*.png | wc -l) screens, launch plan produced)"
else
	result "launcher UI smoke test" "FAIL (see build/test-results/ui_preview.log)"; status=1
fi

ELF="$ROOT/build/nx/uzdoom"
[ -f "$ELF" ] || ELF="$ROOT/build/nx/uzdoom.elf"
if bash "$ROOT/tests/check_no_overclock.sh" "$ELF" > "$OUT/no_overclock.log" 2>&1; then
	result "no-overclock check" "PASS"
else
	result "no-overclock check" "FAIL (see build/test-results/no_overclock.log)"; status=1
fi

if [ -d "$ROOT/dist/RDoom-NX" ]; then
	if python3 -I "$ROOT/tests/check_package.py" "$ROOT/dist" > "$OUT/package.log" 2>&1; then
		result "package integrity" "PASS ($(grep -c '^ok:' "$OUT/package.log") checks)"
	else
		result "package integrity" "FAIL (see build/test-results/package.log)"; status=1
	fi
else
	result "package integrity" "SKIPPED (no dist/ yet)"
fi
exit $status
