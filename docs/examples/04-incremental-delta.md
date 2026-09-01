# Worked example: incremental ΔJ

**Phase 4.** You don't reroute the whole SNN to peek a move. You reroute the sources that can notice.

Moving neuron `v` changes `p(v)` only. The sources whose dest-core set can change are

\[
\mathcal{A}(v)=\{v\}\cup N^-(v).
\]

That's `v`'s own axon plus anyone who already targeted `v`.

### Toy

- Neuron 0 at (0,0) spikes to `{1, 2, 3}` sitting at `(3,0), (3,1), (3,2)`.
- Neuron 4 at (1,1) spikes to `{1}`.
- Neuron 1 has no outgoing axon.
- Move **1** from `(3,0)` to `(1,0)`.

So `N⁻(1) = {0, 4}` and `A(1) = {1, 0, 4}`. Neurons 2 and 3 keep their cached `T_u`.

| Source | Before \|T\| | After \|T\| |
| --- | ---: | ---: |
| 0 (the fanout-3 axon) | 5 | 5 |
| 4 (the predecessor) | 3 | 1 |
| **C_hop** | **8** | **6** |

`ΔC_hop = −2`. Source 0 still needs the long spine to reach 2 and 3, so its union length doesn't drop. Source 4's unicast shortens `(1,1)→(3,0)` (3 links) to `(1,1)→(1,0)` (1 link).

`IncrementalEvaluator` subtracts the cached unions for `A(v)`, writes the new ones, and rebuilds max-load / variance from the load vector. Catch2 requires that peek to match a full `evaluate()` to `1e-9`.

```bash
python3 scripts/intuition.py   # section 4
./build/hysmap demo --mesh 4 --neurons 80 --seed 1 --mapper hysmap-seeded --time-incremental
```

[`docs/phases/04-performance.md`](../phases/04-performance.md) · [`docs/algorithm.md`](../algorithm.md) §5
