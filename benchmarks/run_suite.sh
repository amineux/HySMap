#!/usr/bin/env bash
# Run the documented HySMap evidence suite and write CSV under results/.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${HYSMAP_BIN:-$ROOT/build/hysmap}"
OUT="${1:-$ROOT/results/bench.csv}"
mkdir -p "$(dirname "$OUT")"
if [[ ! -x "$BIN" ]]; then
  echo "Build first: cmake -B build && cmake --build build -j" >&2
  exit 1
fi
MODE="${2:-quick}"
if [[ "$MODE" == "full" ]]; then
  "$BIN" bench --full --out "$OUT" --seed 1
else
  "$BIN" bench --quick --out "$OUT" --seed 1
fi
echo "CSV: $OUT"
