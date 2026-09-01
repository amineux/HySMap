#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${HYSMAP_BIN:-$ROOT/build/hysmap}"
PY="${PYTHON:-$ROOT/.venv/bin/python}"
if [[ ! -x "$BIN" ]]; then
  echo "Build first: cmake -B build && cmake --build build -j" >&2
  exit 1
fi
if [[ ! -x "$PY" ]]; then
  PY=python3
fi
mkdir -p "$ROOT/docs/assets" "$ROOT/results/demo" "$ROOT/results/plots"
"$PY" "$ROOT/scripts/plot_results.py" --csv "$ROOT/results/bench.csv" --out "$ROOT/results/plots"
"$PY" "$ROOT/scripts/make_demo.py" --hysmap "$BIN" --net "$ROOT/examples/net_potjans_80.json" --out "$ROOT/docs/assets"
echo "demo artifacts: docs/assets/ and results/demo/"
