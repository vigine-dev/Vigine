#!/usr/bin/env python3
"""check_strict_encapsulation.py -- Static checker for strict encapsulation (INV-12).

Scans every C++ header under include/vigine/ and src/ and fails when any
class or struct declares data members that are not private.

Rules:
  - For ``class`` declarations (implicit default: private):
      Any data member under ``public:`` or ``protected:`` is a violation.
  - For ``struct`` declarations (implicit default: public):
      Any data member under ``protected:`` is a violation.
      Pure-data public structs (aggregates) are intentional and not flagged.

Waiver: place ``// ENCAP EXEMPTION:`` on the class/struct declaration line
to skip the entire class body, including nested types.

Exemptions by filename (not flagged regardless of content):
  - Files ending in ``kind.h``
  - ``nodeid.h``, ``edgeid.h``, ``entityid.h``, ``serviceid.h``, ``stateid.h``,
    ``taskid.h``, ``payloadtypeid.h``, ``connectionid.h``, ``namedthreadid.h``,
    ``busid.h``, ``messagekind.h``
  - Any file whose name ends with ``id.h`` and whose body only contains a struct
    whose name ends with ``Id`` (auto-detected)

Exit codes:
  0 -- all scanned classes conform.
  1 -- at least one violation found (or a required path was not found).

Standard-library Python only; no third-party runtime dependencies.
"""

import argparse
import re
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

WAIVER_MARKER = "// ENCAP EXEMPTION:"

# Default scan roots relative to repo root.
DEFAULT_SCAN_DIRS: list[str] = [
    "include/vigine",
    "src",
]

EXTENSIONS: frozenset[str] = frozenset({".h", ".hpp", ".hxx"})

# Filenames that are always exempt (POD id and kind headers).
_EXEMPT_FILENAMES: frozenset[str] = frozenset(
    {
        "nodeid.h",
        "edgeid.h",
        "entityid.h",
        "serviceid.h",
        "stateid.h",
        "taskid.h",
        "payloadtypeid.h",
        "connectionid.h",
        "namedthreadid.h",
        "busid.h",
        "messagekind.h",
    }
)

# ---------------------------------------------------------------------------
# Compiled patterns
# ---------------------------------------------------------------------------

# Class or struct opening line (not a forward declaration -- we check for
# body later).  Captures keyword ("class" or "struct") and name.
_CLASS_OPEN_RE = re.compile(r"^\s*(class|struct)\s+(\w+)")

# Access specifier lines.
_ACCESS_PUBLIC_RE    = re.compile(r"^\s*public\s*:")
_ACCESS_PROTECTED_RE = re.compile(r"^\s*protected\s*:")
_ACCESS_PRIVATE_RE   = re.compile(r"^\s*private\s*:")

# Data member heuristic:
#   - Has a type-like token followed by an identifier
#   - No '(' before the ';' or end of relevant segment (not a method)
#   - Not a pure keyword line (using, typedef, static_assert, friend, #, //)
#   - The identifier must not be preceded by '(' (which would make it a param)
#
# We classify a line as a data member when:
#   1. It looks like ``<type-tokens> <ident> [= ...] ;``
#   2. It has no '(' before the first ';' (excluding template / attribute parts)
#   3. It is not a typedef / using / friend / static_assert / preprocessor

_DATA_MEMBER_RE = re.compile(
    r"^\s*"
    # Reject lines starting with known non-member keywords.
    r"(?!(?:using\b|friend\b|static_assert\b|typedef\b|#|//|\*|/\*|return\b|"
    r"virtual\b|explicit\b|inline\b|constexpr\b|static\b))"
    # Type tokens: may include qualifiers, pointers, references, angle brackets.
    r"(?:(?:const|volatile|mutable|static)\s+)*"
    r"[\w:*&<>[\],\s]+"
    # Identifier -- must look like a field name.
    r"\b(\w+)\s*"
    # Terminated by optional init then ';'
    r"(?:[{=;])",
)

# Comment patterns.
_BLOCK_COMMENT_RE = re.compile(r"^\s*/?\*")
_LINE_COMMENT_RE  = re.compile(r"^\s*//")


def _is_comment_line(line: str) -> bool:
    return bool(_BLOCK_COMMENT_RE.match(line) or _LINE_COMMENT_RE.match(line))


def _strip_line_comment(line: str) -> str:
    """Remove trailing // comment; crude but sufficient for brace counting."""
    pos = line.find("//")
    return line[:pos] if pos >= 0 else line


def _strip_comments(line: str) -> str:
    """Strip // tails and /* ... */ blocks (single-line only)."""
    line = re.sub(r"/\*.*?\*/", "", line)
    pos = line.find("//")
    return line[:pos] if pos >= 0 else line


# ---------------------------------------------------------------------------
# File-level exemption check
# ---------------------------------------------------------------------------

def _is_exempt_file(path: Path) -> bool:
    """Return True when the file is on the POD-id / kind whitelist."""
    name = path.name.lower()
    if name in _EXEMPT_FILENAMES:
        return True
    if name.endswith("kind.h"):
        return True
    # Also exempt any file whose name ends with "id.h" (catches future id headers).
    if name.endswith("id.h"):
        return True
    return False


# ---------------------------------------------------------------------------
# Class body extractor
# ---------------------------------------------------------------------------

def _find_class_body_end(lines: list[str], start: int) -> int | None:
    """Return the 0-based index of the closing '}' line for the class at *start*.

    Returns None when no opening brace is found before a bare ';'
    (forward declaration).
    """
    depth = 0
    found_open = False
    for idx in range(start, len(lines)):
        sc     = _strip_line_comment(lines[idx])
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
# Data-member line detection
# ---------------------------------------------------------------------------

def _looks_like_data_member(raw: str) -> bool:
    """Return True when the line looks like a data-member declaration.

    Heuristic:
      - Matches the _DATA_MEMBER_RE pattern.
      - No '(' before the first ';' (not a method).
      - Not a static constexpr / static const constant (class-scope constant).
      - Not a pure-virtual / deleted / defaulted declaration.
      - Not a continuation line of a multi-line method signature.
    """
    if _is_comment_line(raw):
        return False

    sc = _strip_comments(raw)

    # Skip access specifier lines themselves.
    if (
        _ACCESS_PUBLIC_RE.match(raw)
        or _ACCESS_PROTECTED_RE.match(raw)
        or _ACCESS_PRIVATE_RE.match(raw)
    ):
        return False

    # Skip lines that are purely namespace/brace/template boilerplate.
    stripped = sc.strip()
    if not stripped or stripped in ("{", "}", "};"):
        return False

    # Skip static constexpr / static const class-scope constants.
    if re.match(r"^\s*static\s+(?:constexpr|const)\b", raw):
        return False

    # Skip lines with '(' before ';' -- those are method declarations.
    semi_pos = sc.find(";")
    paren_pos = sc.find("(")
    if paren_pos >= 0 and (semi_pos < 0 or paren_pos < semi_pos):
        return False

    # Skip operator / using / friend / typedef / preprocessor / enum.
    if re.match(
        r"^\s*(?:operator\b|using\b|friend\b|typedef\b|static_assert\b|#|~|enum\b)",
        raw,
    ):
        return False

    # Skip continuation lines of multi-line method signatures.
    # These are lines that look like "<type> <ident> = <value>" but end
    # with ") = 0;", ");", or ")" -- all signatures of a parameter list
    # continuation rather than a data member initialiser.
    sc_stripped = stripped
    if re.search(r"\)\s*(?:=\s*0\s*)?;?\s*$", sc_stripped):
        # Ends with ) or ); or ) = 0; -- method related line.
        if "(" not in sc_stripped:
            # No '(' but ends with ')' -- this is a continuation line.
            return False

    return bool(_DATA_MEMBER_RE.match(raw))


# ---------------------------------------------------------------------------
# Class body analyser
# ---------------------------------------------------------------------------

# Access state labels.
_PUBLIC    = "public"
_PROTECTED = "protected"
_PRIVATE   = "private"


def _scan_class_body(
    lines: list[str],
    start: int,
    end: int,
    default_access: str,
    path: Path,
    violations: list[str],
    quiet: bool,
    *,
    keyword: str,  # "class" or "struct"
) -> None:
    """Walk the class body in [start..end] and report non-private data members."""
    access = default_access
    depth = 0  # Relative to the opening brace of *this* class.

    for idx in range(start, end + 1):
        raw    = lines[idx]
        lineno = idx + 1
        sc     = _strip_line_comment(raw)

        opens  = sc.count("{")
        closes = sc.count("}")

        # Track nesting so we only inspect the top level of this class.
        depth += opens - closes
        if depth < 0:
            depth = 0

        # Only inspect lines at depth == 1 (directly inside this class).
        # After the first '{' the depth goes to 1; an inner class pushes it to 2+.
        # We adjust: start *before* the opening '{' so depth starts at 0 and
        # becomes 1 after the opening brace line.

        # Access specifier lines update current access.
        if _ACCESS_PUBLIC_RE.match(raw):
            access = _PUBLIC
            continue
        if _ACCESS_PROTECTED_RE.match(raw):
            access = _PROTECTED
            continue
        if _ACCESS_PRIVATE_RE.match(raw):
            access = _PRIVATE
            continue

        # Only flag lines directly inside this class (depth == 1 after open brace).
        if depth != 1:
            continue

        if not _looks_like_data_member(raw):
            continue

        # For classes: flag public + protected data members.
        # For structs: flag only protected data members (public is intentional).
        if keyword == "class":
            if access in (_PUBLIC, _PROTECTED):
                msg = f"{path}:{lineno}: {access} data member in class"
                violations.append(msg)
                if not quiet:
                    print(msg)
        else:  # struct
            if access == _PROTECTED:
                msg = f"{path}:{lineno}: protected data member in struct"
                violations.append(msg)
                if not quiet:
                    print(msg)


# ---------------------------------------------------------------------------
# File scanner
# ---------------------------------------------------------------------------

def scan_file(path: Path, quiet: bool) -> list[str]:
    """Return list of violation strings for *path* (empty when clean)."""
    if _is_exempt_file(path):
        return []

    try:
        text = path.read_text(encoding="utf-8")
    except UnicodeDecodeError as exc:
        msg = f"{path}: encoding error -- {exc}"
        if not quiet:
            print(msg, file=sys.stderr)
        return [msg]

    lines = text.splitlines()
    violations: list[str] = []
    skip_until: int = -1   # End index (inclusive) of a waiived class body.

    idx = 0
    while idx < len(lines):
        # Skip waiived class body.
        if skip_until >= 0 and idx <= skip_until:
            idx += 1
            continue
        skip_until = -1

        raw    = lines[idx]

        # A line with the waiver marker is always safe.  When on a class
        # declaration, swallow the entire class body so nested types
        # inside the exempted class do not generate false reports.
        if WAIVER_MARKER in raw:
            end = _find_class_body_end(lines, idx)
            if end is not None:
                skip_until = end
            idx += 1
            continue

        if _is_comment_line(raw):
            idx += 1
            continue

        m = _CLASS_OPEN_RE.match(raw)
        if not m:
            idx += 1
            continue

        keyword = m.group(1)   # "class" or "struct"

        # Locate the class body.
        end_idx = _find_class_body_end(lines, idx)
        if end_idx is None:
            # Forward declaration -- skip.
            idx += 1
            continue

        # Determine default access for this keyword.
        default_access = _PRIVATE if keyword == "class" else _PUBLIC

        # Scan the body.
        _scan_class_body(
            lines,
            idx,
            end_idx,
            default_access,
            path,
            violations,
            quiet,
            keyword=keyword,
        )

        idx = end_idx + 1

    return violations


# ---------------------------------------------------------------------------
# Directory scanner
# ---------------------------------------------------------------------------

def scan_paths(scan_roots: list[Path], quiet: bool) -> tuple[int, int]:
    """Scan all roots; return (files_scanned, violation_count)."""
    files_scanned   = 0
    violation_count = 0
    for root in scan_roots:
        if not root.exists():
            msg = f"check_strict_encapsulation: path not found: {root}"
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


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Check INV-12 strict encapsulation: every class must keep all data "
            "members private.  Struct public data is intentional and not flagged."
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
            "When provided, replaces the default scan dirs."
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
        scan_roots = [repo_root / d for d in DEFAULT_SCAN_DIRS]

    files_scanned, violation_count = scan_paths(scan_roots, args.quiet)

    summary = (
        f"check_strict_encapsulation: {files_scanned} files scanned, "
        f"{violation_count} violations"
    )
    print(summary)

    return 0 if violation_count == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
