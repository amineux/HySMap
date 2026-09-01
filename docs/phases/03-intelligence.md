# Phase 3 — Intelligence

Unweighted edges treat a 0.5 Hz axon like a 20 Hz axon. Don't. Weight by rate, and let the hypergraph's Laplacian suggest a seed.

```bash
./build/hysmap map --input examples/net_potjans_80.json --mesh 4 \
    --mapper activity-qap --seed 1

./build/hysmap map --input examples/net_potjans_80.json --mesh 4 \
    --seed-strategy spectral --refine greedy --seed 1

./build/hysmap map --input examples/net_potjans_80.json --mesh 4 \
    --mapper spectral --seed 1
```

## Activity weighting

Replace each unit edge with the source rate \(r_u\). Edge+QAP and Activity+QAP share the same search; only the pairwise flow \(F_{ij}\) changes. High-rate axons pull their cores together under the QAP objective \(\sum_{ij} F_{ij} D_{\pi(i)\pi(j)}\).

Same topology, two rate profiles (41 vs 14 vs 21): [`docs/examples/02-activity-weighting.md`](../examples/02-activity-weighting.md).

## QAP initialization

`place_qap` is a multi-start greedy 2-swap on that pairwise objective, then a force-directed polish. It is a **seed**, not the hardware cost. Two mappings can share QAP cost and differ in multicast hops.

`--seed-strategy qap` runs balanced assignment + edge refinement + QAP placement and stops there unless `--refine` says otherwise.

## Spectral hypergraph placement

Inspired by Ronzani & Silvano ([arXiv:2601.16118](https://arxiv.org/abs/2601.16118) §IV-B2), not copied from their code:

1. Explode each hyperedge into pairwise weights (source + destinations).
2. Build the **normalized Laplacian**.
3. Take the two smallest nontrivial eigenpairs (Fiedler-style 2-D embedding).
4. Discretize onto unused mesh coordinates.

`--mapper spectral` seeds neurons from the *neuron-level* Laplacian, places cores from the *partition-level* Laplacian, then continues into multicast refine (Phase 4) unless you override `--refine`.

```mermaid
flowchart LR
  H[Hypergraph] --> L[Normalized Laplacian]
  L --> E[Eigenvectors u1, u2]
  E --> G[Continuous 2-D embedding]
  G --> M[Nearest free mesh cell]
```

Next: [Phase 4 — Performance](04-performance.md).
