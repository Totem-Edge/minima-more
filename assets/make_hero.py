#!/usr/bin/env python3
"""Generate the hero performance infographic (assets/hero.svg + hero.png).

One wide banner summarising the headline C++-vs-Java results, all measured on
one host: AWS c6g.xlarge - 4 vCPU / 8 GB - Neoverse-N1 (aarch64).

Data: reference-bench/results/aws8-mainnet, aws8-core, aws8-sweep.
Run: python3 assets/make_hero.py
"""
import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, Rectangle

OUT = os.path.dirname(os.path.abspath(__file__))

BG, CARD, EDGE = "#0d1117", "#161b22", "#30363d"
TXT, SUB = "#e6edf3", "#8b949e"
CPP, JAVA, BLUE = "#3fb950", "#f0883e", "#58a6ff"

# title, C++ value, Java value, unit, badge, badge colour
CARDS = [
    ("Mainnet peak RAM", 101, 457, "MB", "4.5× less", CPP),
    ("Mainnet idle RAM", 182, 698, "MB", "3.8× less", CPP),
    ("Mainnet data dir", 64, 683, "MB", "10.7× smaller", CPP),
    ("OS threads", 25, 61, "", "59% fewer", CPP),
    ("MMR RAM (300k)", 186, 585, "MB", "3.1× less", CPP),
    ("CPU (mainnet 60 s)", 34, 72, "CPU-s", "2.1× less", CPP),
    ("Disk writes (idle)", 4.2, 5.9, "MB/h", "29% less", CPP),
    ("Synced RAM (solo)", 20, 207, "MB", "11× less", CPP),
    ("Mempool lookup", 1.0, 20417, "µs", "21,000×", BLUE),
    ("SHA3-256", 967, 283, "k h/s", "3.4× faster", BLUE),
    ("WOTS signing", 1.07, 5.24, "ms", "4.9× faster", BLUE),
    ("MMR add+root", 214, 122, "k/s", "1.8× faster", BLUE),
]


def fmt(v):
    if v >= 1000:
        return f"{v:,.0f}"
    if v < 10:
        return f"{v:.1f}".rstrip("0").rstrip(".")
    return f"{v:.0f}"


def card(ax, title, cv, jv, unit, badge, bcol):
    ax.set_xlim(0, 10); ax.set_ylim(0, 10); ax.axis("off")
    ax.add_patch(FancyBboxPatch((0.15, 0.15), 9.7, 9.7,
                 boxstyle="round,pad=0.15,rounding_size=0.6",
                 fc=CARD, ec=EDGE, lw=1.2, zorder=1))
    ax.text(0.7, 9.3, title, color=TXT, fontsize=12, fontweight="bold",
            va="top", zorder=3)
    ax.add_patch(FancyBboxPatch((6.45, 8.35), 3.1, 0.98,
                 boxstyle="round,pad=0.1,rounding_size=0.4",
                 fc=bcol, ec="none", zorder=3))
    ax.text(8.0, 8.84, badge, ha="center", va="center", color="#0d1117",
            fontsize=10.5, fontweight="bold", zorder=4)

    maxv = max(cv, jv) or 1.0
    base, hmax = 1.35, 5.4
    bc = base + max(0.28, hmax * (cv / maxv))   # min height so small bars show
    bj = base + max(0.28, hmax * (jv / maxv))
    ax.add_patch(Rectangle((2.35, base), 1.8, bc - base, fc=CPP, zorder=2))
    ax.add_patch(Rectangle((5.65, base), 1.8, bj - base, fc=JAVA, zorder=2))
    cl = f"{fmt(cv)} {unit}".strip()
    jl = f"{fmt(jv)} {unit}".strip()
    ax.text(3.25, bc + 0.25, cl, ha="center", color=CPP, fontsize=11.5,
            fontweight="bold", zorder=3)
    ax.text(6.55, bj + 0.25, jl, ha="center", color=JAVA, fontsize=11.5,
            fontweight="bold", zorder=3)
    ax.text(3.25, base - 0.7, "C++", ha="center", color=TXT, fontsize=11, zorder=3)
    ax.text(6.55, base - 0.7, "Java", ha="center", color=SUB, fontsize=11, zorder=3)


def main():
    fig = plt.figure(figsize=(16, 9), dpi=150, facecolor=BG)

    fig.text(0.5, 0.955, "MINIMA MORE", ha="center", color=TXT,
             fontsize=42, fontweight="bold")
    fig.text(0.5, 0.902, "Same blockchain. A fraction of the resources.",
             ha="center", color=CPP, fontsize=19, style="italic")
    fig.text(0.5, 0.864,
             "C++ node vs Java node (minima-core)  ·  AWS c6g.xlarge: 4 vCPU / 8 GB"
             " Neoverse-N1  ·  same chain, same host  ·  green = C++  orange = Java",
             ha="center", color=SUB, fontsize=12)

    gs = fig.add_gridspec(3, 4, left=0.035, right=0.965, top=0.820, bottom=0.105,
                          wspace=0.18, hspace=0.34)
    for i, (title, cv, jv, unit, badge, bcol) in enumerate(CARDS):
        ax = fig.add_subplot(gs[i // 4, i % 4])
        card(ax, title, cv, jv, unit, badge, bcol)

    fig.text(0.5, 0.048,
             "P2P peer discovery fixed on live mainnet:  1  →  up to ~10 peers"
             "   (missing NIO length-framing prefix)",
             ha="center", color=CPP, fontsize=13, fontweight="bold")
    fig.text(0.5, 0.021,
             "Same protocol · same SHA3-256 / WOTS cryptography · same consensus rules"
             "        ·        Full methodology & raw data: reference-bench/",
             ha="center", color=SUB, fontsize=10)

    fig.savefig(os.path.join(OUT, "hero.png"), facecolor=BG)
    fig.savefig(os.path.join(OUT, "hero.svg"), format="svg", facecolor=BG)
    plt.close(fig)
    print("wrote hero.png + hero.svg")


if __name__ == "__main__":
    main()
