#!/usr/bin/env bash
# Convenience wrapper around the top-level Makefile
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
BOARD="${BOARD:-badge}"
WAD_BACKEND="${WAD_BACKEND:-embedded}"
exec make BOARD="$BOARD" WAD_BACKEND="$WAD_BACKEND" "$@"
