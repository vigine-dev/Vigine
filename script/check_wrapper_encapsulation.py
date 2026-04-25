#!/usr/bin/env python3
"""check_wrapper_encapsulation.py -- Static checker for wrapper encapsulation (INV-11).

Scans public wrapper headers under include/vigine/<domain>/ and fails
when any header references graph-substrate types or namespaces.  Two
modes are available:

  --mode wrapper (default)
      Checks Level-1 wrapper headers.  Forbidden tokens are the core
      graph primitive names and namespace qualifiers.

  --mode facade
      Extends the forbidden list with Level-2 facade-specific tokens
      that must never bleed through into facade public headers.

Exit codes:
  0 -- all scanned files clean.
  1 -- at least one violation found (or a required path was not found).

Waiver: a line containing ``// INV-11 EXEMPTION:`` is skipped without
error.  For class-scoped waivers place the marker on the class
declaration line; the checker skips that entire class body including
nested types.

Standard-library Python only; no third-party runtime dependencies.
"""

import argparse
import re
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

WAIVER_MARKER = "// INV-11 EXEMPTION:"

# Default scan roots relative to repo root -- one entry per wrapper domain.
DEFAULT_WRAPPER_PATHS: list[str] = [
    "include/vigine/api/service",
    "include/vigine/ecs",
    "include/vigine/statemachine",
    "include/vigine/taskflow",
    "include/vigine/api/context",
    "include/vigine/messaging",
    "include/vigine/payload",
    "include/vigine/threading",
]

# File extensions treated as C++ headers.
EXTENSIONS: frozenset[str] = frozenset({".h", ".hpp", ".hxx"})

# ---------------------------------------------------------------------------
# Forbidden token lists
# ---------------------------------------------------------------------------

# Level-1 wrapper forbidden identifiers (word-boundary match).
_WRAPPER_IDENTIFIERS: list[str] = [
    "NodeId",
    "EdgeId",
    "IGraph",
    "INode",
    "IEdge",
    "IEdgeData",
    "IGraphVisitor",
    "IGraphQuery",
    "AbstractGraph",
    "EdgeData",
    "TraverseMode",
]

# Substring tokens for Level-1 (namespace qualifiers and include paths).
_WRAPPER_SUBSTRINGS: list[str] = [
    "vigine::graph::",
    "<vigine/graph/",
]

# Additional identifiers forbidden in Level-2 facade headers.
_FACADE_EXTRA_IDENTIFIERS: list[str] = [
    "IBusControlBlock",
    "IConnectionToken",
]

# Additional substring tokens for Level-2 facades.
_FACADE_EXTRA_SUBSTRINGS: list[str] = [
    "vigine::graph::kind::",
]

# ---------------------------------------------------------------------------
# Pattern builders
# ---------------------------------------------------------------------------

# Matches lines that are entirely within a comment block.
_BLOCK_COMMENT_LINE = re.compile(r"^\s*/?\*")
_LINE_COMMENT       = re.compile(r"^\s*//")

# Matches the opening of a class/struct definition (not a forward decl).
_CLASS_OPEN_RE = re.compile(r"^\s*(?:class|struct)\s+(\w+)")


def _is_comment_line(line: str) -> bool:
    """Return True when the line starts with a comment marker."""
    return bool(_BLOCK_COMMENT_LINE.match(line) or _LINE_COMMENT.match(line))


def _build_patterns(
    identifiers: list[str],
    substrings: list[str],
) -> list[tuple[str, re.Pattern]]:
    """Return (token, compiled_pattern) pairs for all forbidden tokens."""
    pairs: list[tuple[str, re.Pattern]] = []
    for token in identifiers:
        pairs.append((token, re.compile(r"\b" + re.escape(token) + r"\b")))
    for token in substrings:
        pairs.append((token, re.compile(re.escape(token))))
    return pairs


# ---------------------------------------------------------------------------
# Body extractor (supports class-level waiver)
# ---------------------------------------------------------------------------

def _strip_line_comment(line: str) -> str:
    """Remove trailing // comment; crude but sufficient for brace counting."""
    pos = line.find("//")
    return line[:pos] if pos >= 0 else line


def _find_class_body_end(lines: list[str], start: int) -> int | None:
    """Return the 0-based index of the closing '}' line for the class at *start*.

    Returns None when no opening brace is found (forward declaration).
    """
    depth = 0
    found_open = False
    for idx in range(start, len(lines)):
        sc = _strip_line_comment(lines[idx])
        opens  = sc.count("{")
        closes = sc.count("}")
        if not found_open:
            if opens > 0:
                found_open = True
            elif ";" in sc:
                return None  # Forward declaration.
        if found_open:
            depth += opens - closes
            if depth <= 0:
                return idx
    return None


# ---------------------------------------------------------------------------
# File scanner
# ---------------------------------------------------------------------------

def scan_file(
    path: Path,
    patterns: list[tuple[str, re.Pattern]],
    quiet: bool,
) -> list[str]:
    """Return violation strings for *path* (empty when clean)."""
    try:
        text = path.read_text(encoding="utf-8")
    except UnicodeDecodeError as exc:
        msg = f"{path}: encoding error -- {exc}"
        if not quiet:
            print(msg, file=sys.stderr)
        return [msg]

    lines = text.splitlines()
    violations: list[str] = []
    skip_until: int = -1  # End index (inclusive) of a waived class body.

    idx = 0
    while idx < len(lines):
        # Skip waived class body (set below when a class carries the marker).
        if skip_until >= 0 and idx <= skip_until:
            idx += 1
            continue

        skip_until = -1  # Reset once past the waived block.
        raw    = lines[idx]
        lineno = idx + 1

        # A line with the waiver marker is always safe.  If the marker appears
        # on a CLASS / STRUCT declaration line we also skip the entire class
        # body so nested types cannot trigger false reports. Previously any
        # line carrying the marker activated the class-body-swallow walk,
        # which let a marker on an unrelated `#include` or `namespace` line
        # brace-count straight through a neighbouring namespace and skip
        # legitimate checks. Gating on `_CLASS_OPEN_RE` restricts the walk
        # to the intended placement site.
        if WAIVER_MARKER in raw:
            if _CLASS_OPEN_RE.match(raw):
                end = _find_class_body_end(lines, idx)
                if end is not None:
                    skip_until = end
            idx += 1
            continue

        # Pure comment lines cannot introduce real dependencies.
        if _is_comment_line(raw):
            idx += 1
            continue

        # Check every forbidden token.
        for token, pat in patterns:
            if pat.search(raw):
                msg = f"{path}:{lineno}: {token}"
                violations.append(msg)
                if not quiet:
                    print(msg)
                break  # One report per line is enough.

        idx += 1

    return violations


# ---------------------------------------------------------------------------
# Directory scanner
# ---------------------------------------------------------------------------

def scan_paths(
    scan_roots: list[Path],
    patterns: list[tuple[str, re.Pattern]],
    quiet: bool,
) -> tuple[int, int]:
    """Scan all roots; return (files_scanned, violation_count)."""
    files_scanned   = 0
    violation_count = 0
    for root in scan_roots:
        if not root.exists():
            msg = f"check_wrapper_encapsulation: path not found: {root}"
            print(msg, file=sys.stderr)
            violation_count += 1
            continue
        for p in sorted(root.rglob("*")):
            if p.suffix not in EXTENSIONS or not p.is_file():
                continue
            files_scanned += 1
            hits = scan_file(p, patterns, quiet)
            violation_count += len(hits)
    return files_scanned, violation_count


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Check INV-11 wrapper encapsulation: wrapper/facade public headers "
            "must not reference graph-substrate types or namespaces."
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
            "Path to scan (can be repeated). "
            "When provided, replaces the default wrapper paths."
        ),
    )
    parser.add_argument(
        "--mode",
        choices=["wrapper", "facade"],
        default="wrapper",
        help=(
            "Checking mode: 'wrapper' (default) checks Level-1 headers; "
            "'facade' extends the forbidden list for Level-2 facade headers."
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
        scan_roots = [
            p if p.is_absolute() else repo_root / p
            for p in args.extra_paths
        ]
    else:
        scan_roots = [repo_root / d for d in DEFAULT_WRAPPER_PATHS]

    # Build forbidden-token patterns.
    identifiers = list(_WRAPPER_IDENTIFIERS)
    substrings  = list(_WRAPPER_SUBSTRINGS)
    if args.mode == "facade":
        identifiers += _FACADE_EXTRA_IDENTIFIERS
        substrings  += _FACADE_EXTRA_SUBSTRINGS

    patterns = _build_patterns(identifiers, substrings)

    files_scanned, violation_count = scan_paths(scan_roots, patterns, args.quiet)

    summary = (
        f"check_wrapper_encapsulation: {files_scanned} files scanned, "
        f"{violation_count} violations"
    )
    print(summary)

    return 0 if violation_count == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
