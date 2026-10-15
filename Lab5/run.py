#!/usr/bin/env python3
"""Measure dense GEMM on already built SPM/systolic-array models."""
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
import subprocess
import sys

from build import binary_path
from cli_utils import positive_int, nonnegative_int, seed_int, stall_percentage, run_main

HERE = Path(__file__).resolve().parent


def save(rows, path):
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader(); writer.writerows(rows)


def run_case(m, k, n, array, tile_m, tile_k, tile_n, buffering="double", latency=12,
             beat_cycles=1, stall_percent=6, seed=2026, repeat=1, spm_tile=64, spm_k=128,
             trace=None, trace_start=0, trace_cycles=4096, dump_output=None, verify=False, max_cycles=100000000):
    options = {"m": m, "k": k, "n": n, "tile-m": tile_m, "tile-k": tile_k, "tile-n": tile_n,
               "buffering": buffering, "latency": latency, "beat-cycles": beat_cycles,
               "stall-percent": stall_percent, "seed": seed, "repeat": repeat, "max-cycles": max_cycles}
    if trace:
        trace = Path(trace).resolve(); trace.parent.mkdir(parents=True, exist_ok=True)
        options.update({"trace": trace, "trace-start": trace_start, "trace-cycles": trace_cycles})
    if dump_output:
        dump_output = Path(dump_output).resolve(); dump_output.parent.mkdir(parents=True, exist_ok=True)
        options["dump-output"] = dump_output
    command = [str(binary_path(array, spm_tile, spm_k))]
    if verify: command.append("--verify")
    for name, value in options.items(): command.extend(["--"+name, str(value)])
    return list(csv.DictReader(io.StringIO(subprocess.check_output(command, text=True))))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--m", type=positive_int, nargs="+", default=[128, 256, 512])
    parser.add_argument("--k", type=positive_int)
    parser.add_argument("--n", type=positive_int)
    parser.add_argument("--array", type=positive_int, choices=[4, 8, 16], nargs="+", default=[4, 8, 16])
    parser.add_argument("--tile", type=positive_int, nargs="+", default=[16, 32, 64])
    parser.add_argument("--tile-m", type=positive_int)
    parser.add_argument("--tile-k", type=positive_int)
    parser.add_argument("--tile-n", type=positive_int)
    parser.add_argument("--buffering", choices=["single", "double"], nargs="+", default=["single", "double"])
    parser.add_argument("--latency", type=nonnegative_int, default=12)
    parser.add_argument("--beat-cycles", type=positive_int, default=1)
    parser.add_argument("--stall-percent", type=stall_percentage, default=6)
    parser.add_argument("--seed", type=seed_int, default=2026)
    parser.add_argument("--repeat", type=positive_int, default=1)
    parser.add_argument("--spm-tile", type=positive_int, default=64)
    parser.add_argument("--spm-k", type=positive_int, default=128)
    parser.add_argument("--output", type=Path, default=HERE/"results")
    parser.add_argument("--trace", type=Path)
    parser.add_argument("--trace-start", type=nonnegative_int, default=0)
    parser.add_argument("--trace-cycles", type=positive_int, default=4096)
    parser.add_argument("--save-output", action="store_true")
    parser.add_argument("--verify", action="store_true", help="计时之外逐元素核对整数矩阵结果")
    parser.add_argument("--max-cycles", type=positive_int, default=100000000, help="每次仿真的最大工作周期")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    arrays = sorted(set(args.array)); sizes = sorted(set(args.m)); tiles = sorted(set(args.tile))
    cases = [(m, args.k if args.k is not None else m, args.n if args.n is not None else m, array, args.tile_m if args.tile_m is not None else tile, args.tile_k if args.tile_k is not None else tile,
              args.tile_n if args.tile_n is not None else tile, buffering) for m in sizes for array in arrays
             for tile in tiles for buffering in dict.fromkeys(args.buffering)]
    cases = list(dict.fromkeys(cases))
    if any(args.spm_tile % array != 0 for array in arrays):
        parser.error("--spm-tile 必须是所选阵列边长的整数倍。")
    for _, _, _, _, tm, tk, tn, _ in cases:
        if tm > args.spm_tile or tn > args.spm_tile or tk > args.spm_k:
            parser.error(f"分块 {tm}x{tk}x{tn} 超过 SPM 容量：M/N≤{args.spm_tile}，K≤{args.spm_k}。")
    for array in arrays:
        model = binary_path(array, args.spm_tile, args.spm_k)
        if not model.is_file() or not (model.parent / "build_environment.json").is_file():
            parser.error(f"缺少模型 {model}；先运行 python3 build.py --array {array} "
                         f"--spm-tile {args.spm_tile} --spm-k {args.spm_k}。")
    random.Random(args.seed).shuffle(cases)
    metadata = {"recorded_at_utc": datetime.now(timezone.utc).isoformat(),
                "system": platform.system(), "architecture": platform.machine(), "logical_cpus": os.cpu_count(),
                "parameters": {key: str(value) if isinstance(value, Path) else value for key, value in vars(args).items()},
                "builds": [json.loads((binary_path(a,args.spm_tile,args.spm_k).parent/"build_environment.json").read_text()) for a in arrays],
                "reference": json.loads((HERE/"reference.json").read_text()),
                "golden_ratio_commit": "fb012f772374bf826f81582a31e95455c91fcb26"}
    (args.output/"environment.json").write_text(json.dumps(metadata,ensure_ascii=False,indent=2)+"\n")
    raw = []
    for index,(m,k,n,array,tm,tk,tn,buffering) in enumerate(cases):
        print(f"Lab5 {index+1}/{len(cases)}: {m}x{k}x{n}, {array}x{array}, tile={tm}x{tk}x{tn}, {buffering}", file=sys.stderr, flush=True)
        name = f"m{m}_k{k}_n{n}_a{array}_tm{tm}_tk{tk}_tn{tn}_{buffering}"
        dump = args.output/"outputs"/(name+".bin") if args.save_output else None
        raw.extend(run_case(m,k,n,array,tm,tk,tn,buffering,args.latency,args.beat_cycles,args.stall_percent,
                            args.seed,args.repeat,args.spm_tile,args.spm_k,
                            args.trace if index==0 else None,args.trace_start,args.trace_cycles,dump,args.verify,args.max_cycles))
        save(raw,args.output/"raw.csv")
    summaries = [row.copy() for row in raw if row["record"]=="summary"]
    for row in summaries:
        baseline = next(r for r in summaries if r["m"]==row["m"] and r["k"]==row["k"] and r["n"]==row["n"]
                        and r["array"]==row["array"] and r["buffering"]==args.buffering[0]
                        and int(r["tile_m"])==min(int(s["tile_m"]) for s in summaries)
                        and int(r["tile_k"])==min(int(s["tile_k"]) for s in summaries)
                        and int(r["tile_n"])==min(int(s["tile_n"]) for s in summaries))
        row.update(speedup=float(baseline["cycles"])/float(row["cycles"]),
                   baseline_buffering=baseline["buffering"], baseline_tile_m=baseline["tile_m"],
                   baseline_tile_k=baseline["tile_k"], baseline_tile_n=baseline["tile_n"],
                   read_mib=float(row["read_bytes"])/1048576,write_mib=float(row["write_bytes"])/1048576)
    summaries.sort(key=lambda r: (int(r["m"]),int(r["array"]),int(r["tile_m"]),int(r["tile_k"]),int(r["tile_n"]),r["buffering"]))
    save(summaries,args.output/"summary.csv")
    spec=importlib.util.spec_from_file_location("lab5_plot",HERE/"plot.py")
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    module.plot_results(args.output/"summary.csv",args.output)
    print(f"Lab5 measurements and figures: {args.output}",file=sys.stderr)


if __name__ == "__main__": run_main(main)
