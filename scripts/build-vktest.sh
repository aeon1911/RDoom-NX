#!/usr/bin/env bash
# Builds vktest/vktest.nro inside the rdoom-nx-build image.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
docker run --rm -u "$(id -u):$(id -g)" -v "$ROOT:/src" -w /src/vktest rdoom-nx-build \
	bash -c 'make -j"$(nproc)" && n=$(aarch64-none-elf-objdump -d vktest.elf | grep -c "msr\s*tpidr_el0" || true); echo "tpidr_el0 writes in vktest.elf: $n"'
ls -l "$ROOT/vktest/vktest.nro"
