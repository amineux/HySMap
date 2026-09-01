# CLI cookbook

Assume you already ran `cmake -B build && cmake --build build -j`. Paths are from the repo root.

### 1. Phase 1 demo (tiny simulator)

```bash
./build/hysmap demo --mesh 4 --neurons 80 --seed 1
```

Prints hops, max link load, J, and the conservative lower bound.

### 2. Random seed + greedy edge-cut refine

```bash
./build/hysmap map --input examples/net_potjans_80.json --mesh 4 \
    --seed-strategy random --refine greedy --seed 1
```

This is the graph-style mapper: unit cut, then QAP-ish placement only if the named `--mapper` asks for it. Here the flags override the pipeline.

### 3. Activity + spectral

```bash
./build/hysmap map --input examples/net_potjans_80.json --mesh 4 \
    --mapper activity-qap --seed 1

./build/hysmap map --input examples/net_potjans_80.json --mesh 4 \
    --seed-strategy spectral --refine greedy --seed 1
```

### 4. Compare the five named mappers (the wow command)

```bash
./build/hysmap compare --input examples/net_potjans_80.json --mesh 4 --seed 1
```

Last line is hop reduction vs Edge+QAP and vs Activity+QAP. Seeded. Should match the README.

### 5. Quick bench → plots

```bash
./build/hysmap bench --quick --out results/bench.csv
python3 scripts/plot_results.py --csv results/bench.csv --out results/plots
```

`--quick` is one seed and a small mesh list. Don't overwrite `results/bench.csv` unless you mean to replace the published table.

### 6. Loihi-style research export

```bash
./build/hysmap export --input examples/net_potjans_80.json --mesh 4 \
    --format loihi-json -o /tmp/loihi.json
./build/hysmap export --input examples/net_potjans_80.json --format loihi-stub -o /tmp/loihi.txt
```

Research JSON. Not official NxSDK.

### 7. Python

```bash
cmake -B build -DHYSMAP_BUILD_PYTHON=ON
cmake --build build -j
PYTHONPATH=build python3 python/examples/quickstart.py
python3 scripts/intuition.py
```
