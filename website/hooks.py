"""Keep operation pages and source downloads in sync with the independent labs."""

from io import BytesIO
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

from mkdocs.structure.files import File

ROOT = Path(__file__).resolve().parent.parent
OMIT = {"build", "results", ".venv", "__pycache__", ".git"}


def on_page_markdown(markdown, page, config, files):
    for number in range(1, 6):
        marker = f"{{{{LAB{number}_README}}}}"
        if marker in markdown:
            readme = (ROOT / f"Lab{number}" / "README.md").read_text(encoding="utf-8")
            # The operation page supplies its own title and introductory guidance.
            markdown = markdown.replace(marker, readme.split("\n", 1)[1].lstrip())
    return markdown


def on_files(files, config):
    for number in range(1, 6):
        lab = ROOT / f"Lab{number}"
        build_dirs = {p.parent for p in lab.rglob("CMakeCache.txt")}
        archive = BytesIO()
        with ZipFile(archive, "w", ZIP_DEFLATED) as output:
            for path in sorted(lab.rglob("*")):
                relative = path.relative_to(lab)
                if (not path.is_file() or OMIT.intersection(relative.parts)
                        or any(part.startswith("build-") for part in relative.parts)
                        or any(parent in build_dirs for parent in path.parents)
                        or path.name == ".DS_Store" or path.suffix in {".pyc", ".vcd", ".fst"}):
                    continue
                output.write(path, str(Path(lab.name) / relative))
        files.append(File.generated(config, f"downloads/Lab{number}.zip", content=archive.getvalue()))
    return files
