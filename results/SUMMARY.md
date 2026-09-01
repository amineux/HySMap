# Measured mapping results (this repository)

These numbers come from `results/bench.csv`, produced by the `hysmap compare` CLI on
Potjans-inspired recurrent SNNs. They are **not** copied from arXiv:2601.16118 or
arXiv:2608.26223.

Primary metric: activity-weighted **routed multicast hops** \(C_{\mathrm{hop}}=\sum_u r_u|T_u|\)
under deterministic XY routing. Values are **mean ± sample s.d.** over seeds (single-seed rows have no ±).

| Neurons | Mesh | Seeds | Edge+QAP | Activity+QAP | HySMap | vs Edge | vs Activity |
| ---: | :---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 80 | 4×4 | 3 | 4552±251 | 4515±559 | 3543±258 | 22.2% | 21.5% |
| 80 | 5×5 | 3 | 5727±140 | 5270±43 | 4483±65 | 21.7% | 14.9% |
| 80 | 6×6 | 3 | 6354±178 | 5979±226 | 5325±61 | 16.2% | 10.9% |
| 108 | 4×4 | 3 | 6489±87 | 6280±226 | 5143±188 | 20.7% | 18.1% |
| 108 | 5×5 | 3 | 8683±388 | 8241±177 | 7022±303 | 19.1% | 14.8% |
| 108 | 6×6 | 3 | 10403±615 | 9811±340 | 7775±371 | 25.3% | 20.8% |
| 108 | 7×7 | 2 | 12022±352 | 11501±533 | 9873±750 | 17.9% | 14.2% |
| 160 | 6×6 | 1 | 17108 | 16199 | 12820 | 25.1% | 20.9% |

HySMap hop reduction vs Edge+QAP: **16.2–25.3%**. vs Activity+QAP: **10.9–21.5%**.

Threading (Phase 4) does not change the hop table: candidate peeks are evaluated in parallel, then the same best move is committed. A post-thread check on seed=1, 80 neurons, 4×4, `hysmap-seeded` still reports **3557.069** hops.

### Incremental and threads (same binary)

| Workload | Incremental vs full \(\Delta\mathcal{J}\) | 4-thread gain batch |
| --- | ---: | ---: |
| 80 neurons, 4×4, seeded | 2.66× | 1.66× |
| 108 neurons, 6×6, seeded | 4.44× | 2.56× |

Original suite incremental speedups across jobs: **2.3–5.1×**, \(\|\mathcal{A}(v)\|\approx 9–14\), max \(|\Delta\mathcal{J}|\) at floating-point noise.

Plots: [`results/plots/`](plots/). Demo animation: [`results/demo/`](demo/).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
./benchmarks/run_suite.sh results/bench.csv quick
python scripts/plot_results.py --csv results/bench.csv --out results/plots
```
