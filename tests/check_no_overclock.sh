#!/usr/bin/env bash
# Verifies that RDoom has no way to raise clocks.
#   1. the linked ELF contains none of the libnx/service calls that change
#      CPU/GPU/EMC clocks or select a boost/performance configuration;
#   2. RDoom's own sources, scripts and configs never call or mention a clock
#      or overclock tool (documentation may explain the policy).
# Usage: tests/check_no_overclock.sh [build/nx/RDoom elf]
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ELF=${1:-$ROOT/build/nx/uzdoom.elf}
fail=0

# Clock/voltage control: must not be linked at all.
FORBIDDEN_SYMBOLS=(
	pcvSetClockRate pcvSetVoltageValue pcvInitialize
	clkrstSetClockRate clkrstOpenSession clkrstInitialize
)
# Performance-mode entry points that libnx's own applet.c references in every
# homebrew: _appletInitialize calls apmSetPerformanceConfiguration only for a
# non-zero __nx_applet_PerformanceConfiguration (libnx default {0,0}), and
# _appletCleanup calls appletSetCpuBoostMode only to put Normal mode back.
# They may be linked, but every call must come from libnx's applet.c and the
# configuration must be the zero default.
LIBNX_ONLY_SYMBOLS=(apmSetPerformanceConfiguration apmInitialize appletSetCpuBoostMode)
if [ -f "$ELF" ]; then
	report=$(docker run --rm -v "$ROOT:/src" -w /src rdoom-nx-build bash -c '
		E=$1; shift
		nm "$E" > /tmp/syms 2>/dev/null || aarch64-none-elf-nm "$E" > /tmp/syms
		aarch64-none-elf-objdump -d --no-show-raw-insn "$E" > /tmp/dis
		echo "SYMS $(wc -l < /tmp/syms)"
		for s in "$@"; do
			a=$(awk -v s="$s" "\$3==s{print \$1}" /tmp/syms | sed "s/^0*//")
			[ -z "$a" ] && { echo "ABSENT $s"; continue; }
			grep -E "\sb(l)?\s+$a <" /tmp/dis | awk -F: "{print \$1}" | while read -r site; do
				echo "CALL $s $(aarch64-none-elf-addr2line -f -e "$E" 0x$site | tr "\n" " ")"
			done
		done
		cfg=$(aarch64-none-elf-objdump -t "$E" | awk "\$NF==\"__nx_applet_PerformanceConfiguration\"{print \$1, \$4, \$5}")
		echo "PERFCFG $cfg"
	' _ "${ELF#$ROOT/}" "${FORBIDDEN_SYMBOLS[@]}" "${LIBNX_ONLY_SYMBOLS[@]}" 2>/dev/null)
	if ! grep -q '^SYMS [1-9]' <<<"$report"; then
		echo "FAIL: could not read symbols from $ELF"
		fail=1
	fi
	for s in "${FORBIDDEN_SYMBOLS[@]}"; do
		if ! grep -q "^ABSENT $s$" <<<"$report"; then
			echo "FAIL: $s is linked into $(basename "$ELF")"
			fail=1
		fi
	done
	outside=$(for s in "${LIBNX_ONLY_SYMBOLS[@]}"; do
		grep "^CALL $s " <<<"$report" | grep -vE " _applet(Initialize|Cleanup) .*libnx-.*/nx/source/services/applet.c"
	done)
	if [ -n "$outside" ]; then
		echo "FAIL: performance-mode calls outside libnx applet init/cleanup:"
		echo "$outside"
		fail=1
	fi
	# the configuration libnx would apply must be its zero default (weak, in .bss)
	if grep -qE '^PERFCFG [0-9a-f]+ \.bss 0+8$' <<<"$report"; then
		echo "ok: __nx_applet_PerformanceConfiguration is libnx's zero default (.bss): no performance mode is ever set"
	else
		echo "FAIL: __nx_applet_PerformanceConfiguration is not the zero default: $(grep '^PERFCFG' <<<"$report")"
		fail=1
	fi
	[ $fail = 0 ] && echo "ok: no clock/voltage entry point linked; apm/boost calls only in libnx applet init/cleanup ($(grep -c '^CALL' <<<"$report") call sites)"
else
	echo "skip: $ELF not built yet (symbol check)"
fi

# sources and configuration written for RDoom
pattern='pcvSetClockRate|clkrstSetClockRate|apmSetPerformanceConfiguration|appletSetCpuBoostMode|sys-clk|sysclk|Horizon-OC|overclock'
hits=$(grep -rniE "$pattern" \
	"$ROOT/launcher/src" "$ROOT/scripts" "$ROOT/docker" "$ROOT/vktest/source" \
	"$ROOT/engine/src/common/platform/posix/nx" \
	--include='*.c' --include='*.cpp' --include='*.h' --include='*.sh' --include='Dockerfile' --include='*.json' --include='*.ini' 2>/dev/null \
	| grep -v 'never changes\|does nothing\|no overclock\|no-overclock\|sem overclock\|não tem opções de overclock\|nunca altera\|não altera frequências\|check_no_overclock' || true)
if [ -n "$hits" ]; then
	echo "FAIL: clock-related references in RDoom code:"
	echo "$hits"
	fail=1
else
	echo "ok: no clock-control calls or overclock options in RDoom sources/scripts/configs"
fi
# the profile format has no clock fields
if grep -qiE 'clock|freq|boost' "$ROOT/launcher/src/core/profile.h"; then
	echo "FAIL: profile.h mentions clocks"
	fail=1
else
	echo "ok: the profile format has no clock/frequency/boost fields"
fi
exit $fail
