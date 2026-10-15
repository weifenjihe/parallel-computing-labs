#!/usr/bin/env python3
"""Build this experiment's Verilator models in a temporary source checkout."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile
from cli_utils import positive_int, run_main

HERE = Path(__file__).resolve().parent


def binary_path(array, spm_tile=64, spm_k=128):
    return HERE / "build" / f"a{array}_t{spm_tile}_k{spm_k}" / "sim"


def build(array, spm_tile=64, spm_k=128, jobs=2):
    if array not in [4, 8, 16] or spm_tile < array or spm_tile % array != 0 or spm_k < 1 or jobs < 1:
        raise RuntimeError("构建参数无效：array=4/8/16，spm-tile 为 array 的正整数倍，spm-k/jobs>0。")
    tool = shutil.which("verilator")
    compiler = shutil.which(os.environ.get("CXX", "c++"))
    if tool is None or compiler is None:
        raise RuntimeError("找不到 Verilator 或 C++ 编译器，请按 README 安装依赖并检查 PATH/CXX。")
    metadata = {"array": array, "spm_tile": spm_tile, "spm_k": spm_k,
                "verilator": subprocess.check_output([tool, "--version"], text=True).strip(),
                "compiler": compiler,
                "compiler_version": subprocess.check_output([compiler, "--version"], text=True).splitlines()[0],
                "system": platform.system(), "architecture": platform.machine(),
                "cxx_flags": "-std=c++17 -O3"}
    sources = sorted(p for folder in ["rtl", "src", "include"] for p in (HERE/folder).rglob("*") if p.is_file())
    digest = hashlib.sha256(json.dumps(metadata, sort_keys=True).encode())
    digest.update(Path(__file__).read_bytes())
    for source in sources:
        digest.update(str(source.relative_to(HERE)).encode()); digest.update(source.read_bytes())
    key = digest.hexdigest()
    binary = binary_path(array, spm_tile, spm_k)
    stamp = binary.parent / "source.sha256"
    if binary.exists() and stamp.exists() and stamp.read_text() == key:
        print(f"Already built: array={array}, SPM tile={spm_tile}, K={spm_k}", file=sys.stderr)
        return binary
    binary.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="lab5_systolic_") as temporary:
        stage = Path(temporary)
        for folder in ["rtl", "src", "include"]: shutil.copytree(HERE/folder, stage/folder)
        flags = (f"-std=c++17 -O3 -I{stage/'include'} -DLAB_ARRAY={array} "
                 f"-DLAB_SPM_TILE={spm_tile} -DLAB_SPM_K={spm_k}")
        command = [tool, "--cc", "--exe", "--build", "-j", str(jobs), "-Wall",
                   "--top-module", "matrix_accelerator", "--Mdir", "obj", "-o", "sim",
                   "--trace-vcd", "--trace-depth", "3", "-MAKEFLAGS", f"OPT_FAST=-O3 OPT_SLOW=-O3 CXX={compiler}", f"-GARRAY={array}",
                   f"-GMAX_TILE={spm_tile}", f"-GMAX_K={spm_k}", "-CFLAGS", flags]
        command += [str(p.relative_to(HERE)) for p in sources if p.suffix in [".sv", ".cpp"]]
        print(f"Building array={array}, SPM tile={spm_tile}, K={spm_k}", file=sys.stderr, flush=True)
        result = subprocess.run(command, cwd=stage, text=True, capture_output=True)
        (binary.parent/"build.log").write_text(result.stdout+result.stderr)
        if result.returncode:
            raise RuntimeError(f"Verilator 构建失败（退出码 {result.returncode}），详情见 {binary.parent/'build.log'}。")
        shutil.copy2(stage/"obj/sim", binary)
        stamp.write_text(key)
    (binary.parent/"build_environment.json").write_text(json.dumps(metadata, indent=2)+"\n")
    return binary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--array", type=positive_int, choices=[4, 8, 16], nargs="+", default=[4, 8, 16])
    parser.add_argument("--spm-tile", type=positive_int, default=64)
    parser.add_argument("--spm-k", type=positive_int, default=128)
    parser.add_argument("--jobs", type=positive_int, default=2)
    args = parser.parse_args()
    for array in sorted(set(args.array)): print(build(array, args.spm_tile, args.spm_k, args.jobs))


if __name__ == "__main__": run_main(main)
