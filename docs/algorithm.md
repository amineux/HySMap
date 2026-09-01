# HySMap algorithms

This note is an engineer-level walkthrough of what the C++ library actually optimizes.
The implementation is **inspired by** two public preprints; it is not a dump of the authors’ code and does not claim their experimental tables.

| Idea | Source |
| --- | --- |
| Raise SNN mapping from graphs to source-rooted hypergraphs; hyperedge overlap / locality; spectral (Laplacian) placement | Ronzani & Silvano, *A Case for Hypergraphs to Model and Map SNNs on Neuromorphic Hardware*, [arXiv:2601.16118](https://arxiv.org/abs/2601.16118) |
| Activity-weighted directed hyperedges; route-union multicast hops + congestion; exact incremental gain from locality; conservative placement lower bound | Khorasanian, *Beyond Edge Cuts: Activity-Weighted Multicast Hypergraph Mapping for Spiking Neural Networks on Mesh NoCs* (M-HySMap), [arXiv:2608.26223](https://arxiv.org/abs/2608.26223) |

---

## 1. The communication object

An SNN is stored as a **directed hypergraph**. Neuron \(u\) owns at most one hyperedge

\[
h_u = (u \rightarrow N^+(u))
\]

weighted by a profiled (or synthetic) spike rate \(r_u \ge 0\). One spike is one multicast event. If twenty postsynaptic targets sit on the same remote core, the NoC should see **one** destination core, not twenty graph edges.

A mapping is a pair \((p,\pi)\):

- \(p : V \rightarrow \{0,\ldots,k-1\}\) assigns neurons to logical cores,
- \(\pi\) injectively places those logical cores on the \(R\times C\) mesh.

The physical core of neuron \(v\) is \(\pi(p(v))\).

## 2. Why a graph cut is the wrong objective

The unweighted edge cut

\[
C_{\mathrm{cut}}(p)=\sum_{(u,v)\in E}\mathbf{1}[p(u)\neq p(v)]
\]

counts remote **synapses**. Hardware first collapses targets onto distinct destination cores

\[
D_u(p)=\{p(v):v\in N^+(u),\ p(v)\neq p(u)\}.
\]

Two partitions can share \(C_{\mathrm{cut}}\) and differ in \(\sum_u |D_u|\). That is the “many-to-one collapse” observation from M-HySMap.

## 3. Route-aware multicast cost

Let \(\mathrm{XY}(a,b)\) be the deterministic **X-then-Y** Manhattan path. For source \(u\),

\[
T_u(p,\pi)=\bigcup_{c\in D_u(p)}\mathrm{XY}\bigl(\pi(p(u)),\pi(c)\bigr).
\]

Shared prefixes appear once. This is the exact route set under XY, **not** a Steiner tree and not an adaptive multicast fabric.

\[
C_{\mathrm{hop}}=\sum_{u} r_u\,|T_u|,\qquad
L_\ell=\sum_{u} r_u\,\mathbf{1}[\ell\in T_u].
\]

The composite objective used by the mapper (defaults \(\alpha=1\), \(\beta=0.10\), \(\gamma=0.01\)) is

\[
\mathcal{J}=\alpha C_{\mathrm{hop}}+\beta\max_\ell L_\ell+\gamma\operatorname{Var}_\ell(L_\ell)+\delta_s I_s+\delta_r I_r.
\]

Soft imbalance weights are zero by default. Hard balance uses slack \(s=0.15\):

\[
\bigl\lfloor(1-s)n/k\bigr\rfloor \le |p^{-1}(c)| \le \bigl\lceil(1+s)n/k\bigr\rceil
\]

and the per-core neuron capacity.

These terms are **mapping-level traffic proxies**. They are not calibrated energy, latency, or cycle counts.

## 4. Conservative lower bound

For a **fixed** partition \(p\), define \(F_{\mathrm{remote}}=\sum_u r_u |D_u|\). Any connected route union to \(d\) destinations uses at least \(d\) links, so \(C_{\mathrm{hop}}\ge F_{\mathrm{remote}}\). If the mesh has \(M=2(R(C-1)+C(R-1))\) directed adjacent links, then \(\max_\ell L_\ell \ge C_{\mathrm{hop}}/M\). Variance is nonnegative. Therefore

\[
\mathcal{J}(p,\pi)\ \ge\ \alpha F_{\mathrm{remote}}+\beta\frac{F_{\mathrm{remote}}}{M}+\delta_s I_s+\delta_r I_r.
\]

The bound is intentionally loose. It is a sanity check (`CostBreakdown::lower_bound`), not a claim of optimality.

## 5. Affected-source locality

Moving neuron \(v\) changes only \(p(v)\). The only source-rooted hyperedges that can change are

\[
\mathcal{A}(v)=\{v\}\cup N^-(v).
\]

Proof sketch: \(h_v\) sees a new source core. For \(u\neq v\), \(D_u\) depends only on labels of \(N^+(u)\). If \((u,v)\notin E\), those labels are unchanged, so \(T_u\) and the link-load contribution of \(u\) are unchanged.

`IncrementalEvaluator` caches \((D_u,T_u,r_u|T_u|)\) per source. A peek subtracts \(\mathcal{A}(v)\), recomputes those sources under the tentative label, and re-aggregates \(\max\) / variance from the updated load vector. Unit tests require the peeked \(\mathcal{J}\) to match a full recompute to \(10^{-9}\).

Placement 2-swaps change many routes; those candidates are scored by rebuilding source routes under the swapped \(\pi\) (still cheap at \(k\le 49\)).

## 6. Search portfolio

| Stage | What it optimizes |
| --- | --- |
| **Edge+QAP** | FM-style boundary moves on unit edge cut; pairwise QAP \(\sum_{ij} F_{ij} D_{\pi(i)\pi(j)}\) with \(F_{ij}\) unweighted; force-directed neighbor polish |
| **Activity+QAP** | Same, but \(F_{ij}\) and the cut use \(r_u\) |
| **Spectral** | Neuron-level normalized hypergraph Laplacian → 2-D Fiedler embedding quantized onto cores (capacity-aware); core-level Laplacian placement (Ronzani & Silvano style); then multicast refine |
| **HySMap-seeded** | Activity+QAP seed, then incremental multicast partition moves + multicast placement swaps |
| **HySMap** | Portfolio incumbent: best \(\mathcal{J}\) among Activity+QAP, seeded refine, spectral refine, plus joint partition/placement cycles |

QAP is treated as a **strong seed**, not a straw man. Most of the hop reduction vs Activity+QAP should come from counting destination-core fanout and shared XY links instead of pairwise distance.

## 7. Spectral placement (hypergraph Laplacian)

Following Ronzani & Silvano §IV-B2, we explode each partition-level hyperedge into pairwise weights, form the **normalized Laplacian**

\[
\mathcal{L}_{ii}=1,\qquad
\mathcal{L}_{ij}=-\frac{1}{\sqrt{\mathrm{wdeg}(i)\,\mathrm{wdeg}(j)}}\sum_{e\ni\{i,j\}} w(e),
\]

take the two smallest *nontrivial* eigenpairs (Eigen `SelfAdjointEigenSolver`), min-max normalize the embedding, and discretize each logical core onto the nearest unused mesh coordinate.

## 8. Synthetic SNNs

`generate_snn` builds **Potjans-inspired** layered E/I populations (relative sizes from Potjans & Diesmann, *Cerebral Cortex* 24:785–806, 2014) with a published-style 8×8 connection-probability table, finite-size inflation so tiny nets still have fanout, lognormal rates, and extra same-layer recurrence. These are scaled research workloads, not a full-scale cortical microcircuit.

## 9. What we do not claim

- We did not reproduce the 115-job M-HySMap evidence suite or the large NMH mappings in arXiv:2601.16118.
- Hop reductions in the README are **this repo’s** measured CLI runs.
- XY multicast is a model of a deterministic mesh, not Loihi / TrueNorth / SpiNNaker firmware.
- The search is a greedy heuristic. It is monotone on accepted moves; it is not globally optimal.
