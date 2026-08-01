#!/bin/env python
import subprocess

import matplotlib.pyplot as plt
from matplotlib.ticker import ScalarFormatter

TRIES = 10000

access_times = []
num_pages = []

for i in range(1, 13):
    n = 2**i
    result = subprocess.run(
        ["./tlb", "-p", str(n), "-t", str(TRIES)],
        check=True,
        text=True,
        capture_output=True,
    )

    num_pages.append(n)
    access_times.append(float(result.stdout.strip()))

plt.figure(figsize=(8, 5))
plt.plot(
    num_pages,
    access_times,
    marker="o",
    linestyle="-",
    color="b",
    label="Access Time Per Page",
)

plt.xscale("log", base=2)
plt.xticks(num_pages, labels=[str(p) for p in num_pages])

plt.gca().xaxis.set_major_formatter(ScalarFormatter())

plt.title("TLB Access Time vs Number of Pages")
plt.xlabel("Number of Pages")
plt.ylabel("Time per Access (ns)")
plt.grid(True, which="both", ls="--")
plt.legend()

plt.savefig("access_time_number_of_pages", dpi=300)
