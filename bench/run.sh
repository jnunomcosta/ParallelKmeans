#!/usr/bin/env bash
# Runs the standard benchmark suite. Usage: bench/run.sh [kmeans-binary] [output-dir]
# KMEANS_BENCH_QUICK=1 shrinks every suite for a fast smoke run.
set -euo pipefail

bin="${1:-build/release/apps/cli/kmeans}"
out="${2:-docs/bench}"

if [[ ! -x "$bin" ]]; then
    echo "error: kmeans binary not found at '$bin'" >&2
    echo "build it first: cmake --preset release && cmake --build --preset release -j" >&2
    echo "or pass its path as the first argument" >&2
    exit 1
fi
mkdir -p "$out"

start=$SECONDS
common=(--dataset blobs --repeat 5 --warmup 1 --iters 50)
n_d2=1e6,1e7
n_d16=1e6
n_weak=1e6
if [[ "${KMEANS_BENCH_QUICK:-0}" == 1 ]]; then
    common=(--dataset blobs --repeat 1 --warmup 1 --iters 5)
    n_d2=1e5
    n_d16=1e5
    n_weak=1e5
fi

{
    if command -v lscpu >/dev/null; then lscpu; else uname -a; fi
    echo
    echo "kmeans version: $("$bin" --version)"
    echo "date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "OMP_* environment:"
    env | grep '^OMP_' || echo "(none set)"
} >"$out/machine.txt"

"$bin" bench "${common[@]}" --scaling strong --dim 2 --k 8 --n "$n_d2" --csv "$out/strong_d2.csv"
"$bin" bench "${common[@]}" --scaling strong --dim 16 --k 32 --n "$n_d16" --csv "$out/strong_d16.csv"
"$bin" bench "${common[@]}" --scaling weak --dim 2 --k 8 --n "$n_weak" --csv "$out/weak_d2.csv"

echo "Total elapsed: $((SECONDS - start)) s"
