"""Plot benchmark CSVs. Usage: .venv/bin/python bench/plot.py [docs/bench]"""
import csv
import glob
import os
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

PALETTE = ["#0072B2", "#D55E00", "#009E73", "#CC79A7", "#E69F00", "#56B4E9"]
plt.rcParams.update({"font.size": 12, "figure.facecolor": "white", "axes.facecolor": "white"})

folder = sys.argv[1] if len(sys.argv) > 1 else "docs/bench"


def num(s):
    return float(s) if s != "" else None


# series: label -> {"seq": per_iter_ms or None, "rows": [row dicts of omp runs]}
strong, weak = {}, {}
for path in sorted(glob.glob(os.path.join(folder, "*.csv"))):
    suite = os.path.basename(path)[:-4]
    with open(path, newline="") as f:
        for r in csv.DictReader(f):
            if r["scaling"] == "weak":
                label = suite
            else:
                label = f"{suite}, n={int(r['n']):,}"
            group = weak if r["scaling"] == "weak" else strong
            s = group.setdefault(label, {"seq": None, "rows": []})
            if r["impl"] == "seq":
                s["seq"] = float(r["per_iter_ms"]) if s["seq"] is None else s["seq"]
            else:
                s["rows"].append({k: num(v) if k not in ("scaling", "impl", "dataset") else v
                                  for k, v in r.items()})
every = {**strong, **weak}
color = {label: PALETTE[i % len(PALETTE)] for i, label in enumerate(every)}

cpu = ""
try:
    with open(os.path.join(folder, "machine.txt")) as f:
        for line in f:
            if line.startswith("Model name:"):
                cpu = line.split(":", 1)[1].strip()
                break
except OSError:
    pass


def chart(name, title, ylabel, series, ycol, extra=None, ylim=None, log_y=False):
    fig, ax = plt.subplots(figsize=(8, 5), dpi=200)
    for label, s in series.items():
        pts = [(r["threads"], r[ycol]) for r in s["rows"] if r[ycol] is not None]
        if pts:
            ax.plot(*zip(*pts), "o-", color=color[label], label=label)
        if extra:
            extra(ax, label, s)
    ax.set_xscale("log", base=2)
    ticks = sorted({r["threads"] for s in series.values() for r in s["rows"]})
    ax.set_xticks(ticks)
    ax.set_xticklabels([str(int(t)) for t in ticks])
    ax.minorticks_off()
    if log_y:
        ax.set_yscale("log")
    if ylim:
        ax.set_ylim(*ylim)
    ax.set_xlabel("threads")
    ax.set_ylabel(ylabel)
    ax.grid(alpha=0.3)
    ax.legend(loc="center left", bbox_to_anchor=(1.01, 0.5), frameon=False)
    fig.suptitle(title, fontsize=14, y=0.98)
    ax.set_title(cpu, fontsize=10, color="#444444")
    fig.tight_layout()
    fig.savefig(os.path.join(folder, name), metadata={"Software": None})
    plt.close(fig)


def ideal(ax, label, s):
    if label == next(iter(strong)):
        xs = sorted({r["threads"] for r in s["rows"]})
        ax.plot(xs, xs, "--", color="gray", label="ideal")


def one(ax, label, s):
    if label == next(iter(strong)):
        ax.axhline(1.0, linestyle="--", color="gray", label="ideal")


def seq_line(ax, label, s):
    if s["seq"] is not None:
        ax.axhline(s["seq"], linestyle=":", color=color[label])


chart("speedup.png", "Speedup of omp over seq (strong scaling)", "speedup", strong, "speedup", ideal)
chart("efficiency.png", "Parallel efficiency (strong scaling)", "efficiency", strong, "efficiency",
      one, ylim=(0, 1.1))
chart("time_per_iter.png", "Time per iteration (dotted: seq)", "ms per iteration", strong,
      "per_iter_ms", seq_line, log_y=True)
if weak:
    chart("weak_efficiency.png", "Weak-scaling efficiency (constant points per thread)",
          "weak efficiency", weak, "efficiency", one, ylim=(0, 1.1))
chart("karp_flatt.png", "Karp-Flatt serial fraction (strong scaling)", "serial fraction", strong,
      "karp_flatt")
