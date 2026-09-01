# HySMap technical report

**Activity-weighted multicast hypergraph mapping for spiking neural networks on mesh NoCs**

Version 0.2.0 — implementation report, not a conference submission.

This document describes *this* C++20 library. It is **inspired by** Ronzani & Silvano ([arXiv:2601.16118](https://arxiv.org/abs/2601.16118)) and Khorasanian ([arXiv:2608.26223](https://arxiv.org/abs/2608.26223)). It does not claim those authors’ results, code, or datasets.

---

## 1. Problem

Spiking neural networks communicate with sparse, event-driven spikes. On a neuromorphic many-core chip the expensive object is a spike that must cross the on-chip network. Mapping therefore asks two coupled questions:

1. **Partition** — which neurons share a core (capacity, axon reuse).
2. **Place** — where each core sits on an \(R\times C\) mesh.

The classical reduction is a directed *graph* of synapses plus a pairwise placement cost (quadratic assignment). That reduction is convenient and wrong in a precise way: one axon spike is delivered to a *set* of destinations, and XY routes to those destinations share links. Two mappings can share an edge cut and differ in multicast fanout, hop count, and bottleneck load.

HySMap treats the communication object as a **directed, activity-weighted hyperedge** and optimizes the **union of deterministic mesh routes**.

---

## 2. Model

### 2.1 Hypergraph

\[
h_u=(u\rightarrow N^+(u)),\qquad w(h_u)=r_u\ge 0.
\]

Each neuron owns at most one outgoing hyperedge (its axon). \(r_u\) is a profiled or synthetic spike rate. Inbound adjacency \(N^-(v)\) is indexed for locality.

### 2.2 Mapping

\[
p:V\to\{0,\ldots,k-1\},\qquad \pi:\{0,\ldots,k-1\}\to\text{mesh}.
\]

The physical core of \(v\) is \(\pi(p(v))\). Balance uses slack \(s=0.15\):

\[
\lfloor(1-s)n/k\rfloor \le |p^{-1}(c)| \le \lceil(1+s)n/k\rceil
\]

and a hard per-core capacity.

### 2.3 Routing and objective

\(\mathrm{XY}(a,b)\) is the X-then-Y Manhattan path. Destination cores of source \(u\):

\[
D_u=\{p(v):v\in N^+(u),\,p(v)\neq p(u)\}.
\]

Route union \(T_u=\bigcup_{c\in D_u}\mathrm{XY}(\pi(p(u)),\pi(c))\). Then

\[
C_{\mathrm{hop}}=\sum_u r_u|T_u|,\qquad
L_\ell=\sum_u r_u\mathbf{1}[\ell\in T_u],
\]

\[
\mathcal{J}=\alpha C_{\mathrm{hop}}+\beta\max_\ell L_\ell+\gamma\operatorname{Var}(L)
\]

with defaults \(\alpha=1\), \(\beta=0.10\), \(\gamma=0.01\). Soft imbalance weights are zero. These are **traffic proxies**, not calibrated energy or latency.

The union is the exact XY multicast set. It is not a Steiner tree and not an adaptive fabric.

### 2.4 Conservative lower bound

For fixed \(p\), \(F_{\mathrm{remote}}=\sum_u r_u|D_u|\). Any connected route to \(d\) destinations uses at least \(d\) links, so \(C_{\mathrm{hop}}\ge F_{\mathrm{remote}}\). With \(M=2(R(C-1)+C(R-1))\) directed links, \(\max L\ge C_{\mathrm{hop}}/M\). Variance is nonnegative. Hence

\[
\mathcal{J}\ge \alpha F_{\mathrm{remote}}+\beta F_{\mathrm{remote}}/M.
\]

The bound is intentionally loose (`CostBreakdown::lower_bound`).

---

## 3. Algorithms

### 3.1 Graph baselines (Edge+QAP, Activity+QAP)

Balanced seed, FM-style boundary refinement on unit or activity-weighted edge cut, then multi-start QAP 2-swap on \(F_{ij}\) and a force-directed neighbor polish. This is Phase 2–3 machinery and the controlled baseline: Activity+QAP already sees rates and topology.

### 3.2 Spectral seed

Normalized hypergraph Laplacian after exploding each hyperedge into pairwise weights (Ronzani & Silvano style). Two nontrivial eigenvectors → 2-D embedding → nearest unused mesh cell. Applied at neuron level (partition seed) and core level (placement).

### 3.3 Multicast refinement

Greedy boundary moves under \(\mathcal{J}\). Candidate cores: all other cores when \(k\le 64\). Feasible if balance holds. Immediate accept of the best strictly improving move.

### 3.4 Locality and incremental gains

**Proposition (affected sources).** A move of \(v\) can change only hyperedges in \(\mathcal{A}(v)=\{v\}\cup N^-(v)\).

`IncrementalEvaluator` caches \(T_u\) per source. A peek copies the load vector (\(M\le 200\)), subtracts \(\mathcal{A}(v)\), recomputes those sources, and re-aggregates. Tests require \(|\Delta\mathcal{J}|<10^{-9}\) versus `evaluate()`.

Placement 2-swaps rebuild routes under the swapped \(\pi\) (still \(O(n)\) per pair).

### 3.5 Threads

`parallel_for` shards candidate evaluations across `std::thread` workers when the batch has at least 32 jobs. Smaller peeks stay serial (spawn overhead). `hysmap compare` runs independent mapper pipelines concurrently.

### 3.6 Complexity (qualitative)

Let \(H\) be the mesh diameter and \(M\) the link count. Full evaluation is \(\sum_u(\deg^+(u)+|D_u|H)+O(M)\). Incremental replaces the sum by \(u\in\mathcal{A}(v)\). Placement 2-swap search is \(O(k^2)\) candidate evaluations per pass.

---

## 4. Implementation

C++20 library (`include/hysmap/`, `src/`), CMake ≥ 3.20, Eigen 3.4 (dense `SelfAdjointEigenSolver`), Catch2 v3, optional pybind11. No GPU. Deterministic `std::mt19937_64` seeds.

CLI: `generate`, `map`, `compare`, `export`, `bench`, `demo`.

Loihi-style export writes cores, mesh coordinates, soma compartments, axon fanout, and destination-core lists. It is labeled as a **research export**, not an Intel NxSDK / Lava artifact.

Python module `hysmap` exposes generate / evaluate / run_mapper / export.

---

## 5. Experiments

Workloads: Potjans-inspired layered E/I nets (relative population sizes from Potjans & Diesmann, *Cerebral Cortex* 24:785–806, 2014), 80 / 108 / 160 neurons, meshes 4×4–7×7. Same generated graph and activity vector for every mapper in a job.

Primary metric: \(C_{\mathrm{hop}}\). Full CSV: `results/bench.csv`.

On that suite HySMap reduced routed hops by **16.2–25.3%** versus Edge+QAP and **10.9–21.5%** versus Activity+QAP (mean over seeds; see `results/SUMMARY.md`). Incremental evaluation was **2.3–5.1×** faster than full recompute in the original evidence run; a later 108-neuron / 6×6 check measured **4.44×**. Four-thread candidate evaluation measured **1.66×** (80 / 4×4) and **2.56×** (108 / 6×6) versus a serial peek of the same batch.

These numbers are **this binary’s**. They are not Table 2 of arXiv:2608.26223.

---

## 6. Limitations

- Scale: tens to low hundreds of neurons, not chip-scale SNNs.
- Routing: fixed XY. Another deterministic policy can replace `xy_path` / `multicast_union` without changing the locality theorem.
- Search: greedy, monotone on accepted moves, not globally optimal.
- Activity: synthetic rates, one static vector.
- Threads: help large candidate batches; they do not magically speed 16-core peeks.
- Export: Loihi-*style* JSON, not a vendor SDK.

---

## 7. Future work

pybind11 notebooks; NEST / Brian2 / NxNet importers; west-first and hardware multicast trees; a GUI; calibrated energy only with a public cost model; Loihi 2 / SpiNNaker backends if a documented subset can be matched honestly.

---

## References

1. M. Ronzani and C. Silvano. A Case for Hypergraphs to Model and Map SNNs on Neuromorphic Hardware. arXiv:2601.16118, 2026.
2. A. Khorasanian. Beyond Edge Cuts: Activity-Weighted Multicast Hypergraph Mapping for Spiking Neural Networks on Mesh NoCs. arXiv:2608.26223, 2026.
3. T. C. Potjans and M. Diesmann. The Cell-Type Specific Cortical Microcircuit. Cerebral Cortex 24:785–806, 2014.
4. W. Maass. Networks of Spiking Neurons. Neural Networks 10:1659–1671, 1997.
5. C. M. Fiduccia and R. M. Mattheyses. A Linear-Time Heuristic for Improving Network Partitions. DAC 1982.
6. T. C. Koopmans and M. Beckmann. Assignment Problems and the Location of Economic Activities. Econometrica 25:53–76, 1957.
7. D. Zhou, J. Huang, and B. Schölkopf. Learning with Hypergraphs. NeurIPS 2006.
