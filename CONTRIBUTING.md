# Contributing to HySMap

Thanks for helping turn a mapping research prototype into a product-quality SDK.

## Ground rules

- **No fabricated academic claims.** Phrase results as *this implementation's measured numbers*. Cite Ronzani & Silvano (arXiv:2601.16118) and Khorasanian (arXiv:2608.26223) as inspiration, not as if this repo is those papers.
- Keep the core deterministic: every mapper path should take an explicit RNG seed.
- Incremental multicast gains must match a full recompute to numerical precision. If you change `IncrementalEvaluator`, extend `tests/test_incremental.cpp`.
- Prefer small, focused C++20 modules under `include/hysmap/` + `src/`. No GPU dependencies in the default build.

## Workflow

```bash
cmake -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/hysmap demo --mesh 4 --neurons 80 --seed 1
```

- Open a PR against `main`. CI must stay green (Ubuntu C++ tests + Python binding smoke).
- Phase docs live under `docs/phases/`. Keep the README phase table in sync.
- `hysmap export` is a Loihi-*style* research format — do not claim NxSDK compatibility.
- If you change the objective, routing, or generator, re-run `hysmap bench --quick` and update `results/` plus the README table with **your** numbers.

## Style

- `NeuronId`, `CoreId`, `Hyperedge`, `Mapping` — do not introduce raw `int` IDs in new APIs.
- RAII, const-correctness, no uninitialized reads.
- Document assumptions next to lower bounds and hardware proxies. These are traffic models, not calibrated energy/latency claims.
