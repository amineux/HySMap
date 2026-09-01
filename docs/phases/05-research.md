# Phase 5 — Research

Compare *representations*, not just optimizers. Same SNN, same seed, same mesh. If Activity+QAP already knows who spikes, leftover hop reduction is from dest-core fanout and shared XY links — not from a fancier local search.

```bash
./build/hysmap compare --input examples/net_potjans_80.json --mesh 4 --seed 1 --csv out.csv
./build/hysmap bench --quick --out results/bench.csv
.venv/bin/python scripts/plot_results.py --csv results/bench.csv --out results/plots
```

## Ablation

| Mapper | What it optimizes |
| --- | --- |
| Edge+QAP | Unit edge cut + pairwise distance |
| Activity+QAP | Rate-weighted cut + pairwise distance |
| Spectral | Laplacian seed + multicast refine |
| HySMap-seeded | Activity+QAP then incremental multicast |
| HySMap | Portfolio incumbent under \(\mathcal{J}\) |

Activity+QAP already knows who spikes. Remaining hop reduction is from counting **destination-core fanout** and **shared XY links**.

## What we report

Hop tables in the README and [`results/SUMMARY.md`](../../results/SUMMARY.md) are **this repository’s** CLI runs. They are not the 115-job M-HySMap suite and not the large NMH mappings in arXiv:2601.16118.

Plots (generated from `results/bench.csv`):

- `results/plots/hops_by_mesh.png`
- `results/plots/hop_reduction.png`
- `results/plots/incremental_speedup.png`

Mean ± sample standard deviation over seeds is in the summary.

## Honest limits

Scaled Potjans-inspired nets (80–160 neurons). Traffic proxies, not energy. Greedy search, not global optimality.

Next: [Phase 6 — Publication quality](06-publication.md).
