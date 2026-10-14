#!/usr/bin/env bash
# RDoom NX - full reproducible build.
#
#   scripts/build.sh            # image + host tools + RDoom.nro + package + tests
#   JOBS=4 scripts/build.sh     # limit parallel jobs (default: nproc)
#   CLEAN=1 scripts/build.sh    # start from empty build directories
#
# Everything that targets the Switch runs inside the rdoom-nx-build Docker
# image (docker/Dockerfile: devkitA64 + libnx + portlibs + Mesa 26.2.4 SDK).
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
JOBS=${JOBS:-$(nproc)}
IMAGE=rdoom-nx-build
LOG_DIR="$ROOT/build/logs"
mkdir -p "$LOG_DIR"

step() { printf '\n==> %s\n' "$*"; }
die() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }

# 1. dependencies on the host -------------------------------------------------
step "Checking host tools"
for t in docker git python3 rsvg-convert magick sha256sum curl unzip; do
	command -v "$t" >/dev/null || die "missing host tool: $t"
done
docker info >/dev/null 2>&1 || die "docker daemon not reachable"

# 2. sources and revisions -----------------------------------------------------
step "Checking source revisions"
UZ_BASE=7910df7102033fd7754a317f3254f24101135926   # UZDoom tag 5.0.3
git -C "$ROOT/engine" cat-file -e "$UZ_BASE^{commit}" || die "engine/ does not contain UZDoom 5.0.3 ($UZ_BASE)"
git -C "$ROOT/engine" merge-base --is-ancestor "$UZ_BASE" HEAD || die "engine/ HEAD is not based on UZDoom 5.0.3"
echo "engine: $(git -C "$ROOT/engine" describe --always --tags --dirty) on UZDoom 5.0.3 ($UZ_BASE)"
SDK="$ROOT/third_party/mesa-sdk/mesa-26.2.4-switch-unified-horizon-sdk-606441c9.zip"
if [ ! -f "$SDK" ]; then
	step "Fetching the Mesa Horizon SDK (danfromtico/mesa-switch CI artifact 11484346771)"
	mkdir -p "$(dirname "$SDK")"
	gh api repos/danfromtico/mesa-switch/actions/artifacts/11484346771/zip > "$SDK" || die "cannot download the Mesa SDK (needs gh auth)"
fi
echo "937cbda82d411c3ca909aa977b3ced82d4e6070046ab016911b1ce84e58ca392  $SDK" | sha256sum -c - >/dev/null || die "Mesa SDK checksum mismatch"

FD_ZIP="$ROOT/third_party/freedoom/freedoom-0.13.0.zip"
if [ ! -f "$ROOT/third_party/freedoom/freedoom-0.13.0/freedoom2.wad" ]; then
	step "Fetching Freedoom 0.13.0 (BSD licence; launcher art and the Freedoom package)"
	mkdir -p "$(dirname "$FD_ZIP")"
	[ -f "$FD_ZIP" ] || curl -fsSL -o "$FD_ZIP" https://github.com/freedoom/freedoom/releases/download/v0.13.0/freedoom-0.13.0.zip || die "cannot download Freedoom"
	echo "3f9b264f3e3ce503b4fb7f6bdcb1f419d93c7b546f4df3e874dd878db9688f59  $FD_ZIP" | sha256sum -c - >/dev/null || die "Freedoom checksum mismatch"
	(cd "$ROOT/third_party/freedoom" && unzip -q -o "$FD_ZIP")
fi

# 3. toolchain image -------------------------------------------------------------
step "Building the toolchain image ($IMAGE)"
docker build -q -f "$ROOT/docker/Dockerfile" -t "$IMAGE" "$ROOT/third_party" > "$LOG_DIR/docker-image.log" 2>&1 || { cat "$LOG_DIR/docker-image.log"; die "docker build failed"; }

# 4. artwork (host: rsvg-convert + ImageMagick) -------------------------------------
step "Rendering launcher artwork"
python3 "$ROOT/assets/src/generate_art.py" > "$LOG_DIR/artwork.log"

if [ "${CLEAN:-0}" = 1 ]; then
	rm -rf "$ROOT/build/native" "$ROOT/build/nx"
fi

run() {
	docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp -e JOBS="$JOBS" \
		-v "$ROOT:/src" -w /src "$IMAGE" bash -euo pipefail -c "$1"
}

# 5. host tools used by the engine build (zipdir, re2c, lemon) ----------------------
step "Building host tools"
run 'cmake -S engine -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null
     ninja -C build/native -j"$JOBS" zipdir re2c lemon' > "$LOG_DIR/native.log" 2>&1 || { tail -40 "$LOG_DIR/native.log"; die "host tools failed"; }

# 6-7. engine + launcher for the Switch ---------------------------------------------
step "Building RDoom.nro (UZDoom 5.0.3 + RDoom launcher, Vulkan)"
run 'export PORTLIBS_PREFIX=$DEVKITPRO/portlibs/switch
     if [ ! -f build/nx/build.ninja ]; then
       cmake -S engine -B build/nx -G Ninja \
         -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake \
         -DCMAKE_BUILD_TYPE=Release -DNINTENDO_SWITCH=ON \
         -DHAVE_VULKAN=ON -DHAVE_OPENGL=OFF -DHAVE_GLES2=OFF -DNO_STRIP=OFF \
         -DDYN_OPENAL=OFF -DDYN_SNDFILE=OFF -DDYN_MPG123=OFF \
         -DIMPORT_EXECUTABLES=/src/build/native/ImportExecutables.cmake \
         -DSNDFILE_INCLUDE_DIR=$PORTLIBS_PREFIX/include \
         -DSNDFILE_LIBRARY=$PORTLIBS_PREFIX/lib/libsndfile.a
     fi
     ninja -C build/nx -j"$JOBS"' > "$LOG_DIR/nx.log" 2>&1 || { grep -E "error|Error" "$LOG_DIR/nx.log" | head -40; tail -20 "$LOG_DIR/nx.log"; die "Switch build failed (full log: $LOG_DIR/nx.log)"; }
grep -c "warning:" "$LOG_DIR/nx.log" | xargs -I{} echo "compiler warnings: {} (see build/logs/nx.log)"

# thread-pointer check: every object we compile must use libnx's soft TP; only
# the prebuilt Mesa Rust code reads TPIDR_EL0 (handled by nx_tls_bridge.c)
step "Checking thread-pointer use in RDoom's own objects"
run 'bad=0
     for o in $(find build/nx -name "*.o" -o -name "*.obj" | grep -v nx_tls_bridge); do
       if aarch64-none-elf-objdump -d "$o" 2>/dev/null | grep -q "tpidr_el0"; then echo "uses tpidr_el0: $o"; bad=1; fi
     done
     exit $bad' > "$LOG_DIR/tp-check.log" 2>&1 || { cat "$LOG_DIR/tp-check.log"; die "an RDoom object reads TPIDR_EL0 directly"; }
echo "ok: no RDoom object reads TPIDR_EL0"

# 8-9. package -----------------------------------------------------------------------
step "Packaging"
"$ROOT/scripts/package.sh"

# 10. tests ----------------------------------------------------------------------------
step "Running automated tests"
"$ROOT/scripts/test.sh"

step "Done"
ls -l "$ROOT/dist/RDoom-NX/switch/rdoom/RDoom.nro" "$ROOT/dist/"*.zip
