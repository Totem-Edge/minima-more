#!/usr/bin/env python3
"""Generate the SVG charts embedded in the root README.

Run: python3 assets/make_charts.py

Data: measured C++ (this node) vs Java (minima-core) on ONE host:
AWS c6g.xlarge - 4 vCPU / 8 GB - Neoverse-N1 (aarch64).
Raw output: reference-bench/results/aws8-mainnet, aws8-core, aws8-sweep,
aws8-engines.
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


# 1. Full-node RAM (C++ vs Java), fresh private chain, MB
fig, ax = plt.subplots(figsize=(7, 3.8))
labels = ["Idle RAM", "Peak startup RAM"]
cpp = [18.6, 19.1]
java = [223.9, 223.9]
x = np.arange(len(labels))
w = 0.36
b1 = ax.bar(x - w / 2, cpp, w, label="C++ node", color=CPP)
b2 = ax.bar(x + w / 2, java, w, label="Java node", color=JAVA)
ax.set_xticks(x)
ax.set_xticklabels(labels)
ax.set_ylabel("Process RAM (MB)")
ax.set_title("Full-node RAM footprint", fontweight="bold")
ax.set_ylim(0, 270)
ax.legend(frameon=False)
for bb in (b1, b2):
    for r in bb:
        ax.annotate(f"{r.get_height():.0f}", (r.get_x() + r.get_width() / 2, r.get_height()),
                    textcoords="offset points", xytext=(0, 4), ha="center",
                    fontsize=10, fontweight="bold")
_save(fig, "node_ram.svg")

# 2. MMR peak RAM (300k entries on the 8 GB host)
bars("mmr_ram.svg", ["C++ node", "Java node"], [186, 585], [CPP, JAVA],
     "MMR - peak RSS for 300,000 entries", "Peak RSS (MB)", fmt="{:.0f}")

# 3. MMR bytes/entry progression (optimisation history)
bars("mmr_bytes_per_entry.svg",
     ["Original", "+By-value", "+SBO", "+Compact key"],
     [858, 762, 698, 635], [BLUE, BLUE, BLUE, CPP],
     "MMR memory per entry (lower is better)", "Bytes / entry", fmt="{:.0f}",
     hline=622, hlabel="Java ~622 (heap)")

# 4. MMR addEntry+getRoot throughput (same host)
bars("mmr_build_time.svg", ["C++ node", "Java node"], [214360, 121644], [CPP, JAVA],
     "MMR addEntry + getRoot - entries per second", "Entries / second", fmt="{:,.0f}")

# 5. Mempool coin lookup (log scale) with speedup annotation
fig, ax = plt.subplots(figsize=(7, 3.8))
b = ax.bar([0, 1], [0.97, 20417], color=[CPP, JAVA], width=0.6)
ax.set_yscale("log")
ax.set_xticks([0, 1])
ax.set_xticklabels(["C++  O(1) index", "Java  linear scan"])
ax.set_ylabel("us per lookup (log)")
ax.set_title("Mempool double-spend check - ~21,000x faster", fontweight="bold")
ax.set_ylim(0.5, 80000)
ax.annotate("0.97 us", (0, 0.97), textcoords="offset points", xytext=(0, 6),
            ha="center", fontweight="bold")
ax.annotate("20,417 us", (1, 20417), textcoords="offset points", xytext=(0, 6),
            ha="center", fontweight="bold")
_save(fig, "coin_lookup.svg")

# 6. SQLite batched writes
bars("sqlite_write.svg", ["Per-row (old)", "Batched (new)"], [2183, 7681],
     [BLUE, CPP], "SQLite write throughput - 3.5x with batching",
     "Inserts / second", fmt="{:,.0f}")

# 7. Microbenchmarks (3 panels)
fig, axes = plt.subplots(1, 3, figsize=(11, 3.6))
panels = [
    ("SHA3-256 224B", ["C++", "Java"], [967126, 283026], "hashes/s", "{:,.0f}"),
    ("WOTS sign (batched)", ["C++", "Java"], [1.07, 5.24], "ms (lower better)", "{:.2f}"),
    ("MMR addEntry+getRoot", ["C++", "Java"], [214360, 121644], "entries/s", "{:,.0f}"),
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
fig.suptitle("Microbenchmarks - C++ node vs Java node", fontweight="bold", y=1.02)
_save(fig, "microbench.svg")

# --- full-lifecycle benchmarks (same 8 GB host) -----------------------------

# 8. Fully-synced node RAM (peak + idle), MB
fig, ax = plt.subplots(figsize=(7, 3.8))
labels = ["Peak during sync", "Idle after sync"]
cpp = [19.6, 21.2]
java = [207.2, 151.3]
x = np.arange(len(labels)); w = 0.36
b1 = ax.bar(x - w / 2, cpp, w, label="C++ node", color=CPP)
b2 = ax.bar(x + w / 2, java, w, label="Java node", color=JAVA)
ax.set_xticks(x); ax.set_xticklabels(labels)
ax.set_ylabel("RSS (MB)")
ax.set_title("Fully-synced node RAM", fontweight="bold")
ax.set_ylim(0, 250); ax.legend(frameon=False)
for bb in (b1, b2):
    for r in bb:
        ax.annotate(f"{r.get_height():.0f}", (r.get_x() + r.get_width() / 2, r.get_height()),
                    textcoords="offset points", xytext=(0, 4), ha="center",
                    fontsize=10, fontweight="bold")
_save(fig, "lifecycle_synced_ram.svg")

# 9. CPU at idle (CPU-seconds per hour)
bars("lifecycle_cpu_idle.svg", ["C++ node", "Java node"], [66.84, 102.51], [CPP, JAVA],
     "CPU at idle - CPU-seconds per hour (lower is better)", "CPU-seconds / hour", fmt="{:.1f}")

# 10. CPU under transaction load (CPU ms/tx + send latency)
fig, ax = plt.subplots(figsize=(7, 3.8))
labels = ["CPU ms per tx", "Send latency (ms)"]
cpp = [2070, 569]
java = [2855, 2298]
x = np.arange(len(labels)); w = 0.36
b1 = ax.bar(x - w / 2, cpp, w, label="C++ node", color=CPP)
b2 = ax.bar(x + w / 2, java, w, label="Java node", color=JAVA)
ax.set_xticks(x); ax.set_xticklabels(labels)
ax.set_ylabel("Milliseconds (lower is better)")
ax.set_title("CPU under transaction load", fontweight="bold")
ax.set_ylim(0, 3300); ax.legend(frameon=False)
for bb in (b1, b2):
    for r in bb:
        ax.annotate(f"{r.get_height():.0f}", (r.get_x() + r.get_width() / 2, r.get_height()),
                    textcoords="offset points", xytext=(0, 4), ha="center",
                    fontsize=9, fontweight="bold")
_save(fig, "lifecycle_tx_load.svg")

# 11. Disk writes at idle (MB/hour)
bars("lifecycle_disk.svg", ["C++ node", "Java node"], [4.18, 5.90], [CPP, JAVA],
     "Disk writes at idle - MB per hour (lower is better)", "MB / hour", fmt="{:.2f}")

# 12. Live mainnet (fresh both-node run, AWS 8 GB): memory/disk/CPU panels
fig, axes = plt.subplots(1, 4, figsize=(14, 3.6))
panels = [
    ("Peak RSS", [101, 457], "MB", "{:.0f}"),
    ("Idle RSS", [182, 698], "MB", "{:.0f}"),
    ("Data dir", [64, 683], "MB", "{:.0f}"),
    ("CPU / 60 s", [33.7, 72.1], "CPU-s", "{:.0f}"),
]
for ax, (title, vals, ylab, fmt) in zip(axes, panels):
    b = ax.bar([0, 1], vals, color=[CPP, JAVA], width=0.6)
    ax.set_xticks([0, 1])
    ax.set_xticklabels(["C++", "Java"])
    ax.set_ylabel(ylab)
    ax.set_title(title, fontweight="bold", fontsize=11)
    ax.set_ylim(0, max(vals) * 1.2)
    for r, v in zip(b, vals):
        ax.annotate(fmt.format(v), (r.get_x() + r.get_width() / 2, v),
                    textcoords="offset points", xytext=(0, 4), ha="center",
                    fontsize=9, fontweight="bold")
fig.suptitle("Live mainnet — C++ node vs Java node (same host, same chain)",
             fontweight="bold", y=1.02)
_save(fig, "mainnet.svg")

print("done")
