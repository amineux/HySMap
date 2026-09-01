#!/usr/bin/env python3
"""Zero-dep walkthrough: why a spike is not a graph edge.

Prints the numeric examples used in docs/examples/. XY routing is X-then-Y,
matching include/hysmap/mesh.hpp. No HySMap build required.
"""

from __future__ import annotations

from typing import Iterable


Coord = tuple[int, int]
Link = tuple[Coord, Coord]


def manhattan(a: Coord, b: Coord) -> int:
    return abs(a[0] - b[0]) + abs(a[1] - b[1])


def xy_path(src: Coord, dst: Coord) -> list[Link]:
    """Deterministic X-then-Y path (empty if src == dst)."""
    if src == dst:
        return []
    x, y = src
    tx, ty = dst
    links: list[Link] = []
    while x != tx:
        nx = x + (1 if tx > x else -1)
        links.append(((x, y), (nx, y)))
        x = nx
    while y != ty:
        ny = y + (1 if ty > y else -1)
        links.append(((x, y), (x, ny)))
        y = ny
    return links


def multicast_union(src: Coord, dests: Iterable[Coord]) -> set[Link]:
    links: set[Link] = set()
    for d in dests:
        if d == src:
            continue
        links.update(xy_path(src, d))
    return links


def ascii_mesh(marks: dict[Coord, str], n: int = 4) -> str:
    rows = []
    for y in range(n):
        cells = []
        for x in range(n):
            cells.append(marks.get((x, y), "·"))
        rows.append("  ".join(cells))
    return "\n".join(rows)


def section_hyperedge() -> None:
    print("=== 1. Hyperedge vs edges (one spike, five destinations) ===")
    src = (0, 0)
    dests = [(1, 0), (2, 0), (3, 0), (3, 1), (3, 2)]
    pair = sum(manhattan(src, d) for d in dests)
    union = multicast_union(src, dests)
    print("4×4 mesh, source S at (0,0), dests along a spine:")
    marks = {src: "S"}
    for i, d in enumerate(dests, 1):
        marks[d] = str(i)
    print(ascii_mesh(marks))
    print(f"graph cost (sum of pairwise Manhattan hops): {pair}")
    print(f"multicast cost (unique XY links in the union): {len(union)}")
    print(f"ratio: {pair / len(union):.1f}× overcount if you bill every synapse")
    print("shared prefix (0,0)→(1,0)→(2,0)→(3,0) is paid once, not 1+2+3+3+3 times")
    print()


def section_activity() -> None:
    print("=== 2. Activity weighting (same wires, different rates) ===")
    # Two sources on a 4×4. Same dest cores. Only the rates change.
    # Hot axon H at (0,1) talks to (3,1) and (3,2).
    # Cold axon C at (0,2) talks to (0,3) — already local-ish.
    h_src, h_dests = (0, 1), [(3, 1), (3, 2)]
    c_src, c_dests = (0, 2), [(0, 3)]
    h_hops = len(multicast_union(h_src, h_dests))  # X:3 + Y:1 = 4
    c_hops = len(multicast_union(c_src, c_dests))  # Y:1 = 1
    print("placement A — hot axon has a long spine, cold axon is local:")
    print(f"  |T_H|={h_hops}  |T_C|={c_hops}")
    for name, rh, rc in (("quiet H / busy C", 1.0, 10.0), ("busy H / quiet C", 10.0, 1.0)):
        cost = rh * h_hops + rc * c_hops
        print(f"  rates ({name}): C_hop = {rh}*{h_hops} + {rc}*{c_hops} = {cost:.0f}")
    # Swap H onto the spine dest side: H at (2,1) → (3,1),(3,2)
    h2 = (2, 1)
    h2_hops = len(multicast_union(h2, h_dests))
    print(f"placement B — move the hot source to (2,1): |T_H|={h2_hops}")
    print(f"  busy H: C_hop = 10*{h2_hops} + 1*{c_hops} = {10 * h2_hops + c_hops:.0f}")
    print("same topology. the mapper should drag the loud axon toward its dest cores.")
    print()


def section_partition() -> None:
    print("=== 3. Partition then place (12 neurons, capacity 3, 2×2) ===")
    place = {0: (0, 0), 1: (1, 0), 2: (0, 1), 3: (1, 1)}

    def hops(assign: dict[int, int]) -> tuple[int, int]:
        remote = 0
        total = 0
        for u in range(11):
            if assign[u] == assign[u + 1]:
                continue
            remote += 1
            total += len(multicast_union(place[assign[u]], [place[assign[u + 1]]]))
        return remote, total

    packed = {n: n // 3 for n in range(12)}  # 0-2, 3-5, 6-8, 9-11
    striped = {n: n % 4 for n in range(12)}  # 0,4,8 / 1,5,9 / …
    pr, ph = hops(packed)
    sr, sh = hops(striped)
    print("chain 0→1→…→11, three neurons per core, cores at the 2×2 corners")
    print(f"  packed (neighbors share a core):   remote synapses={pr}  C_hop={ph}")
    print(f"  striped (n goes to core n%4):      remote synapses={sr}  C_hop={sh}")
    print("partition decides who shares a core (0 hops). placement prices the rest.")
    print()


def section_incremental() -> None:
    print("=== 4. Incremental ΔJ (move one neuron) ===")
    # Source 0 at (0,0) spikes to 1,2,3 sitting at (3,0),(3,1),(3,2).
    # Neuron 4 at (1,1) spikes to 1 — so 1 has a predecessor besides nobody else.
    # Move neuron 1 from (3,0) → (1,0).
    src0 = (0, 0)
    dests_before = [(3, 0), (3, 1), (3, 2)]
    dests_after = [(1, 0), (3, 1), (3, 2)]
    src4 = (1, 1)
    t0_b = multicast_union(src0, dests_before)
    t0_a = multicast_union(src0, dests_after)
    t4_b = multicast_union(src4, [(3, 0)])
    t4_a = multicast_union(src4, [(1, 0)])
    hops_b = len(t0_b) + len(t4_b)
    hops_a = len(t0_a) + len(t4_a)
    print("sources that can change when we move neuron 1:")
    print("  A(1) = {1} ∪ N⁻(1) = {1, 0, 4}")
    print("  (1 has no outgoing axon in this toy, so only h_0 and h_4 reroute)")
    print(f"  before: |T_0|={len(t0_b)}  |T_4|={len(t4_b)}  C_hop={hops_b}")
    print(f"  after:  |T_0|={len(t0_a)}  |T_4|={len(t4_a)}  C_hop={hops_a}")
    print(f"  ΔC_hop = {hops_a - hops_b}")
    print("neuron 2 and 3 keep their cached routes. we never resimulate the whole net.")
    print()


def main() -> None:
    section_hyperedge()
    section_activity()
    section_partition()
    section_incremental()
    # Lock the numbers the README quotes.
    src, dests = (0, 0), [(1, 0), (2, 0), (3, 0), (3, 1), (3, 2)]
    pair = sum(manhattan(src, d) for d in dests)
    uni = len(multicast_union(src, dests))
    if pair != 15 or uni != 5:
        raise SystemExit(f"intuition drift: expected 15 vs 5, got {pair} vs {uni}")


if __name__ == "__main__":
    main()
