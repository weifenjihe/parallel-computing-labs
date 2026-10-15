#!/usr/bin/env python3
"""Run distributed CSR SpMM using this experiment's local build."""
import argparse
import csv
from datetime import datetime, timezone
import importlib.util
import io
import json
import os
from pathlib import Path
import platform
import random
import shutil
import statistics
import subprocess
import sys

from cli_utils import positive_int, nonnegative_int, seed_int, percentage, build_directory, check_threads, run_main

HERE = Path(__file__).resolve().parent
VARIANTS = [("Broadcast", "broadcast"), ("Allgather", "allgather"),
            ("Halo", "halo"), ("Overlap", "overlap")]
PROFILES = ["uniform", "skewed", "blocked", "banded"]


def save(rows, destination):
    with destination.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader(); writer.writerows(rows)


def environment(launcher, build_dir):
    info = {"recorded_at_utc": datetime.now(timezone.utc).isoformat(),
            "system": platform.system(), "architecture": platform.machine(),
            "logical_cpus": os.cpu_count(), "python": platform.python_version(),
            "mpi_launcher": launcher,
            "mpi_version": subprocess.check_output([launcher, "--version"], text=True).splitlines()[0],
            "mpi_environment": {k: v for k, v in os.environ.items()
                                if k.startswith(("OMPI_", "MPICH_", "I_MPI_"))}}
    if platform.system() == "Darwin":
        for key in ["machdep.cpu.brand_string", "hw.perflevel0.physicalcpu", "hw.perflevel1.physicalcpu"]:
            info[key] = subprocess.run(["sysctl", "-n", key], text=True, capture_output=True).stdout.strip()
    cache = build_dir / "CMakeCache.txt"
    info["build_directory"] = str(build_dir)
    info["compiler"] = next(line.split("=", 1)[1] for line in cache.read_text().splitlines()
                            if line.startswith("CMAKE_CXX_COMPILER:"))
    info["build_type"] = next(line.split("=", 1)[1] for line in cache.read_text().splitlines()
                              if line.startswith("CMAKE_BUILD_TYPE:"))
    info["compiler_version"] = subprocess.check_output([info["compiler"], "--version"], text=True).splitlines()[0]
    return info


def run_benchmark(output, ranks=4, sizes=(4096, 8192, 16384), k=8192, n=1024,
                  nnz_per_row=32, tile=128, block_nnz=256, partition="nnz",
                  profiles=PROFILES, repeat=7, rounds=3, seed=2026, hostfile=None, build_dir=None, verify=False):
    build_dir = build_directory(HERE, build_dir, "lab4_spmm")
    output = Path(output); output.mkdir(parents=True, exist_ok=True)
    counts = sorted({1, ranks} | ({2} if ranks >= 2 else set()))
    sizes = sorted(set(sizes)); profiles = list(dict.fromkeys(profiles))
    launcher = shutil.which("mpiexec") or shutil.which("mpirun")
    if launcher is None:
        raise RuntimeError("找不到 mpiexec/mpirun，请先安装 MPI 并检查 PATH。")
    launch_options = ["--hostfile", str(Path(hostfile).resolve())] if hostfile else []
    metadata = environment(launcher, build_dir)
    if "Open MPI" in metadata["mpi_version"] or "OpenRTE" in metadata["mpi_version"]:
        launch_options.append("--oversubscribe")
    metadata.update({"m_sizes": sizes, "k": k, "n": n, "nnz_per_row": nnz_per_row,
                     "tile": tile, "block_nnz": block_nnz, "partition": partition,
                     "profiles": profiles, "requested_ranks": counts, "repeats": repeat,
                     "rounds": rounds, "warmups_per_process": 1, "seed": seed,
                     "hostfile": str(Path(hostfile).resolve()) if hostfile else None,
                     "launch_options": launch_options,
                     "timing": "Maximum rank time including packing, B exchange and local multiplication",
                     "verify": verify, "golden_ratio_commit": "fb012f772374bf826f81582a31e95455c91fcb26"})
    (output / "environment.json").write_text(json.dumps(metadata, indent=2, ensure_ascii=False) + "\n")
    raw = []
    cases = [(label, kernel, workers) for workers in counts for label, kernel in VARIANTS]
    randomizer = random.Random(seed)

    completed = 0
    total = rounds * len(sizes) * len(profiles) * (len(cases) + 2)

    def measure(m, profile, outer, label, kernel, workers):
        nonlocal completed
        options = {"m": m, "k": k, "n": n, "nnz-per-row": nnz_per_row, "tile": tile,
                   "block-nnz": block_nnz, "partition": partition, "profile": profile,
                   "kernel": kernel, "repeat": repeat, "seed": seed}
        command = [launcher, *launch_options, "-n", str(workers), str(build_dir / "lab4_spmm")]
        if verify:
            command.append("--verify")
        for name, value in options.items(): command.extend(["--" + name, str(value)])
        print(f"Lab4 {completed + 1}/{total} | round {outer + 1}/{rounds}: M={m}, {profile}, {label}, ranks={workers}",
              file=sys.stderr, flush=True)
        result = subprocess.check_output(command, text=True)
        for row in csv.DictReader(io.StringIO(result)):
            row.update(round=outer, variant=label, requested_ranks=workers)
            raw.append(row)
        completed += 1
        save(raw, output / "raw.csv")

    for outer in range(rounds):
        workloads = [(m, profile) for m in sizes for profile in profiles]
        randomizer.shuffle(workloads)
        for m, profile in workloads:
            measure(m, profile, outer, "Serial", "serial", 1)
            order = cases.copy(); randomizer.shuffle(order)
            for label, kernel, workers in order: measure(m, profile, outer, label, kernel, workers)
            measure(m, profile, outer, "Serial", "serial", 1)
            save(raw, output / "raw.csv")

    summaries = [row for row in raw if row["record"] == "summary"]
    medians = {}
    labels = [("Serial", 1)] + [(label, workers) for workers in counts for label, _ in VARIANTS]
    for m in sizes:
        for profile in profiles:
            for label, workers in labels:
                selected = [row for row in summaries if int(row["m"]) == m and row["profile"] == profile
                            and row["variant"] == label and int(row["ranks"]) == workers]
                medians[m, profile, label, workers] = statistics.median(float(row["elapsed_s"]) for row in selected)
    results = []
    for m in sizes:
        for profile in profiles:
            for label, workers in labels:
                selected = [row for row in summaries if int(row["m"]) == m and row["profile"] == profile
                            and row["variant"] == label and int(row["ranks"]) == workers]
                trials = [float(row["elapsed_s"]) for row in raw if row["record"] == "trial"
                          and int(row["m"]) == m and row["profile"] == profile
                          and row["variant"] == label and int(row["ranks"]) == workers]
                elapsed = medians[m, profile, label, workers]
                speed = medians[m, profile, "Serial", 1] / elapsed
                process_speed = medians[m, profile, label, 1] / elapsed
                row = min(selected, key=lambda r: abs(float(r["elapsed_s"]) - elapsed)).copy()
                row.update(elapsed_s=elapsed, median_ms=elapsed * 1000,
                           gflops=2.0 * float(row["nnz"]) * n / elapsed / 1e9,
                           speedup=speed, efficiency=speed / workers,
                           process_speedup=process_speed, process_efficiency=process_speed / workers,
                           received_mib=float(row["receive_bytes"]) / (1024 * 1024),
                           setup_s=statistics.median(float(r["setup_s"]) for r in selected),
                           round_min_ms=min(float(r["elapsed_s"]) for r in selected) * 1000,
                           round_max_ms=max(float(r["elapsed_s"]) for r in selected) * 1000,
                           samples=len(trials), trial_min_ms=min(trials) * 1000, trial_max_ms=max(trials) * 1000)
                results.append(row)
    save(results, output / "summary.csv")
    spec = importlib.util.spec_from_file_location("lab4_plot", HERE / "plot.py")
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    module.plot_results(output / "summary.csv", output, ranks)
    print(f"Lab4 measurements and figures: {output}", file=sys.stderr)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--m", type=positive_int, nargs="+", default=[4096, 8192, 16384])
    parser.add_argument("--k", type=positive_int, default=8192)
    parser.add_argument("--n", type=positive_int, default=1024)
    parser.add_argument("--nnz-per-row", type=nonnegative_int, default=32)
    parser.add_argument("--ranks", type=positive_int, default=4)
    parser.add_argument("--tile", type=positive_int, default=128)
    parser.add_argument("--block-nnz", type=positive_int, default=256)
    parser.add_argument("--partition", choices=["rows", "nnz"], default="nnz")
    parser.add_argument("--profile", choices=PROFILES, nargs="+", default=PROFILES)
    parser.add_argument("--repeat", type=positive_int, default=7)
    parser.add_argument("--rounds", type=positive_int, default=3)
    parser.add_argument("--seed", type=seed_int, default=2026)
    parser.add_argument("--hostfile", type=Path)
    parser.add_argument("--output", type=Path, default=HERE / "results")
    parser.add_argument("--build-dir", type=Path, default=HERE / "build", help="构建目录，例如 build-llvm")
    parser.add_argument("--verify", action="store_true", help="计时之外逐元素/逐桶核对结果")
    args = parser.parse_args()
    if args.nnz_per_row > args.k:
        parser.error("--nnz-per-row 不能超过 --k；缩小矩阵时请同步缩小每行非零数。")
    run_benchmark(args.output, ranks=args.ranks, sizes=args.m, k=args.k, n=args.n,
                  nnz_per_row=args.nnz_per_row, tile=args.tile, block_nnz=args.block_nnz,
                  partition=args.partition, profiles=args.profile, repeat=args.repeat,
                  rounds=args.rounds, seed=args.seed, hostfile=args.hostfile, build_dir=args.build_dir, verify=args.verify)


if __name__ == "__main__": main()
