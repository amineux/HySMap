# Phase 1 — Tiny simulator

**Goal:** a 10–100 neuron SNN, a 4×4 mesh, a hypergraph, XY routing, and a cost you can print in one command.

```bash
./build/hysmap demo --mesh 4 --neurons 80 --seed 1
```

That is the Phase 1 contract. Everything later is a better *search* over the same model.

## The three objects

```mermaid
flowchart LR
  N[Neurons 10–100] --> H[Hyperedge h_u = u → fanout]
  H --> R[XY multicast union T_u]
  R --> J["J = hops + congestion"]
```

1. **SNN as a directed hypergraph.** Neuron \(u\) emits one spike event. Destinations \(N^+(u)\) share that event. Weight \(r_u\) is the source rate. See `DirectedHypergraph`.
2. **Mesh NoC.** An \(N\times N\) grid of cores. Deterministic **X-then-Y** paths. Multicast cost is the *union* of those paths, so a shared prefix is paid once. See `MeshNoC`.
3. **Cost.** \(C_{\mathrm{hop}}=\sum_u r_u|T_u|\). Congestion is \(\max_\ell L_\ell\) and \(\mathrm{Var}(L)\). Composite \(\mathcal{J}\) lives in `evaluate()`.

```
        dest B ●──────● dest C
               │
 source ●──────●  shared XY prefix counted once
               │
        dest A ●
```

A 2×2 walk-through is in [`docs/algorithm.md`](../algorithm.md) §3.

## What “simple SNN” means here

`generate_snn` builds a **Potjans-inspired** layered E/I microcircuit, scaled to tens or hundreds of neurons — not the 77k-neuron cortical model. Rates are synthetic (lognormal around layer means). This is a research workload generator, not a neuroscience claim.

```bash
./build/hysmap generate --preset potjans --neurons 80 --seed 1 --out net.json
./build/hysmap map --input net.json --mesh 4 --mapper activity-qap --seed 1
```

## Phase 1 checklist

| Piece | Module |
| --- | --- |
| Hypergraph | `include/hysmap/hypergraph.hpp` |
| Mesh + XY | `include/hysmap/mesh.hpp` |
| Cost + lower bound | `include/hysmap/cost.hpp` |
| Generator | `include/hysmap/snn_generator.hpp` |
| One-command demo | `src/cli.cpp` → `hysmap demo` |

Next: [Phase 2 — Mapper](02-mapper.md).
