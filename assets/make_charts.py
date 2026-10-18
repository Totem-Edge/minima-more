#!/usr/bin/env python3
"""Generate the SVG charts embedded in the root README.

Run: python3 assets/make_charts.py
Data: measured C++ (this node) vs Java (minima-core) benchmarks, see
reference-bench/RESULTS_LOG.md (M1 and OCI A1 sweeps).
"""
import os
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

OUT = os.path.dirname(os.path.abspath(__file__))
CPP = "#2f9e44"     # this node
JAVA = "#e8590c"    # Java reference
GREY = "#868e96"
BLUE = "#1c7ed6"

plt.rcParams.update({
    "font.size": 11,
    "axes.spines.top": False,
    "axes.spines.right": False,
    "axes.edgecolor": "#adb5bd",
    "text.color": "#212529",
    "axes.labelcolor": "#212529",
    "xtick.color": "#212529",
    "ytick.color": "#212529",
    "figure.facecolor": "#ffffff",
    "axes.facecolor": "#ffffff",
})


def _save(fig, name):
    fig.tight_layout()
    fig.savefig(os.path.join(OUT, name), format="svg")
    plt.close(fig)
    print("wrote", name)


def bars(name, labels, values, colors, title, ylabel, fmt="{:.0f}",
         ylog=False, hline=None, hlabel=None, figname=None):
    fig, ax = plt.subplots(figsize=(7, 3.8))
    x = np.arange(len(labels))
    b = ax.bar(x, values, color=colors, width=0.6)
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.set_ylabel(ylabel)
    ax.set_title(title, fontweight="bold")
    if ylog:
        ax.set_yscale("log")
    ymax = max(values)
    ax.set_ylim(0, ymax * 1.18)
    for rect, v in zip(b, values):
        ax.annotate(fmt.format(v), (rect.get_x() + rect.get_width() / 2, v),
                    textcoords="offset points", xytext=(0, 4),
                    ha="center", fontsize=10, fontweight="bold")
    if hline is not None:
        ax.axhline(hline, ls="--", color=GREY, lw=1.2)
        ax.text(len(labels) - 0.5, hline, "  " + (hlabel or ""), color=GREY,
                va="bottom", ha="right", fontsize=9)
    _save(fig, name)


# 1. Full-node RAM (C++ vs Java), MB
fig, ax = plt.subplots(figsize=(7, 3.8))
labels = ["Idle RAM", "Peak startup RAM"]
cpp = [19, 20]
java = [302, 302]
x = np.arange(len(labels))
w = 0.36
b1 = ax.bar(x - w / 2, cpp, w, label="C++ node", color=CPP)
b2 = ax.bar(x + w / 2, java, w, label="Java node", color=JAVA)
ax.set_xticks(x)
ax.set_xticklabels(labels)
ax.set_ylabel("Process RAM (MB)")
ax.set_title("Full-node RAM footprint", fontweight="bold")
ax.set_ylim(0, 360)
ax.legend(frameon=False)
for bb in (b1, b2):
    for r in bb:
        ax.annotate(f"{r.get_height():.0f}", (r.get_x() + r.get_width() / 2, r.get_height()),
                    textcoords="offset points", xytext=(0, 4), ha="center",
                    fontsize=10, fontweight="bold")
_save(fig, "node_ram.svg")

# 2. MMR 1M peak RAM
bars("mmr_ram.svg", ["C++ node", "Java node"], [624, 1898], [CPP, JAVA],
     "MMR — peak RAM for 1,000,000 entries", "Peak RSS (MB)", fmt="{:.0f}")

# 3. MMR bytes/entry progression
bars("mmr_bytes_per_entry.svg",
     ["Original", "+By-value", "+SBO", "+Compact key"],
     [858, 762, 698, 634], [BLUE, BLUE, BLUE, CPP],
     "MMR memory per entry (lower is better)", "Bytes / entry", fmt="{:.0f}",
     hline=602, hlabel="Java ~602 (heap)")

# 4. MMR build time progression
bars("mmr_build_time.svg",
     ["Original", "+Compact key", "+In-place hash", "+Fast paths"],
     [3785, 3282, 2959, 2216], [BLUE, BLUE, BLUE, CPP],
     "MMR build time — 1,000,000 entries (M1)", "Milliseconds", fmt="{:.0f}",
     hline=2256, hlabel="Java 2256")

# 5. Mempool coin lookup (log scale) with speedup annotation
fig, ax = plt.subplots(figsize=(7, 3.8))
b = ax.bar([0, 1], [0.9, 43733], color=[CPP, JAVA], width=0.6)
ax.set_yscale("log")
ax.set_xticks([0, 1])
ax.set_xticklabels(["C++  O(1) index", "Java  linear scan"])
ax.set_ylabel("µs per lookup (log)")
ax.set_title("Mempool double-spend check — ~49,000x faster", fontweight="bold")
ax.set_ylim(0.5, 120000)
ax.annotate("0.9 µs", (0, 0.9), textcoords="offset points", xytext=(0, 6),
            ha="center", fontweight="bold")
ax.annotate("43,733 µs", (1, 43733), textcoords="offset points", xytext=(0, 6),
            ha="center", fontweight="bold")
_save(fig, "coin_lookup.svg")

# 6. SQLite batched writes
bars("sqlite_write.svg", ["Per-row (old)", "Batched (new)"], [1519, 3222],
     [BLUE, CPP], "SQLite write throughput — 2.1x with batching",
     "Inserts / second", fmt="{:.0f}")

# 7. Microbenchmarks (3 panels)
fig, axes = plt.subplots(1, 3, figsize=(11, 3.6))
panels = [
    ("SHA3-256 224B", ["C++", "Java"], [1158839, 346289], "hashes/s", "{:,.0f}"),
    ("WOTS sign (batched)", ["C++", "Java"], [0.92, 4.30], "ms (lower better)", "{:.2f}"),
    ("MMR addEntry+getRoot", ["C++", "Java"], [259839, 139109], "entries/s", "{:,.0f}"),
]
for ax, (title, lbls, vals, ylab, fmt) in zip(axes, panels):
    b = ax.bar([0, 1], vals, color=[CPP, JAVA], width=0.6)
    ax.set_xticks([0, 1])
    ax.set_xticklabels(lbls)
    ax.set_ylabel(ylab)
    ax.set_title(title, fontweight="bold", fontsize=11)
    ax.set_ylim(0, max(vals) * 1.2)
    for r, v in zip(b, vals):
        ax.annotate(fmt.format(v), (r.get_x() + r.get_width() / 2, v),
                    textcoords="offset points", xytext=(0, 4), ha="center",
                    fontsize=9, fontweight="bold")
fig.suptitle("Microbenchmarks — C++ node vs Java node", fontweight="bold", y=1.02)
_save(fig, "microbench.svg")

print("done")
