# Measured mapping results (this repository)

These numbers come from `results/bench.csv`, produced by the `hysmap compare` CLI on
Potjans-inspired recurrent SNNs. They are **not** copied from arXiv:2601.16118 or
arXiv:2608.26223.

Primary metric: activity-weighted **routed multicast hops** \(C_{\mathrm{hop}}=\sum_u r_u|T_u|\)
under deterministic XY routing.

| Neurons | Mesh | Seeds | Edge+QAP hops | Activity+QAP hops | HySMap hops | vs Edge | vs Activity |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 80 | 4×4 | 3 | 4552 | 4515 | 3543 | 22.2% | 21.5% |
| 80 | 5×5 | 3 | 5727 | 5270 | 4483 | 21.7% | 14.9% |
| 80 | 6×6 | 3 | 6354 | 5979 | 5325 | 16.2% | 10.9% |
| 108 | 4×4 | 3 | 6489 | 6280 | 5143 | 20.7% | 18.1% |
| 108 | 5×5 | 3 | 8683 | 8241 | 7022 | 19.1% | 14.8% |
| 108 | 6×6 | 3 | 10403 | 9811 | 7775 | 25.3% | 20.8% |
| 108 | 7×7 | 2 | 12022 | 11501 | 9873 | 17.9% | 14.2% |
| 160 | 6×6 | 1 | 17108 | 16199 | 12820 | 25.1% | 20.9% |

Across this suite HySMap reduced routed hops by **16.2–25.3%** vs Edge+QAP and
**10.9–21.5%** vs Activity+QAP.

Incremental vs full objective recompute on the same candidate moves:
**2.3–5.1×** faster (larger on the 160-neuron / 6×6 job), average \(|\mathcal{A}(v)|\approx 9–14\)
sources, maximum \(|\Delta\mathcal{J}|\) at floating-point noise (unit tests require \(<10^{-9}\)).

Reproduce:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
./benchmarks/run_suite.sh results/bench.csv quick
```
