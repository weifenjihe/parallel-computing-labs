#!/usr/bin/env python3
"""Run histogram measurements using this experiment's local build."""
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
import statistics
import subprocess
import sys

from cli_utils import positive_int, nonnegative_int, seed_int, percentage, build_directory, check_threads, run_main

HERE = Path(__file__).resolve().parent
DEFAULT_SIZES = [1000000, 5000000, 10000000]
DEFAULT_STRIPES = [1, 4, 16, 64]
DEFAULT_DISTRIBUTIONS = ["uniform", "hotspot"]


def save(rows, destination):
    with destination.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader(); writer.writerows(rows)


def environment(build_dir):
    info = {"recorded_at_utc": datetime.now(timezone.utc).isoformat(),
            "system": platform.system(), "architecture": platform.machine(),
            "logical_cpus": os.cpu_count(), "python": platform.python_version(),
            "openmp_environment": {k: v for k, v in os.environ.items() if k.startswith(("OMP_", "KMP_"))}}
    if platform.system() == "Darwin":
        for key in ["machdep.cpu.brand_string", "hw.perflevel0.physicalcpu", "hw.perflevel1.physicalcpu"]:
            info[key] = subprocess.run(["sysctl", "-n", key], text=True, capture_output=True).stdout.strip()
    cache = build_dir / "CMakeCache.txt"
    info["build_directory"] = str(build_dir)
    info["compiler"] = next(line.split("=", 1)[1] for line in cache.read_text().splitlines()
                            if line.startswith("CMAKE_CXX_COMPILER:"))
    info["compiler_version"] = subprocess.check_output([info["compiler"], "--version"], text=True).splitlines()[0]
    return info


def run_benchmark(output, threads=4, sizes=None, stripes=None, distributions=None,
                  bins=256, hot_percent=90, repeat=7, rounds=3, seed=2026, build_dir=None, verify=False):
    sizes = sorted(set(DEFAULT_SIZES if sizes is None else sizes))
    stripes = sorted(set(DEFAULT_STRIPES if stripes is None else stripes))
    distributions = list(dict.fromkeys(DEFAULT_DISTRIBUTIONS if distributions is None else distributions))
    counts = sorted({1, threads} | ({2} if threads >= 2 else set()))
    variants = [("Critical", "critical", stripes[0]), ("Atomic", "atomic", stripes[0])]
    variants += [(f"Lock{count}", "locks", count) for count in stripes]
    variants += [("Private", "private", stripes[0])]
    cases = [(label, kernel, lock_count, workers) for workers in counts
             for label, kernel, lock_count in variants]
    build_dir = build_directory(HERE, build_dir, "lab3_histogram")
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    metadata = environment(build_dir)
    metadata.update({"input_sizes": sizes, "lock_counts": stripes, "distributions": distributions,
                     "bins": bins, "hot_percent": hot_percent, "repeats": repeat, "rounds": rounds,
                     "warmups_per_process": 1, "requested_threads": counts, "seed": seed,
                     "verify": verify, "golden_ratio_commit": "fb012f772374bf826f81582a31e95455c91fcb26"})
    (output / "environment.json").write_text(json.dumps(metadata, indent=2, ensure_ascii=False) + "\n")
    raw = []
    randomizer = random.Random(seed)

    completed = 0
    total = rounds * len(sizes) * len(distributions) * (len(cases) + 2)

    def measure(n, distribution, outer, label, kernel, lock_count, workers):
        nonlocal completed
        options = {"n": n, "bins": bins, "stripes": lock_count, "seed": seed,
                   "distribution": distribution, "hot-percent": hot_percent,
                   "kernel": kernel, "threads": workers, "repeat": repeat}
        command = [str(build_dir / "lab3_histogram")]
        if verify:
            command.append("--verify")
        for name, value in options.items():
            command.extend(["--" + name, str(value)])
        print(f"Lab3 {completed + 1}/{total} | round {outer + 1}/{rounds}: N={n}, {distribution}, {label}, threads={workers}",
              file=sys.stderr, flush=True)
        output_text = subprocess.check_output(command, text=True)
        records = list(csv.DictReader(io.StringIO(output_text)))
        check_threads(records, workers)
        for row in records:
            row.update(round=outer, variant=label, requested_threads=workers)
            raw.append(row)
        completed += 1
        save(raw, output / "raw.csv")

    for outer in range(rounds):
        workloads = [(n, distribution) for n in sizes for distribution in distributions]
        randomizer.shuffle(workloads)
        for n, distribution in workloads:
            measure(n, distribution, outer, "Serial", "serial", stripes[0], 1)
            order = cases.copy(); randomizer.shuffle(order)
            for label, kernel, lock_count, workers in order:
                measure(n, distribution, outer, label, kernel, lock_count, workers)
            measure(n, distribution, outer, "Serial", "serial", stripes[0], 1)
            save(raw, output / "raw.csv")

    summaries = [row for row in raw if row["record"] == "summary"]
    configurations = [("Serial", 1)] + [(label, workers) for workers in counts for label, _, _ in variants]
    medians = {}
    for distribution in distributions:
        for n in sizes:
            for label, workers in configurations:
                selected = [row for row in summaries if row["distribution"] == distribution and int(row["n"]) == n
                            and row["variant"] == label and int(row["threads"]) == workers]
                medians[distribution, n, label, workers] = statistics.median(float(row["elapsed_s"]) for row in selected)
    results = []
    for distribution in distributions:
        for n in sizes:
            for label, workers in configurations:
                selected = [row for row in summaries if row["distribution"] == distribution and int(row["n"]) == n
                            and row["variant"] == label and int(row["threads"]) == workers]
                trials = [float(row["elapsed_s"]) for row in raw if row["record"] == "trial"
                          and row["distribution"] == distribution and int(row["n"]) == n
                          and row["variant"] == label and int(row["threads"]) == workers]
                elapsed = medians[distribution, n, label, workers]
                speed = medians[distribution, n, "Serial", 1] / elapsed
                thread_speed = medians[distribution, n, label, 1] / elapsed
                row = min(selected, key=lambda item: abs(float(item["elapsed_s"]) - elapsed)).copy()
                lock_speed = (medians[distribution, n, f"Lock{stripes[0]}", workers] / elapsed
                              if row["kernel"] == "locks" else "")
                row.update(elapsed_s=elapsed, median_ms=elapsed * 1000,
                           million_items_s=n / elapsed / 1e6,
                           speedup=speed, efficiency=speed / workers,
                           thread_speedup=thread_speed, thread_efficiency=thread_speed / workers,
                           lock_count_speedup=lock_speed,
                           lock_count_baseline=stripes[0] if row["kernel"] == "locks" else "",
                           round_min_ms=min(float(item["elapsed_s"]) for item in selected) * 1000,
                           round_max_ms=max(float(item["elapsed_s"]) for item in selected) * 1000,
                           samples=len(trials), trial_min_ms=min(trials) * 1000, trial_max_ms=max(trials) * 1000)
                results.append(row)
    save(results, output / "summary.csv")
    spec = importlib.util.spec_from_file_location("lab3_plot", HERE / "plot.py")
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    module.plot_results(output / "summary.csv", output, threads)
    print(f"Lab3 measurements and figures: {output}", file=sys.stderr)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--n", type=positive_int, nargs="+", default=DEFAULT_SIZES)
    parser.add_argument("--stripes", type=positive_int, nargs="+", default=DEFAULT_STRIPES)
    parser.add_argument("--distribution", choices=DEFAULT_DISTRIBUTIONS, nargs="+", default=DEFAULT_DISTRIBUTIONS)
    parser.add_argument("--bins", type=positive_int, default=256)
    parser.add_argument("--hot-percent", type=percentage, default=90)
    parser.add_argument("--threads", type=positive_int, default=4)
    parser.add_argument("--repeat", type=positive_int, default=7)
    parser.add_argument("--rounds", type=positive_int, default=3)
    parser.add_argument("--seed", type=seed_int, default=2026)
    parser.add_argument("--output", type=Path, default=HERE / "results")
    parser.add_argument("--build-dir", type=Path, default=HERE / "build", help="构建目录，例如 build-llvm")
    parser.add_argument("--verify", action="store_true", help="计时之外逐元素/逐桶核对结果")
    args = parser.parse_args()
    run_benchmark(args.output, threads=args.threads, sizes=args.n, stripes=args.stripes,
                  distributions=args.distribution, bins=args.bins, hot_percent=args.hot_percent,
                  repeat=args.repeat, rounds=args.rounds, seed=args.seed, build_dir=args.build_dir, verify=args.verify)


if __name__ == "__main__":
    run_main(main)
