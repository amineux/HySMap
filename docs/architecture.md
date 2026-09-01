# Architecture

HySMap is a small C++20 library plus a `hysmap` CLI. The library is the product surface; the CLI is how you generate evidence and demos.

```
SNN JSON / generator
        │
        ▼
 DirectedHypergraph          MeshNoC (XY + multicast union)
        │                           │
        └────────────┬──────────────┘
                     ▼
              Mapping (p, π)
                     │
     ┌───────────────┼──────────────────┐
     ▼               ▼                  ▼
 Edge / activity   Spectral seed    IncrementalEvaluator
 partition+QAP     (Laplacian)      (exact local ΔJ)
     │               │                  │
     └───────────────┴────────┬─────────┘
                              ▼
                     Portfolio incumbent
                              │
                              ▼
                   JSON / CSV metrics
```

## Library modules

| Path | Responsibility |
| --- | --- |
| `types.hpp` | `NeuronId`, `CoreId`, `Mapping`, `ObjectiveWeights`, `CostBreakdown` |
| `hypergraph.hpp` | Source-rooted directed hyperedges, predecessor index, \(\mathcal{A}(v)\) |
| `mesh.hpp` | \(R\times C\) cores, directed link IDs, XY paths, multicast unions |
| `cost.hpp` | Full \(\mathcal{J}\), edge/activity cut, conservative lower bound |
| `incremental.hpp` | Cached per-source routes; peek/commit neuron moves and placement swaps |
| `partition.hpp` | Balance, Edge+QAP FM, multicast FM |
| `placement.hpp` | QAP 2-swap, force, min-distance, spectral, multicast 2-swap |
| `mapper.hpp` | Named pipelines + portfolio |
| `snn_generator.hpp` | Potjans-inspired / layered / ER workloads |
| `io.hpp` | JSON network I/O, JSON/CSV results |

Public include: `#include <hysmap/hysmap.hpp>`.

## Data layout

`Mapping::partition[v]` is the logical core of neuron \(v\). `Mapping::placement[c]` is the physical mesh id of logical core \(c\). The CLI’s `assignment` array is the **physical** core of each neuron (`placement[partition[v]]`).

Directed mesh links are packed integers (`LinkId`) so load vectors are dense arrays of length \(M=2(R(C-1)+C(R-1))\).

## CLI

| Command | Role |
| --- | --- |
| `generate` | Write a hypergraph JSON |
| `map` | One mapper → metrics + optional assignment JSON |
| `compare` | Edge+QAP, Activity+QAP, Spectral, HySMap-seeded, HySMap |
| `bench` | Seeded suite over mesh sizes (writes CSV) |
| `demo` | Zero-file walkthrough |

Exit codes: `0` success, `1` usage/runtime error.

## Build graph

CMake 3.20+ fetches **Eigen 3.4** (spectral) and **Catch2 v3** (tests). No GPU, no MPI, no Python required for the core.

```text
cmake -B build && cmake --build build -j && ctest --test-dir build
```

## Extension points (SDK roadmap)

- Replace `MeshNoC::xy_path` / `multicast_union` with another deterministic policy (West-first, custom multicast tree) without touching the incremental locality theorem.
- Swap `generate_snn` for an importer (NEST / Brian / Loihi NxNet). The hypergraph JSON is the stable interchange.
- Bindings: pybind11 is intentionally not in the default graph (see README roadmap).
- GUI / Loihi export are out of scope for v0.1; the library API is shaped so they can sit on top.
