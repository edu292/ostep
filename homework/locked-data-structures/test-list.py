#!/usr/bin/env python
import subprocess

import matplotlib.pyplot as plt

threads = list(range(1, 9))
strategies = ["single", "double", "hand-over-hand"]
modes = ["append", "get", "combined"]

fig, axes = plt.subplots(1, 3, figsize=(15, 4.5), sharey=True, sharex=True)

for ax, mode in zip(axes, modes):
    for strat in strategies:
        times = []
        for t in threads:
            res = subprocess.run(
                ["./list", "-s", strat, "-m", mode, "-t", str(t)],
                capture_output=True,
                text=True,
                check=True,
            )
            times.append(float(res.stdout.strip()))

        ax.plot(
            threads,
            times,
            marker="o",
            label=strat,
            alpha=0.7,
            linestyle="--" if strat == "single" else "-",
        )
    ax.set_title(mode.capitalize())
    ax.grid(True)
    ax.legend()

axes[0].set_xlabel("Threads")
axes[0].set_ylabel("Execution Time (s)")
plt.tight_layout()
plt.savefig("list-benchmark.png", dpi=300)
