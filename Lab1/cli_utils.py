"""Argument and build checks kept local so this lab works after independent download."""
import argparse
import os
from pathlib import Path
import subprocess
import sys


def nonnegative_int(text):
    if not text.isascii() or not text.isdecimal():
        raise argparse.ArgumentTypeError("请输入非负整数。")
    return int(text)


def positive_int(text):
    value = nonnegative_int(text)
    if value < 1:
        raise argparse.ArgumentTypeError("必须大于 0。")
    return value


def seed_int(text):
    value = nonnegative_int(text)
    if value > 0xffffffff:
        raise argparse.ArgumentTypeError("seed 不能超过 4294967295。")
    return value


def percentage(text):
    value = nonnegative_int(text)
    if value > 100:
        raise argparse.ArgumentTypeError("百分比必须在 0～100 之间。")
    return value


def stall_percentage(text):
    value = nonnegative_int(text)
    if value > 99:
        raise argparse.ArgumentTypeError("停顿比例必须在 0～99 之间；100 会使地址握手始终无法完成。")
    return value


def build_directory(here, requested, executable):
    directory = Path(requested or "build").expanduser()
    if not directory.is_absolute():
        directory = here / directory
    directory = directory.resolve()
    if not (directory / "CMakeCache.txt").is_file() or not (directory / executable).is_file():
        raise RuntimeError(f"构建目录 {directory} 缺少 CMakeCache.txt 或 {executable}。"
                           f"先运行 cmake -S . -B '{directory}' -DCMAKE_BUILD_TYPE=Release，"
                           f"再运行 cmake --build '{directory}' --parallel 4。")
    return directory


def check_threads(rows, requested):
    summaries = [row for row in rows if row.get("record") == "summary"]
    if len(summaries) != 1:
        raise RuntimeError("程序没有输出唯一的 summary 记录，请检查构建与程序输出。")
    actual = int(summaries[0]["threads"])
    if actual != requested:
        limit = os.environ.get("OMP_THREAD_LIMIT", "未设置")
        raise RuntimeError(f"请求 {requested} 线程，实际仅 {actual} 线程；"
                           f"OMP_THREAD_LIMIT={limit}。请调整 --threads 或检查 OpenMP 限制设置，"
                           "本次不会把受限制的配置当作请求线程数汇总。")


def run_main(main):
    try:
        main()
    except KeyboardInterrupt:
        print("运行已中断。raw.csv 保留已完成配置；summary.csv 仅在全部完成后生成。"
              "重新运行请使用新的 --output 目录。", file=sys.stderr)
        raise SystemExit(130)
    except (RuntimeError, OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"运行失败：{error}", file=sys.stderr)
        raise SystemExit(2)
