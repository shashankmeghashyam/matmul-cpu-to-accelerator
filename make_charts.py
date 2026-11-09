# =============================================================================
# make_charts.py
#
# Reads my measured results and draws three charts for the README:
#   charts/1_speedup.png      real run time of each version on my laptop
#   charts/2_cache_misses.png simulated misses at each cache level
#   charts/3_validation.png   my simulator vs. Valgrind Cachegrind
#
# Inputs:  results.txt     (made by run_all.sh)
#          validation.txt  (made by validate.sh)
#
# How to run:   python3 make_charts.py
# =============================================================================

import os
import re

import matplotlib
matplotlib.use("Agg")            # draw to files, no window needed
import matplotlib.pyplot as plt

VERSIONS = ["naive", "interchange", "tiled"]
COLORS = ["#d9534f", "#f0ad4e", "#5cb85c"]   # red, orange, green


def to_number(text):
    """Turn '135,021,108' or '7282.45' into a number."""
    return float(text.replace(",", ""))


# -----------------------------------------------------------------------------
# 1) Read results.txt: real timings and simulated misses
# -----------------------------------------------------------------------------
times_ms = {}            # times_ms["naive"] = 7282.45
simulated = {}           # simulated["naive"] = {"L1": ..., "L2": ..., "L3": ...}
current_version = None

with open("results.txt") as file:
    for line in file:
        timing = re.search(r"Version: (\w+)\s+Size: 1024.*Time: ([\d.]+) ms", line)
        if timing:
            times_ms[timing.group(1)] = to_number(timing.group(2))

        header = re.search(r"Simulated caches for: (\w+)", line)
        if header:
            current_version = header.group(1)
            simulated[current_version] = {}

        level = re.search(r"^(L1|L2|L3) .*?([\d]+) misses", line)
        if level and current_version:
            simulated[current_version][level.group(1)] = to_number(level.group(2))

# -----------------------------------------------------------------------------
# 2) Read validation.txt: Cachegrind's L1 data cache misses
# -----------------------------------------------------------------------------
cachegrind_l1 = {}       # cachegrind_l1["naive"] = 135021108
current_version = None

with open("validation.txt") as file:
    for line in file:
        header = re.search(r"Cachegrind: ([\w-]+)", line)
        if header:
            current_version = header.group(1)

        d1 = re.search(r"D1 +misses:\s+([\d,]+)", line)
        if d1 and current_version:
            cachegrind_l1[current_version] = to_number(d1.group(1))

setup_cost = cachegrind_l1["setup-only"]

os.makedirs("charts", exist_ok=True)

# -----------------------------------------------------------------------------
# Chart 1: real run time
# -----------------------------------------------------------------------------
seconds = [times_ms[v] / 1000 for v in VERSIONS]
plt.figure(figsize=(7, 4.5))
bars = plt.bar(VERSIONS, seconds, color=COLORS)
for bar, value in zip(bars, seconds):
    speedup = seconds[0] / value
    label = f"{value:.2f} s" if speedup < 1.05 else f"{value:.2f} s\n({speedup:.1f}x faster)"
    plt.text(bar.get_x() + bar.get_width() / 2, value, label, ha="center", va="bottom")
plt.ylabel("Run time (seconds)")
plt.title("1024 x 1024 matrix multiply on Intel Core i5-1135G7")
plt.ylim(0, max(seconds) * 1.25)
plt.tight_layout()
plt.savefig("charts/1_speedup.png", dpi=150)
plt.close()

# -----------------------------------------------------------------------------
# Chart 2: simulated misses at each cache level (log scale)
# -----------------------------------------------------------------------------
levels = ["L1", "L2", "L3"]
level_names = ["L1 misses", "L2 misses", "L3 misses (trips to RAM)"]
bar_width = 0.25
plt.figure(figsize=(8, 4.5))
for index, version in enumerate(VERSIONS):
    positions = [x + index * bar_width for x in range(len(levels))]
    values = [simulated[version][level] for level in levels]
    plt.bar(positions, values, width=bar_width, label=version, color=COLORS[index])
plt.xticks([x + bar_width for x in range(len(levels))], level_names)
plt.yscale("log")
plt.ylabel("Misses (log scale)")
plt.title("Simulated cache misses, 512 x 512, i5-1135G7 cache sizes")
plt.legend()
plt.tight_layout()
plt.savefig("charts/2_cache_misses.png", dpi=150)
plt.close()

# -----------------------------------------------------------------------------
# Chart 3: my simulator vs. Cachegrind (L1 misses, setup cost removed)
# -----------------------------------------------------------------------------
mine = [simulated[v]["L1"] / 1e6 for v in VERSIONS]
theirs = [(cachegrind_l1[v] - setup_cost) / 1e6 for v in VERSIONS]
positions = range(len(VERSIONS))
plt.figure(figsize=(7, 4.5))
plt.bar([p - 0.2 for p in positions], mine, width=0.4, label="My simulator", color="#337ab7")
plt.bar([p + 0.2 for p in positions], theirs, width=0.4, label="Valgrind Cachegrind", color="#999999")
for p, a, b in zip(positions, mine, theirs):
    difference = abs(a - b) / b * 100
    plt.text(p, max(a, b), f"{difference:.2f}% apart", ha="center", va="bottom")
plt.xticks(list(positions), VERSIONS)
plt.ylabel("L1 data cache misses (millions)")
plt.title("Simulator check: L1 misses vs. Valgrind Cachegrind")
plt.ylim(0, max(mine + theirs) * 1.15)
plt.legend()
plt.tight_layout()
plt.savefig("charts/3_validation.png", dpi=150)
plt.close()

# -----------------------------------------------------------------------------
# Print a table that can be pasted into the README
# -----------------------------------------------------------------------------
print("| Version | My simulator | Cachegrind (minus setup) | Difference |")
print("|---|---|---|---|")
for v in VERSIONS:
    a = simulated[v]["L1"]
    b = cachegrind_l1[v] - setup_cost
    print(f"| {v} | {a:,.0f} | {b:,.0f} | {abs(a - b):,.0f} ({abs(a - b) / b * 100:.3f}%) |")
print("\nCharts saved in the charts/ folder.")
