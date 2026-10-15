#!/usr/bin/env python3
"""Package source files, excluding machine-specific builds and results."""
import argparse
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT.parent / "parallel-computing-labs-source.zip")
    args = parser.parse_args()
    target = args.output.resolve()
    target.parent.mkdir(parents=True, exist_ok=True)
    build_dirs = {p.parent for p in ROOT.rglob("CMakeCache.txt")}
    with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED) as archive:
        for source in sorted(ROOT.rglob("*")):
            relative = source.relative_to(ROOT)
            if not source.is_file() or source.resolve() == target:
                continue
            if any(part in {"build", "results", "__pycache__", ".venv", ".cache", ".pytest_cache", "node_modules"} for part in relative.parts):
                continue
            if any(part.startswith("build-") for part in relative.parts) or any(parent in build_dirs for parent in source.parents):
                continue
            if relative.parts[:2] == ("website", "site"):
                continue
            if source.suffix in {".pyc", ".vcd", ".fst", ".zip", ".log", ".o", ".a", ".so", ".dylib", ".dll", ".exe", ".bin", ".pem", ".key"}:
                continue
            if any(part.startswith(".") and part != ".gitignore" for part in relative.parts):
                continue
            archive.write(source, Path("parallel-computing-labs") / relative)
    print(target)


if __name__ == "__main__":
    main()
