#!/usr/bin/env python3
"""check_naming_convention.py -- Static checker for the I/Abstract naming convention (INV-10).

Scans include/vigine/**/*.h and src/**/*.h and verifies that:

  Rule 1 (I-prefix): A class whose name starts with "I" followed by an
    uppercase letter must NOT have any non-virtual method with an inline
    body and must NOT have any underscore-prefixed data member.
    (Special-member housekeeping -- virtual destructor = default/delete,
    copy/move = default/delete, protected default constructor = default --
    and virtual methods with optional-hook bodies are not counted. Non-
    member lines that happen to carry `(` -- `using Callback = std::function
    <...>`, `typedef void (*fn)(int);`, `friend bool operator==(...)`,
    `static_assert(sizeof(...) == 8);` -- are not member functions and
    therefore do not trigger Rule 1 either.)

  Rule 2 (Abstract-prefix): A class whose name starts with "Abstract" must
    have AT LEAST ONE method that is not pure-virtual (= 0) and not a
    deleted copy/move, OR at least one underscore-prefixed data member.
    A class that is entirely pure-virtual with no state should use the I tier.

Only top-level class/struct definitions are checked; forward declarations
(no body) are skipped; nested classes inside a body are not re-checked.

Waiver: when the class declaration line contains "// INV-10 EXEMPTION:"
the ENTIRE class body is skipped without error, including any nested
class or struct declared inside it.

Exit codes:
  0 -- all checked classes conform.
  1 -- at least one violation found (or a scan path was not found).
"""

import argparse
import re
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

WAIVER_MARKER = "// INV-10 EXEMPTION:"

# Default scan roots cover both the public engine API (`include/vigine`) and
# the internal implementation (`src/`). INV-10 applies to class declarations
# anywhere in the engine tree, not only the public surface — a concrete
# `IFoo` helper that forgets the prefix convention in `src/` must still
# trip CI.
DEFAULT_SCAN_DIRS: list[str] = ["include/vigine", "src"]

EXTENSIONS: frozenset[str] = frozenset({".h", ".hpp"})

# ---------------------------------------------------------------------------
# Compiled patterns
# ---------------------------------------------------------------------------

# Class/struct declaration opening line.
_CLASS_OPEN_RE = re.compile(r"^\s*(?:class|struct)\s+(\w+)")

# Pure-virtual: "= 0;"
_PURE_VIRTUAL_RE = re.compile(r"=\s*0\s*;")

# Deleted or defaulted: "= delete;" or "= default;"
_DEL_DEF_RE = re.compile(r"=\s*(?:delete|default)\s*;")

# Data member: underscore-prefixed identifier (Vigine field convention).
# Must not look like a method (no '(' before the identifier).
_DATA_MEMBER_RE = re.compile(
    r"^\s*"
    r"(?!(?:using\b|friend\b|static_assert\b|typedef\b|#|//|\*|/\*|return\b))"
    r"[\w:*&<>,\s]+"    # type (crude)
    r"\b(_\w+)\s*"      # _field_name
    r"(?:[{=;]|$)",     # initialiser or end of statement
)

# A line that declares or defines a method (has parentheses) and is NOT
# pure-virtual and NOT deleted/defaulted.  Used for the Abstract Rule 2.
_METHOD_DECL_RE = re.compile(r"\(")   # any line with '(' is a method candidate

# Non-method lines that may legitimately contain `(` inside a class body
# and therefore must NOT trip the "has non-virtual method" branch of
# Rule 1. Each pattern matches the beginning of a stripped line.
_NON_METHOD_LINE_RES: tuple[re.Pattern[str], ...] = (
    re.compile(r"^\s*using\b"),          # `using Callback = std::function<...>;`
    re.compile(r"^\s*typedef\b"),        # `typedef void (*fn)(int);`
    re.compile(r"^\s*friend\b"),         # `friend bool operator==(const X&, ...);`
    re.compile(r"^\s*static_assert\b"),  # `static_assert(sizeof(...) == 8);`
)


def _is_non_method_line(sc: str) -> bool:
    return any(pat.match(sc) for pat in _NON_METHOD_LINE_RES)

# Comments.
_BLOCK_COMMENT_RE = re.compile(r"^\s*/?\*")
_LINE_COMMENT_RE  = re.compile(r"^\s*//")


def _is_comment_line(line: str) -> bool:
    return bool(_BLOCK_COMMENT_RE.match(line) or _LINE_COMMENT_RE.match(line))


def _strip_comments(line: str) -> str:
    """Strip // tail and /* ... */ blocks (crude -- ignores string literals)."""
    line = re.sub(r"/\*.*?\*/", "", line)
    pos = line.find("//")
    return line[:pos] if pos >= 0 else line


# ---------------------------------------------------------------------------
# Class body extraction
# ---------------------------------------------------------------------------

def _extract_class_body(lines: list[str], start: int) -> tuple[list[str], int] | None:
    """Return (body_lines, end_idx_0based) for the class opening at *start*.

    Searches forward from *start* for '{'.  Returns None when a ';' is found
    first (forward declaration) or EOF is reached without finding '{'.
    body_lines span from the '{' line to the matching '}'.
    """
    depth = 0
    body: list[str] = []
    found_open = False

    for idx in range(start, len(lines)):
        raw = lines[idx]
        sc  = _strip_comments(raw)
        opens  = sc.count("{")
        closes = sc.count("}")

        if not found_open:
            if opens > 0:
                found_open = True
            elif ";" in sc:
                return None  # Forward declaration.

        if found_open:
            depth  += opens - closes
            body.append(raw)
            if depth <= 0:
                return body, idx

    return None


# ---------------------------------------------------------------------------
# Continuation joiner
# ---------------------------------------------------------------------------

def _join_continuations(body_lines: list[str]) -> list[str]:
    """Collapse C++ declarations that span multiple source lines into
    single-line equivalents before per-line analysis.

    A line that does not end with a statement-terminating character
    (`;`, `{`, `}`, `:`) is glued to the next non-blank non-comment line.
    This lets the analyser see idiomatic multi-line signatures like

        [[nodiscard]] virtual Result
            emit(std::unique_ptr<Payload> p) = 0;

    as one logical statement where both `virtual` and `= 0;` are visible
    at once — rather than separately flagging the parameter line as a
    non-virtual method with a body.

    Comment lines and blank lines pass through verbatim so the outer
    depth-counter (which counts `{` and `}`) still balances correctly.
    """
    out: list[str] = []
    buf: str = ""

    def _flush_buf() -> None:
        nonlocal buf
        if buf:
            out.append(buf)
            buf = ""

    for raw in body_lines:
        if not raw.strip() or _is_comment_line(raw):
            _flush_buf()
            out.append(raw)
            continue

        sc = _strip_comments(raw).rstrip()
        if buf:
            buf = buf.rstrip() + " " + raw.strip()
        else:
            buf = raw

        # End of logical statement when the comment-stripped line ends
        # with one of the terminating characters.
        if sc and sc[-1] in ";{}:":
            _flush_buf()

    _flush_buf()
    return out


# ---------------------------------------------------------------------------
# Class body analyser
# ---------------------------------------------------------------------------

class _ClassInfo:
    """Structural summary of one class body for naming-convention checks.

    Attributes (all at top-level scope -- depth-1 lines only):
      has_pure_virtual             -- at least one "= 0;" method.
      has_non_pure_non_virtual_method
                                   -- at least one non-virtual method that is
                                      not "= 0", "= default", or "= delete".
                                      (Non-virtual methods with bodies count.)
      has_any_non_delete_method    -- at least one method line that is not
                                      "= delete" (used for Abstract Rule 2).
      has_data_member              -- at least one underscore-prefixed field.
    """

    def __init__(self, body_lines: list[str]) -> None:
        self.has_pure_virtual              = False
        self.has_non_pure_non_virtual_impl = False
        self.has_any_non_delete_method     = False
        self.has_data_member               = False
        self._parse(_join_continuations(body_lines))

    def _parse(self, body_lines: list[str]) -> None:
        depth = 0  # 1 = directly inside the class being analysed.

        for raw in body_lines:
            line = raw.rstrip()
            if _is_comment_line(line):
                continue

            sc     = _strip_comments(line)
            opens  = sc.count("{")
            closes = sc.count("}")
            before = depth
            depth += opens - closes
            if depth < 0:
                depth = 0

            # Only inspect lines directly inside the class (before=1 or
            # transitions through depth 1 when opens == closes == 0).
            if before != 1:
                continue

            # -- Pure virtual --
            if _PURE_VIRTUAL_RE.search(line):
                self.has_pure_virtual = True
                # A "= 0;" line cannot also be non-pure.
                continue

            # -- Deleted / defaulted special members --
            # These are NOT counted as "non-pure implementations" for Rule 1
            # (they are housekeeping, not semantics), but they ARE counted as
            # "has a non-deleted method" for Rule 2.
            if _DEL_DEF_RE.search(line):
                if "= delete" not in line:
                    # "= default" counts as "has a non-deleted method."
                    self.has_any_non_delete_method = True
                continue

            # -- Method lines (have parentheses) --
            if _METHOD_DECL_RE.search(sc):
                # Some lines carry `(` but do not declare a member function
                # of the current class (type aliases, forward friend
                # declarations, static-assert expressions). They must not
                # trip the "has non-virtual method" check — Rule 1 is about
                # member functions specifically.
                if _is_non_method_line(sc):
                    continue
                # A non-pure, non-delete, non-default method.
                self.has_any_non_delete_method = True
                # Check whether it is non-virtual (I-prefix violation).
                # Use the comment-stripped form so that words inside comments
                # (e.g. "/* non-virtual body */") do not falsely match.
                sc_nc = re.sub(r"/\*.*?\*/", "", sc)
                if "virtual" not in sc_nc:
                    self.has_non_pure_non_virtual_impl = True
                continue

            # -- Data members (underscore convention, no parentheses) --
            if _DATA_MEMBER_RE.match(line) and "(" not in sc:
                self.has_data_member = True

    # -- Rule 1 helpers --

    def i_prefix_ok(self) -> bool:
        """Return True when the class is a valid I-interface.

        An I-interface has:
          - no non-virtual methods with bodies (non-pure non-virtual impl)
          - no data members
        Virtual methods with default bodies are tolerated (optional hook pattern).
        """
        return not self.has_non_pure_non_virtual_impl and not self.has_data_member

    # -- Rule 2 helpers --

    def abstract_prefix_ok(self) -> bool:
        """Return True when the class legitimately uses the Abstract prefix.

        An Abstract class has at least one non-deleted method (which means it
        provides some implementation beyond pure-virtual contracts) OR at least
        one data member.
        """
        return self.has_any_non_delete_method or self.has_data_member


# ---------------------------------------------------------------------------
# Prefix helpers
# ---------------------------------------------------------------------------

_I_PREFIX_RE       = re.compile(r"^I[A-Z]")       # IFoo
_ABSTRACT_PREFIX_RE = re.compile(r"^Abstract\w")   # AbstractFoo


def _has_i_prefix(name: str) -> bool:
    return bool(_I_PREFIX_RE.match(name))


def _has_abstract_prefix(name: str) -> bool:
    return bool(_ABSTRACT_PREFIX_RE.match(name))


# ---------------------------------------------------------------------------
# File scanner
# ---------------------------------------------------------------------------

def scan_file(path: Path, quiet: bool) -> list[str]:
    """Return list of violation strings for *path* (empty when clean)."""
    try:
        text = path.read_text(encoding="utf-8")
    except UnicodeDecodeError as exc:
        msg = f"{path}: encoding error -- {exc}"
        if not quiet:
            print(msg, file=sys.stderr)
        return [msg]

    lines = text.splitlines()
    violations: list[str] = []

    idx = 0
    while idx < len(lines):
        raw    = lines[idx]
        lineno = idx + 1

        # A waiver marker placed on a class-open line (the idiomatic
        # placement — see `ISignalEmiter`, `IHasState`) skips the WHOLE
        # class body, not just the marker line. Without this, nested
        # types inside a waivered class (`class Inner { ... };`) would
        # be re-checked at the outer scope and produce spurious
        # violations on declarations the waiver was meant to cover.
        if WAIVER_MARKER in raw:
            if _CLASS_OPEN_RE.match(raw):
                result = _extract_class_body(lines, idx)
                if result is not None:
                    _body, end_idx = result
                    idx = end_idx + 1
                    continue
            idx += 1
            continue

        if _is_comment_line(raw):
            idx += 1
            continue

        m = _CLASS_OPEN_RE.match(raw)
        if not m:
            idx += 1
            continue

        name = m.group(1)

        result = _extract_class_body(lines, idx)
        if result is None:
            idx += 1
            continue

        body_lines, end_idx = result

        if _has_i_prefix(name):
            info = _ClassInfo(body_lines)
            if not info.i_prefix_ok():
                if info.has_data_member:
                    detail = "has data member(s)"
                else:
                    detail = "has non-virtual method(s) with bodies"
                msg = (
                    f"{path}:{lineno}: naming: '{name}' has 'I' prefix but "
                    f"{detail} -- should be renamed to 'Abstract{name[1:]}'"
                )
                violations.append(msg)
                if not quiet:
                    print(msg)

        elif _has_abstract_prefix(name):
            info = _ClassInfo(body_lines)
            if not info.abstract_prefix_ok():
                suffix = name[len("Abstract"):]
                msg = (
                    f"{path}:{lineno}: naming: '{name}' has 'Abstract' prefix but "
                    f"all methods are pure-virtual or deleted and there are no data "
                    f"members -- should be renamed to 'I{suffix}'"
                )
                violations.append(msg)
                if not quiet:
                    print(msg)

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
            msg = f"check_naming_convention: path not found: {root}"
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
            "Check INV-10 naming convention: I-prefixed classes must be pure-virtual "
            "interfaces; Abstract-prefixed classes must carry state or non-pure methods."
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
            "Additional path to scan (can be repeated). "
            "When provided, replaces the default scan dirs."
        ),
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="Suppress per-violation output; print only the summary line.",
    )
    args = parser.parse_args(argv)

    if args.root is not None:
        repo_root = args.root.resolve()
    else:
        repo_root = Path(__file__).resolve().parent.parent

    if args.extra_paths:
        scan_roots = [p if p.is_absolute() else repo_root / p for p in args.extra_paths]
    else:
        scan_roots = [repo_root / d for d in DEFAULT_SCAN_DIRS]

    files_scanned, violation_count = scan_paths(scan_roots, args.quiet)

    summary = (
        f"check_naming_convention: {files_scanned} files scanned, "
        f"{violation_count} violations"
    )
    print(summary)

    return 0 if violation_count == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
