#!/usr/bin/env python3
"""Build a short demo animation from real HySMap mappings (CLI JSON)."""

from __future__ import annotations

import argparse
import json
import subprocess
import tempfile
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from matplotlib import animation
from matplotlib.patches import FancyBboxPatch


def run_map(hysmap: str, net: Path, extra: list[str]) -> dict:
    cmd = [hysmap, "map", "--input", str(net), "--mesh", "4", "--json", *extra]
    out = subprocess.check_output(cmd, text=True)
    # JSON may be followed by "wrote ..." — take the first object.
    start = out.find("{")
    end = out.rfind("}")
    return json.loads(out[start : end + 1])


def occupancy(assign: list[int], n: int = 4) -> np.ndarray:
    grid = np.zeros((n, n), dtype=float)
    for core in assign:
        grid[core // n, core % n] += 1
    return grid


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--hysmap", default="build/hysmap")
    ap.add_argument("--net", default="examples/net_potjans_80.json")
    ap.add_argument("--out", default="docs/assets")
    args = ap.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    demo = Path("results/demo")
    demo.mkdir(parents=True, exist_ok=True)

    stages = [
        ("random", ["--seed-strategy", "random", "--refine", "none", "--mapper", "edge-qap"]),
        ("greedy", ["--seed-strategy", "random", "--refine", "greedy", "--mapper", "edge-qap"]),
        ("activity-qap", ["--mapper", "activity-qap"]),
        ("hysmap", ["--mapper", "hysmap-seeded", "--time-incremental"]),
    ]
    snaps = []
    for name, flags in stages:
        js = run_map(args.hysmap, Path(args.net), flags + ["--seed", "1"])
        snaps.append((name, js))
        print(f"{name:12s} hops={js['metrics']['multicast_hops']:.1f} J={js['metrics']['objective']:.1f}")

    # Storyboard PNGs
    fig, axes = plt.subplots(1, 4, figsize=(11.2, 3.1))
    vmax = max(occupancy(s[1]["assignment"]).max() for s in snaps)
    for ax, (name, js) in zip(axes, snaps):
        im = ax.imshow(occupancy(js["assignment"]), cmap="YlGn", vmin=0, vmax=vmax)
        ax.set_title(f"{name}\nJ={js['metrics']['objective']:.0f}", fontsize=9)
        ax.set_xticks(range(4))
        ax.set_yticks(range(4))
    fig.colorbar(im, ax=axes, fraction=0.02, pad=0.02, label="neurons / core")
    fig.suptitle("random placement → HySMap  ·  80 neurons, 4×4, seed=1", fontsize=11)
    fig.tight_layout()
    story = out / "storyboard.png"
    fig.savefig(story, dpi=150)
    fig.savefig(demo / "storyboard.png", dpi=150)
    plt.close(fig)

    # Interpolate occupancy + cost for animation
    grids = [occupancy(s[1]["assignment"]) for s in snaps]
    costs = [s[1]["metrics"]["objective"] for s in snaps]
    frames = 48
    interp_g, interp_c = [], []
    for i in range(len(grids) - 1):
        for t in np.linspace(0, 1, frames // (len(grids) - 1), endpoint=False):
            interp_g.append((1 - t) * grids[i] + t * grids[i + 1])
            interp_c.append((1 - t) * costs[i] + t * costs[i + 1])
    interp_g.append(grids[-1])
    interp_c.append(costs[-1])

    fig, (ax_h, ax_c) = plt.subplots(1, 2, figsize=(8.6, 3.6), gridspec_kw={"width_ratios": [1, 1.15]})
    hm = ax_h.imshow(interp_g[0], cmap="YlGn", vmin=0, vmax=vmax, animated=True)
    ax_h.set_title("Core occupancy")
    ax_h.set_xticks(range(4))
    ax_h.set_yticks(range(4))
    (line,) = ax_c.plot([], [], color="#1b7f4e", lw=2)
    ax_c.set_xlim(0, len(interp_c) - 1)
    ax_c.set_ylim(min(interp_c) * 0.92, max(interp_c) * 1.04)
    ax_c.set_xlabel("frame")
    ax_c.set_ylabel("objective J")
    ax_c.set_title("Cost: random → HySMap")
    ax_c.spines["top"].set_visible(False)
    ax_c.spines["right"].set_visible(False)
    badge = FancyBboxPatch((0.02, 0.82), 0.42, 0.14, transform=ax_h.transAxes,
                           boxstyle="round,pad=0.02", facecolor="white", edgecolor="#1b7f4e")
    ax_h.add_patch(badge)
    label = ax_h.text(0.05, 0.88, "", transform=ax_h.transAxes, fontsize=8, color="#1b7f4e")

    stage_at = []
    for i, _ in enumerate(snaps):
        stage_at.append(i * (frames // (len(snaps) - 1)))

    def stage_name(fi: int) -> str:
        idx = min(len(snaps) - 1, fi * (len(snaps) - 1) // max(1, len(interp_c) - 1))
        return snaps[idx][0]

    def update(fi: int):
        hm.set_data(interp_g[fi])
        line.set_data(range(fi + 1), interp_c[: fi + 1])
        label.set_text(f"{stage_name(fi)}   J={interp_c[fi]:.0f}")
        return hm, line, label

    anim = animation.FuncAnimation(fig, update, frames=len(interp_g), interval=70, blit=True)
    gif = out / "hysmap_demo.gif"
    anim.save(gif, writer="pillow", fps=14, dpi=120)
    anim.save(demo / "hysmap_demo.gif", writer="pillow", fps=14, dpi=120)
    mp4 = out / "hysmap_demo.mp4"
    try:
        anim.save(mp4, writer="ffmpeg", fps=14, dpi=140)
        anim.save(demo / "hysmap_demo.mp4", writer="ffmpeg", fps=14, dpi=140)
        print(f"wrote {mp4}")
    except Exception as exc:  # noqa: BLE001
        print(f"mp4 skipped ({exc})")
    plt.close(fig)
    print(f"wrote {gif} and {story}")


if __name__ == "__main__":
    main()
