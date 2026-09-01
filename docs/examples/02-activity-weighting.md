# Worked example: activity weighting

**Phase 3.** Same wires. Two rate profiles. The cheap mapping flips.

Two axons on a 4×4. XY multicast sizes (from `scripts/intuition.py`):

| Axon | Source | Dest cores | \|T\| |
| --- | --- | --- | ---: |
| H (hot or not) | (0,1) | (3,1), (3,2) | **4** |
| C (local) | (0,2) | (0,3) | **1** |

`C_hop = r_H · 4 + r_C · 1`

| Profile | r_H | r_C | C_hop |
| --- | ---: | ---: | ---: |
| Quiet H, busy C | 1 | 10 | **14** |
| Busy H, quiet C | 10 | 1 | **41** |

Same placement, almost 3× the traffic, just because the loud axon sits on the long spine.

Move only H to (2,1). The dests don't move. `|T_H|` drops 4 → 2.

| | C_hop (busy H) |
| --- | ---: |
| H at (0,1) | 10·4 + 1·1 = **41** |
| H at (2,1) | 10·2 + 1·1 = **21** |

The mapper should drag the high-rate source toward its dest cores. Edge+QAP can't see that — every synapse is 1. Activity+QAP uses `r_u` in the pairwise flow `F_ij`. HySMap still scores the **union**, so two dests that share a prefix don't get billed twice after the move.

```bash
python3 scripts/intuition.py   # section 2
./build/hysmap map --input examples/net_potjans_80.json --mesh 4 \
    --mapper activity-qap --seed 1
```

[`docs/phases/03-intelligence.md`](../phases/03-intelligence.md)
