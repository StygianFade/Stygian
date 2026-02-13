#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

TARGET=""
GROUP=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --target)
      TARGET="${2:-}"
      shift 2
      ;;
    --group)
      GROUP="${2:-}"
      shift 2
      ;;
    *)
      if [[ -z "$TARGET" && -z "$GROUP" ]]; then
        TARGET="$1"
        shift
      else
        echo "unknown argument: $1" >&2
        exit 1
      fi
      ;;
  esac
done

if [[ -z "$TARGET" && -z "$GROUP" ]]; then
  echo "usage: compile/run.sh --target <name> | --group <name>" >&2
  exit 1
fi

UNAME="$(uname -s)"
if [[ "$UNAME" == "Darwin" ]]; then
  RUNNER="$ROOT/compile/macos/build.sh"
else
  RUNNER="$ROOT/compile/linux/build.sh"
fi

if [[ ! -x "$RUNNER" ]]; then
  chmod +x "$RUNNER"
fi

if [[ -n "$GROUP" ]]; then
  "$RUNNER" __none__ "$GROUP"
else
  "$RUNNER" "$TARGET"
fi
