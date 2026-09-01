# Phase 4 — Performance

Don't resimulate 80 axons to ask "what if this one neuron moved?" Cache the routes. Invalidate the sources that can notice. Thread the big candidate batches; leave the tiny ones alone.

Move one neuron, list `A(v)`, watch `C_hop` drop 8 → 6: [`docs/examples/04-incremental-delta.md`](../examples/04-incremental-delta.md).

## Incremental \(\Delta\mathcal{J}\)

Moving neuron \(v\) changes only

\[
\mathcal{A}(v)=\{v\}\cup N^-(v).
\]

`IncrementalEvaluator` subtracts those cached route unions, recomputes them, and rebuilds \(\max\) / variance from the load vector. Catch2 requires peeked \(\mathcal{J}\) to match a full recompute to \(10^{-9}\).

```bash
./build/hysmap demo --mesh 4 --neurons 80 --seed 1 --mapper hysmap-seeded --threads 4
```

Measured on this binary (not the papers):

| Workload | Incremental vs full recompute | 4-thread gain eval |
| --- | ---: | ---: |
| 80 neurons, 4×4 | **2.66×** | **1.66×** |
| 108 neurons, 6×6 | **4.44×** | **2.56×** |

Batches smaller than 32 candidate evaluations stay serial — thread spawn dominates a 16-core peek list. Placement 2-swaps on 6×6 (\(k(k-1)/2=630\) pairs) cross that threshold.

`--threads N` (default: hardware concurrency) controls:

- parallel peek of feasible cores for one neuron
- parallel multicast placement swaps
- parallel independent pipelines inside `hysmap compare` / `bench`

## Route cache

Per source: destination-core set, XY union, hop contribution. Unrelated sources are never touched. That is the locality theorem from Khorasanian ([arXiv:2608.26223](https://arxiv.org/abs/2608.26223)); this repo reimplements the idea.

## Code

- `include/hysmap/incremental.hpp`
- `include/hysmap/parallel.hpp` (`parallel_for`)
- `refine_multicast(..., threads)` / `refine_placement_multicast(..., threads)`

Next: [Phase 5 — Research](05-research.md).
