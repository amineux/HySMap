# Worked example: partition, then place

**Phase 2.** Twelve neurons, capacity 3, a 2×2 mesh (four cores). Chain `0→1→…→11`, every axon rate 1.

Cores sit at the corners:

```
core 0 (0,0)    core 1 (1,0)
core 2 (0,1)    core 3 (1,1)
```

Two partitions, same placement:

### Packed (neighbors share a core)

`p(n) = n // 3` → groups `{0,1,2}`, `{3,4,5}`, `{6,7,8}`, `{9,10,11}`.

On-core synapses are free. Only three axons leave a core:

| Remote axon | From → to | XY hops |
| --- | --- | ---: |
| 2 → 3 | (0,0) → (1,0) | 1 |
| 5 → 6 | (1,0) → (0,1) | 2 |
| 8 → 9 | (0,1) → (1,1) | 1 |
| **C_hop** | | **4** |

Remote synapses: **3**.

### Striped (the "random-looking" partition)

`p(n) = n % 4` → core 0 gets `{0,4,8}`, core 1 `{1,5,9}`, … Capacity still 3, so it's legal. Every hop in the chain is remote.

`C_hop = 16`. Remote synapses: **11**.

| Partition | Remote synapses | C_hop |
| --- | ---: | ---: |
| Packed | 3 | **4** |
| Striped | 11 | **16** |

**4×** more mesh traffic, same neurons, same cores, same capacity. Partition decides who shares a core (zero hops). Placement prices the axons that still leave.

A greedy *edge-cut* refine tries to cut those 11 remote synapses. It still doesn't know that 5→6 is a 2-hop diagonal while 2→3 is 1 hop. That's why Phase 4 scores the XY union.

```bash
python3 scripts/intuition.py   # section 3
./build/hysmap map --input examples/net_potjans_80.json --mesh 4 \
    --seed-strategy random --refine greedy --seed 1
```

[`docs/phases/02-mapper.md`](../phases/02-mapper.md)
