#!/usr/bin/env bash
# Incremental rebuild of RDoom.nro + package (development loop; full build: build.sh).
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp -v "$ROOT:/src" -w /src rdoom-nx-build \
	bash -c 'export PORTLIBS_PREFIX=$DEVKITPRO/portlibs/switch; ninja -C build/nx -j"$(nproc)"' > "$ROOT/build/logs/nx-dev.log" 2>&1 \
	|| { grep -E "error|undefined|multiple" "$ROOT/build/logs/nx-dev.log" | head -20; exit 1; }
"$ROOT/scripts/package.sh" > /dev/null
echo "rebuilt: $(sha256sum "$ROOT/dist/RDoom.nro")"
