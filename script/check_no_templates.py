#!/usr/bin/env python3
"""check_no_templates.py -- Static checker for INV-1 in messaging headers.

INV-1 forbids templates in public-surface headers of the engine's
Level-1 wrappers. Messaging in particular is the fan-out hub every
facade consumes; a template leak on its public surface would cascade
through every facade's signature. This script scans one or more paths
(default: ``include/vigine/messaging/``) for ``template`` declarations
and reports any match.

Usage:

    python script/check_no_templates.py
    python script/check_no_templates.py --path include/vigine/messaging/
    python script/check_no_templates.py --path include/vigine/messaging/ --quiet

Exit codes:
    0 -- all scanned files are template-free.
    1 -- at least one violation found, or a scan path is missing.

Waiver: a line containing the token ``// INV-1 EXEMPTION:`` anywhere on
the same line as the ``template`` declaration is skipped without error.
Use the waiver only with architect approval; the rationale must be in
the line above the exemption.
"""

import argparse
import re
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

WAIVER_MARKER = "// INV-1 EXEMPTION:"

# Default scan roots, relative to the repo root.
DEFAULT_SCAN_DIRS: list[str] = ["include/vigine/messaging"]

# File extensions considered C++ sources or headers.
EXTENSIONS: frozenset[str] = frozenset({".h", ".hpp", ".hxx"})

# Matches a line-initial (possibly indented) ``template`` keyword
# followed by a ``<`` (the opening of the template parameter list).
# Using word-boundary anchors avoids accidental matches inside
# identifiers such as ``my_template_arg``.
TEMPLATE_PATTERN = re.compile(r"^\s*template\s*<")

# Matches the start of a block-comment line and a plain line-comment so
# documentation that mentions the word ``template`` does not trigger a
# violation.
_BLOCK_COMMENT_LINE = re.compile(r"^\s*/?\*")
_LINE_COMMENT = re.compile(r"^\s*//")


def _is_comment_line(line: str) -> bool:
    """Return True when the trimmed line begins with a comment marker."""
    return bool(_BLOCK_COMMENT_LINE.match(line) or _LINE_COMMENT.match(line))


def scan_file(path: Path, quiet: bool) -> list[str]:
    """Return the list of violation strings for *path* (empty when clean)."""
    try:
        text = path.read_text(encoding="utf-8")
    except UnicodeDecodeError as exc:
        msg = f"{path}: encoding error -- {exc}"
        if not quiet:
            print(msg, file=sys.stderr)
        return [msg]

    violations: list[str] = []
    for lineno, line in enumerate(text.splitlines(), start=1):
        if WAIVER_MARKER in line:
            continue
        if _is_comment_line(line):
            continue
        if TEMPLATE_PATTERN.search(line):
            hit = f"{path}:{lineno}: {line.strip()}"
            violations.append(hit)
            if not quiet:
                print(hit)
    return violations


def scan_paths(scan_roots: list[Path], quiet: bool) -> tuple[int, int]:
    """Scan all roots; return (files_scanned, violation_count)."""
    files_scanned = 0
    violation_count = 0
    for root in scan_roots:
        if not root.exists():
            msg = f"check_no_templates: path not found: {root}"
            print(msg, file=sys.stderr)
            violation_count += 1
            continue
        for p in sorted(root.rglob("*")):
            if p.suffix not in EXTENSIONS or not p.is_file():
                continue
            files_scanned += 1
            hits = scan_file(p, quiet)
            violation_count += len(hits)
    return files_scanned, violation_count


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Check INV-1 compliance: forbid templates in messaging headers."
        )
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=None,
        help="Repo root (default: parent of the directory containing this script).",
    )
    parser.add_argument(
        "--path",
        dest="extra_paths",
        action="append",
        type=Path,
        default=[],
        help=(
            "Additional path to scan (can be repeated). When provided, "
            "replaces the default scan dirs."
        ),
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="Suppress per-violation output; print only the summary line.",
    )
    args = parser.parse_args(argv)

    # Resolve repo root.
    if args.root is not None:
        repo_root = args.root.resolve()
    else:
        repo_root = Path(__file__).resolve().parent.parent

    # Resolve scan roots.
    if args.extra_paths:
        scan_roots = [p if p.is_absolute() else repo_root / p for p in args.extra_paths]
    else:
        scan_roots = [repo_root / d for d in DEFAULT_SCAN_DIRS]

    files_scanned, violation_count = scan_paths(scan_roots, args.quiet)

    summary = (
        f"check_no_templates: {files_scanned} files scanned, "
        f"{violation_count} violations"
    )
    print(summary)

    return 0 if violation_count == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
