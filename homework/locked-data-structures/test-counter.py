#!/bin/env python
import subprocess

import matplotlib.pyplot as plt

threads = list(range(1, 9))
add_types = ["precise", "approximate", "atomic"]

plt.figure(figsize=(8, 5))
for add_type in add_types:
    times = []
    for t in threads:
        res = subprocess.run(
            ["./counter", "-a", add_type, "-t", str(t)],
            check=True,
            text=True,
            capture_output=True,
        )
        times.append(float(res.stdout.strip()))

    plt.plot(
        times,
        marker="o",
        label=add_type,
    )


plt.title("Precise vs Aproximate Vs Atomic Counter Performances")
plt.xlabel("Threads")
plt.ylabel("Execution Time NS ( 1.000.000 Adds )")
plt.grid(True)
plt.legend()
plt.savefig("counter-benchmark.png", dpi=300)
