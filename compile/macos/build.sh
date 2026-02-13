#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

TARGET="${1:-}"
GROUP="${2:-}"
CLANG_BIN="${CLANG:-clang}"
MANIFEST="$ROOT/compile/targets.json"

if [[ ! -f "$MANIFEST" ]]; then
  echo "missing manifest: $MANIFEST" >&2
  exit 1
fi

if ! command -v jq >/dev/null 2>&1; then
  echo "jq is required for compile/macos/build.sh" >&2
  exit 1
fi

mkdir -p build

build_one() {
  local name="$1"
  local backend entry stem
  backend="$(jq -r --arg n "$name" '.targets[$n].backend // empty' "$MANIFEST")"
  entry="$(jq -r --arg n "$name" '.targets[$n].entry_source // empty' "$MANIFEST")"
  stem="$(jq -r --arg n "$name" '.targets[$n].output_stem // empty' "$MANIFEST")"
  if [[ -z "$backend" || -z "$entry" || -z "$stem" ]]; then
    echo "unknown target: $name" >&2
    exit 1
  fi

  mapfile -t common_flags < <(jq -r '.common.flags[]' "$MANIFEST")
  mapfile -t common_includes < <(jq -r '.common.includes[]' "$MANIFEST")
  mapfile -t common_sources < <(jq -r '.common.sources[]' "$MANIFEST")
  mapfile -t common_defines < <(jq -r '.common.defines[]' "$MANIFEST")

  args=()
  args+=("${common_flags[@]}")
  for inc in "${common_includes[@]}"; do
    args+=("-I" "$inc")
  done
  for def in "${common_defines[@]}"; do
    args+=("-D${def}")
  done

  args+=("$entry")
  args+=("${common_sources[@]}")
  args+=("window/platform/stygian_cocoa.m" "window/platform/stygian_cocoa_shim.m")

  if [[ "$backend" == "vk" ]]; then
    args+=("$(jq -r '.common.vk_backend_source' "$MANIFEST")")
    args+=("-DSTYGIAN_DEMO_VULKAN" "-DSTYGIAN_VULKAN")
    args+=("-lvulkan")
  else
    args+=("$(jq -r '.common.gl_backend_source' "$MANIFEST")")
    args+=("-framework" "OpenGL")
  fi

  args+=("-o" "build/${stem}")
  args+=("-framework" "Cocoa" "-framework" "QuartzCore" "-framework" "IOKit" "-framework" "CoreVideo")
  args+=("-lz" "-lzstd")

  echo "[${name}] Building..."
  "$CLANG_BIN" "${args[@]}"
  echo "[${name}] Build SUCCESS: build/${stem}"
}

if [[ -n "$GROUP" ]]; then
  mapfile -t group_targets < <(jq -r --arg g "$GROUP" '.groups[$g][]? // empty' "$MANIFEST")
  if [[ ${#group_targets[@]} -eq 0 ]]; then
    echo "unknown group: $GROUP" >&2
    exit 1
  fi
  for t in "${group_targets[@]}"; do
    build_one "$t"
  done
  echo "[${GROUP}] Build SUCCESS"
  exit 0
fi

if [[ -z "$TARGET" ]]; then
  echo "usage: compile/macos/build.sh <target> [group]" >&2
  exit 1
fi

build_one "$TARGET"
