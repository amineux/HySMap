#!/usr/bin/env python3
"""Publication-style plots from results/bench.csv (this repo's runs only)."""

from __future__ import annotations

import argparse
import csv
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

MAPPERS = ["edge-qap", "activity-qap", "spectral", "hysmap-seeded", "hysmap"]
COLORS = {
    "edge-qap": "#7a7a7a",
    "activity-qap": "#3d6a9a",
    "spectral": "#6b4c9a",
    "hysmap-seeded": "#c47b2b",
    "hysmap": "#1b7f4e",
}


def load(path: Path) -> list[dict]:
    with path.open() as f:
        return list(csv.DictReader(f))


def grouped(rows: list[dict]) -> dict:
    g: dict[tuple[int, int], dict[str, list[float]]] = defaultdict(lambda: defaultdict(list))
    for r in rows:
        key = (int(r["neurons"]), int(r["rows"]))
        g[key][r["mapper"]].append(float(r["hops"]))
    return g


def plot_hops(g, out: Path) -> None:
    keys = sorted(g)
    labels = [f"n{n}\n{m}×{m}" for n, m in keys]
    x = np.arange(len(keys))
    width = 0.15
    fig, ax = plt.subplots(figsize=(10.5, 4.6))
    for i, mapper in enumerate(MAPPERS):
        means = [float(np.mean(g[k][mapper])) if g[k][mapper] else 0.0 for k in keys]
        stds = [
            float(np.std(g[k][mapper], ddof=1)) if len(g[k][mapper]) > 1 else 0.0 for k in keys
        ]
        ax.bar(
            x + (i - 2) * width,
            means,
            width,
            yerr=stds,
            label=mapper,
            color=COLORS[mapper],
            capsize=2,
        )
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.set_ylabel("Routed multicast hops")
    ax.set_title("HySMap vs graph baselines (this repo, mean ± s.d.)")
    ax.legend(ncols=3, fontsize=8)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    fig.tight_layout()
    fig.savefig(out, dpi=160)
    plt.close(fig)


def plot_reduction(g, out: Path) -> None:
    keys = sorted(g)
    labels = [f"n{n} {m}×{m}" for n, m in keys]
    vs_e, vs_a = [], []
    for k in keys:
        e = float(np.mean(g[k]["edge-qap"]))
        a = float(np.mean(g[k]["activity-qap"]))
        h = float(np.mean(g[k]["hysmap"]))
        vs_e.append(100.0 * (1.0 - h / e))
        vs_a.append(100.0 * (1.0 - h / a))
    fig, ax = plt.subplots(figsize=(9.2, 4.4))
    x = np.arange(len(keys))
    ax.bar(x - 0.18, vs_e, 0.36, label="vs Edge+QAP", color="#7a7a7a")
    ax.bar(x + 0.18, vs_a, 0.36, label="vs Activity+QAP", color="#1b7f4e")
    ax.set_xticks(x)
    ax.set_xticklabels(labels, rotation=20, ha="right")
    ax.set_ylabel("Hop reduction (%)")
    ax.set_title("HySMap hop reduction (measured here, not paper tables)")
    ax.legend()
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    fig.tight_layout()
    fig.savefig(out, dpi=160)
    plt.close(fig)


def plot_inc(rows: list[dict], out: Path) -> None:
    pts = []
    for r in rows:
        if r["mapper"] != "hysmap":
            continue
        sp = float(r["inc_speedup"])
        if sp <= 0:
            continue
        pts.append((int(r["neurons"]), int(r["rows"]), sp))
    if not pts:
        return
    fig, ax = plt.subplots(figsize=(7.2, 4.2))
    for mesh in sorted({p[1] for p in pts}):
        xs = [p[0] for p in pts if p[1] == mesh]
        ys = [p[2] for p in pts if p[1] == mesh]
        ax.scatter(xs, ys, label=f"{mesh}×{mesh}", s=36)
    ax.set_xlabel("Neurons")
    ax.set_ylabel("Incremental vs full recompute (×)")
    ax.set_title("Incremental ΔJ speedup (same candidate list)")
    ax.legend()
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    fig.tight_layout()
    fig.savefig(out, dpi=160)
    plt.close(fig)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", type=Path, default=Path("results/bench.csv"))
    ap.add_argument("--out", type=Path, default=Path("results/plots"))
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    rows = load(args.csv)
    g = grouped(rows)
    plot_hops(g, args.out / "hops_by_mesh.png")
    plot_reduction(g, args.out / "hop_reduction.png")
    plot_inc(rows, args.out / "incremental_speedup.png")
    print(f"wrote plots under {args.out}")


if __name__ == "__main__":
    main()
