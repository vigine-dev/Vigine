#!/usr/bin/env python3
"""check_graph_purity.py -- Static checker for graph-core purity (INV-9).

Scans include/vigine/graph/ and src/graph/ for engine-layer tokens:
forbidden include paths and engine-concept identifiers. Any match exits 1.

Exit codes:
  0 -- all files clean.
  1 -- at least one violation found (or path not found).

Waiver: a line containing the token // INV-9 EXEMPTION: anywhere on the
same line as the forbidden token is skipped without error.
"""

import argparse
import re
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Forbidden include patterns -- any #include containing these path segments
# is an engine-layer reference that must not appear in graph-core files.
# ---------------------------------------------------------------------------
FORBIDDEN_INCLUDE_SEGMENTS: list[str] = [
    "vigine/messaging/",
    "vigine/service/",
    "vigine/ecs/",
    "vigine/fsm/",
    "vigine/taskflow/",
    "vigine/context/",
]

# ---------------------------------------------------------------------------
# Forbidden identifier tokens (default list from plan_27).
# These are typical wrapper-layer names that must not appear in graph headers
# or sources. The --forbid CLI option can override this set entirely.
# ---------------------------------------------------------------------------
DEFAULT_FORBIDDEN_IDENTIFIERS: list[str] = [
    "Target",
    "Subscription",
    "Entity",
    "Component",
    "State",
    "Task",
    "Message",
    "Attached",
    "Transition",
    "ChildOf",
    "DependsOn",
    # Explicit engine-layer namespace qualifiers and type names:
    "IMessageBus",
    "IService",
    "IECS",
    "IStateMachine",
    "ITaskFlow",
    "IContext",
    "vigine::messaging::",
    "vigine::service::",
    "vigine::ecs::",
    "vigine::fsm::",
    "vigine::taskflow::",
    "vigine::context::",
]

WAIVER_MARKER = "// INV-9 EXEMPTION:"

# Directories scanned relative to repo root.
SCAN_DIRS: list[str] = [
    "include/vigine/graph",
    "src/graph",
]

# File extensions considered C++ sources / headers.
EXTENSIONS: frozenset[str] = frozenset({".h", ".hpp", ".cpp", ".cc", ".cxx"})


def _build_include_pattern(segments: list[str]) -> re.Pattern:
    """Return a compiled pattern that matches any forbidden include path."""
    # Match: # include <.../segment/...> or "...segment..."
    escaped = [re.escape(s) for s in segments]
    combined = "|".join(escaped)
    return re.compile(r'#\s*include\s+[<"](?:[^>"]*(?:' + combined + r')[^>"]*)(?:[>"])')


def _build_identifier_patterns(identifiers: list[str]) -> list[tuple[str, re.Pattern]]:
    """Return (token, pattern) pairs for word-boundary or substring matching."""
    pairs = []
    for token in identifiers:
        # Namespace qualifiers (contain ::) use substring match.
        # Plain identifiers use word boundaries.
        if "::" in token:
            pat = re.compile(re.escape(token))
        else:
            pat = re.compile(r"\b" + re.escape(token) + r"\b")
        pairs.append((token, pat))
    return pairs


# Matches the opening of a C-style block-comment line (including Doxygen markers).
_BLOCK_COMMENT_LINE = re.compile(r"^\s*/?\*")
_LINE_COMMENT = re.compile(r"^\s*//")


def _is_comment_line(line: str) -> bool:
    """Return True when the trimmed line is entirely inside a comment.

    Catches:
      //  single-line comments
      /*  block-comment openers
       *  continuation lines of block comments (e.g. Doxygen /** ... */)
    Does not catch mixed lines like ``code; // comment`` -- those are
    intentionally checked for identifiers since the code part is real.
    """
    return bool(_BLOCK_COMMENT_LINE.match(line) or _LINE_COMMENT.match(line))


def scan_file(
    path: Path,
    include_pat: re.Pattern,
    id_patterns: list[tuple[str, re.Pattern]],
    quiet: bool,
) -> list[str]:
    """Return list of violation strings for *path* (empty when clean)."""
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

        # Check forbidden includes -- always applies, even in comments, because
        # a commented-out include can hide an intent and is still a red flag.
        # But in practice, an #include never appears inside a block comment, so
        # this is just a belt-and-suspenders check.
        #
        # The include pattern already accepts optional whitespace between `#`
        # and `include` (`#\s*include`), so we skip the fast-path substring
        # gate on `"#include"` — that gate missed the valid preprocessor
        # form `# include <...>` (with the space) and turned a reformatted
        # header into a silent bypass. Running the regex unconditionally on
        # every line is a few microseconds and removes the bypass path.
        m = include_pat.search(line)
        if m:
            hit = m.group(0).strip()
            violations.append(f"{path}:{lineno}: {hit}")
            if not quiet:
                print(violations[-1])
            continue  # Don't double-report the same line for identifiers.

        # Identifier checks are skipped for pure comment lines.  Doc comments
        # routinely reference wrapper-layer namespaces as examples without
        # creating a real dependency (e.g. the Doxygen in kind.h that mentions
        # `vigine::messaging::kind` only to explain the convention).
        if _is_comment_line(line):
            continue

        # Check forbidden identifiers.
        for token, pat in id_patterns:
            if pat.search(line):
                violations.append(f"{path}:{lineno}: {token}")
                if not quiet:
                    print(violations[-1])
                break  # One report per line.

    return violations


def scan_paths(
    scan_roots: list[Path],
    include_pat: re.Pattern,
    id_patterns: list[tuple[str, re.Pattern]],
    quiet: bool,
) -> tuple[int, int]:
    """Scan all roots; return (files_scanned, violation_count)."""
    files_scanned = 0
    violation_count = 0
    for root in scan_roots:
        if not root.exists():
            msg = f"check_graph_purity: path not found: {root}"
            print(msg, file=sys.stderr)
            violation_count += 1
            continue
        for p in sorted(root.rglob("*")):
            if p.suffix not in EXTENSIONS or not p.is_file():
                continue
            files_scanned += 1
            hits = scan_file(p, include_pat, id_patterns, quiet)
            violation_count += len(hits)
    return files_scanned, violation_count


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Check graph-core purity: no engine-layer tokens in graph headers/sources."
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
            "Path to scan; overrides the default scan dirs entirely when "
            "supplied. Can be repeated to supply several. Note: this does "
            "NOT append to the defaults — passing --path replaces them."
        ),
    )
    parser.add_argument(
        "--forbid",
        type=str,
        default=None,
        help=(
            "Comma-separated list of forbidden identifier tokens. "
            "Overrides the built-in default list entirely."
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
        scan_roots = [repo_root / d for d in SCAN_DIRS]

    # Resolve identifier list.
    if args.forbid is not None:
        identifiers = [tok.strip() for tok in args.forbid.split(",") if tok.strip()]
    else:
        identifiers = list(DEFAULT_FORBIDDEN_IDENTIFIERS)

    include_pat = _build_include_pattern(FORBIDDEN_INCLUDE_SEGMENTS)
    id_patterns = _build_identifier_patterns(identifiers)

    files_scanned, violation_count = scan_paths(scan_roots, include_pat, id_patterns, args.quiet)

    summary = (
        f"check_graph_purity: {files_scanned} files scanned, {violation_count} violations"
    )
    print(summary)

    return 0 if violation_count == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
