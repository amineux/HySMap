# Examples

| File | Use |
| --- | --- |
| `net_potjans_80.json` | 80-neuron Potjans-inspired hypergraph (`seed=1`) |
| `net_potjans_108.json` | 108-neuron sibling |
| `potjans_4x4.json` | Experiment recipe (generate + map settings) |
| `potjans_5x5.json` | Same on a 5×5 mesh |
| `layered_6x6.json` | Feed-forward + weak recurrence on 6×6 |

```bash
./build/hysmap map --input examples/net_potjans_80.json --mesh 4 --mapper hysmap --seed 1
./build/hysmap map --input examples/net_potjans_80.json --mesh 4 --seed-strategy random --refine greedy
./build/hysmap compare --input examples/net_potjans_108.json --mesh 5 --seed 1
./build/hysmap export --input examples/net_potjans_80.json --format loihi-json --out /tmp/loihi.json
```
