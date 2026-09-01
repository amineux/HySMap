# Worked example: hyperedge vs edges

**Phase 1.** One spike, five destinations, a 4×4 mesh. We count two ways.

```
x →
y   S ── 1 ── 2 ── 3
↓                 │
                  4
                  │
                  5
```

| Neuron | Coord | Manhattan from S(0,0) |
| --- | --- | ---: |
| S | (0,0) | — |
| 1 | (1,0) | 1 |
| 2 | (2,0) | 2 |
| 3 | (3,0) | 3 |
| 4 | (3,1) | 4 |
| 5 | (3,2) | 5 |
| **graph total** | | **15** |

That's what a pairwise hop model bills: five unicasts.

The chip does **X-then-Y** from S to each dest core and **unions** the links:

```
(0,0)→(1,0)→(2,0)→(3,0)→(3,1)→(3,2)
```

Five unique directed links. Dest 2 reuses dest 1's prefix. Dest 5 reuses dest 3 and dest 4.

| Model | Cost |
| --- | ---: |
| Σ Manhattan (graph) | **15** |
| \|XY union\| (hypergraph / multicast) | **5** |

**3× overcount.** The hyperedge is `h_S = S → {1,2,3,4,5}` with weight `r_S`. HySMap stores that once (`DirectedHypergraph`) and scores `|T_S|`, not the edge list.

Same-core collapse is the other half of the lie. Put dests 3, 4, 5 on **one** remote core and a graph cut still sees three remote synapses. The mesh sees one destination core and one XY path.

Reproduce:

```bash
python3 scripts/intuition.py   # section 1
```

See also [`docs/algorithm.md`](../algorithm.md) §2–3 and [`docs/phases/01-tiny-simulator.md`](../phases/01-tiny-simulator.md).
