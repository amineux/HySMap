#!/usr/bin/env python3
"""Phase-1/2/3 walkthrough of the HySMap Python bindings."""

from __future__ import annotations

import hysmap as hy


def main() -> None:
    cfg = hy.GeneratorConfig()
    cfg.preset = hy.GeneratorPreset.Potjans
    cfg.target_neurons = 48
    cfg.seed = 1
    g = hy.generate_snn(cfg)
    print("network:", g.describe())

    mesh = hy.MeshNoC(4, 4)
    mc = hy.MapperConfig()
    mc.kind = hy.MapperKind.HySMapSeeded
    mc.seed = 1
    mc.threads = 2
    mc.time_incremental = True
    mc.hyper_passes = 2
    mc.joint_cycles = 0
    mc.placement_restarts = 2

    result = hy.run_mapper(g, mesh, mc)
    print(f"mapper: {result.mapper}")
    print(f"hops:   {result.metrics.hops:.2f}")
    print(f"J:      {result.metrics.objective:.2f}")
    print(f"bound:  {result.metrics.lower_bound:.2f}")
    print("assignment[:8]:", result.mapping.assignment()[:8])
    print(hy.loihi_style_json(g, mesh, result.mapping)[:240], "...")


if __name__ == "__main__":
    main()
