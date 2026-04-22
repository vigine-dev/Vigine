#!/usr/bin/env python3
"""check_cross_platform.py -- Static checker for cross-platform portability (AD-2).

Scans include/vigine/ and src/ for platform-specific API tokens (Win32, POSIX,
macOS) that appear outside a conditional compilation guard.  Files whose names
signal a single-platform target are automatically exempt, so the legitimate
platform-family implementations under src/platform/ do not generate noise.

Exit codes:
  0 -- all scanned files clean.
  1 -- at least one violation found (or a required path was not found).

Filename-based auto-waivers (file is skipped entirely):
  - *_win.*, *_windows.*, src/*/platform/win*/* -> Win32-only.
  - *_macos.*, *_mac.*, *.mm, src/*/platform/mac*/* -> macOS-only.
  - *_posix.*, *_linux.*, src/platform/linux/*, src/*/platform/linux/* -> POSIX-only.

Inline waiver: place "// CROSS-PLATFORM EXEMPTION:" on any line -- that line
is skipped.  Place it on a class / struct / function declaration line to skip
the entire body that follows (FF-9 style, matching check_wrapper_encapsulation).

Token check: platform-specific identifiers that appear OUTSIDE a
#if defined(_WIN32) / #if defined(__unix__) / #if defined(__APPLE__) guard
block are reported.  Inside a guard the code is intentional and correct.

Standard-library Python only; no third-party runtime dependencies.
"""

import argparse
import re
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

WAIVER_MARKER = "// CROSS-PLATFORM EXEMPTION:"

# Default scan roots relative to repo root.
DEFAULT_SCAN_DIRS: list[str] = [
    "include/vigine",
    "src",
]

# File extensions treated as C++ sources / headers.
EXTENSIONS: frozenset[str] = frozenset({".h", ".hpp", ".hxx", ".cpp", ".cc", ".cxx"})

# ---------------------------------------------------------------------------
# Platform token lists
# ---------------------------------------------------------------------------

# Win32-only identifiers -- word-boundary match.
_WIN32_IDENTIFIERS: list[str] = [
    "HANDLE",
    "WINAPI",
    "HRESULT",
    "DWORD",
    "HWND",
    "HINSTANCE",
    "HMODULE",
    "BOOL",           # Win32 BOOL, not C++ bool
    "CreateFileW",
    "CreateFileA",
    "CreateFile",
    "CloseHandle",
    "SetConsoleCtrlHandler",
    "GetLastError",
    "LoadLibraryW",
    "LoadLibraryA",
    "GetProcAddress",
    "INVALID_HANDLE_VALUE",
    "WAIT_OBJECT_0",
    "CTRL_C_EVENT",
    "CTRL_BREAK_EVENT",
    "CTRL_CLOSE_EVENT",
]

# Win32-only include substrings -- substring match.
_WIN32_INCLUDES: list[str] = [
    "<windows.h>",
    "<winsock2.h>",
    "<ws2tcpip.h>",
    "<winbase.h>",
    "<windef.h>",
    "<handleapi.h>",
    "<synchapi.h>",
    "<fileapi.h>",
    "<consoleapi.h>",
]

# POSIX-only identifiers -- word-boundary match.
_POSIX_IDENTIFIERS: list[str] = [
    "pid_t",
    "uid_t",
    "gid_t",
    "ssize_t",
    "sigaction",
    "sigemptyset",
    "sigaddset",
    "sigprocmask",
    "fork",
    "execv",
    "execvp",
    "waitpid",
    "pthread_t",
    "pthread_create",
    "pthread_join",
    "pthread_mutex_t",
    "pthread_mutex_lock",
    "pthread_mutex_unlock",
    "pipe2",
    "SA_RESTART",
    "SIGTERM",
    "SIGINT",
    "SIGHUP",
    "SIGUSR1",
    "SIGUSR2",
    "SIG_DFL",
    "SIG_IGN",
]

# POSIX-only include substrings -- substring match.
_POSIX_INCLUDES: list[str] = [
    "<unistd.h>",
    "<pthread.h>",
    "<signal.h>",
    "<sys/types.h>",
    "<sys/socket.h>",
    "<sys/wait.h>",
    "<sys/stat.h>",
    "<sys/mman.h>",
    "<sys/epoll.h>",
    "<sys/event.h>",
    "<sys/select.h>",
    "<fcntl.h>",
    "<poll.h>",
    "<netinet/",
    "<arpa/",
]

# macOS-only identifiers -- word-boundary match.
_MACOS_IDENTIFIERS: list[str] = [
    "CFRunLoopRef",
    "CFStringRef",
    "CFBundleRef",
    "NSObject",
    "NSString",
    "NSApplication",
    "NSAutoreleasePool",
    "dispatch_queue_t",
    "dispatch_async",
    "dispatch_sync",
    "kCFRunLoopDefaultMode",
    "mach_port_t",
    "mach_task_self",
]

# macOS-only include substrings -- substring match.
_MACOS_INCLUDES: list[str] = [
    "<CoreFoundation/",
    "<Foundation/",
    "<AppKit/",
    "<UIKit/",
    "<Cocoa/",
    "<dispatch/",
    "<mach/",
    "<objc/",
]

# ---------------------------------------------------------------------------
# Guard patterns
# ---------------------------------------------------------------------------

# Opening a platform guard block.
_GUARD_WIN32_OPEN   = re.compile(r"#\s*if(?:def\s+_WIN32|\s+defined\s*\(\s*_WIN32\s*\))")
_GUARD_UNIX_OPEN    = re.compile(
    r"#\s*if\s+defined\s*\(\s*(?:__unix__|__linux__|__ANDROID__|POSIX)\s*\)"
    r"|#\s*ifdef\s+(?:__unix__|__linux__|POSIX)"
)
_GUARD_APPLE_OPEN   = re.compile(r"#\s*if(?:def\s+__APPLE__|\s+defined\s*\(\s*__APPLE__\s*\))")

# Close of the current #if block (at depth 0 relative to the guard).
_GUARD_ENDIF        = re.compile(r"#\s*endif")
_GUARD_ELIF_OR_ELSE = re.compile(r"#\s*(?:elif|else)")

# Opening any other #if (to count nesting depth).
_ANY_IF_OPEN        = re.compile(r"#\s*if(?:n?def)?\b")

# ---------------------------------------------------------------------------
# Comment patterns
# ---------------------------------------------------------------------------

_BLOCK_COMMENT_LINE = re.compile(r"^\s*/?\*")
_LINE_COMMENT       = re.compile(r"^\s*//")


def _is_comment_line(line: str) -> bool:
    return bool(_BLOCK_COMMENT_LINE.match(line) or _LINE_COMMENT.match(line))


def _strip_line_comment(line: str) -> str:
    pos = line.find("//")
    return line[:pos] if pos >= 0 else line


# ---------------------------------------------------------------------------
# Body extractor (for class-level inline waiver, FF-9 style)
# ---------------------------------------------------------------------------

def _find_body_end(lines: list[str], start: int) -> int | None:
    """Return 0-based index of the closing '}' for the block opening at *start*.

    Returns None when no opening brace is found before a bare ';'
    (forward declaration or a line that opened no body).
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
                return None
        if found_open:
            depth += opens - closes
            if depth <= 0:
                return idx
    return None


# ---------------------------------------------------------------------------
# Pattern builders
# ---------------------------------------------------------------------------

def _build_patterns(
    identifiers: list[str],
    includes: list[str],
) -> list[tuple[str, re.Pattern[str], str]]:
    """Return (token, compiled_pattern, match_kind) triples.

    match_kind is "word" for identifiers (word-boundary) or "sub" for substrings.
    """
    triples: list[tuple[str, re.Pattern[str], str]] = []
    for token in identifiers:
        triples.append((token, re.compile(r"\b" + re.escape(token) + r"\b"), "word"))
    for token in includes:
        triples.append((token, re.compile(re.escape(token)), "sub"))
    return triples


# Combine all platform patterns into one list keyed by category.
_ALL_PATTERNS: list[tuple[str, re.Pattern[str], str]] = (
    _build_patterns(_WIN32_IDENTIFIERS, _WIN32_INCLUDES)
    + _build_patterns(_POSIX_IDENTIFIERS, _POSIX_INCLUDES)
    + _build_patterns(_MACOS_IDENTIFIERS, _MACOS_INCLUDES)
)


# ---------------------------------------------------------------------------
# Filename-based auto-waiver
# ---------------------------------------------------------------------------

def _is_filename_exempt(path: Path) -> bool:
    """Return True when the path clearly belongs to one platform only.

    Exempt patterns (case-insensitive stem check):
      - Stem ends with _win or _windows       -> Win32 file.
      - Stem ends with _macos, _mac           -> macOS file.
      - Extension .mm                         -> Objective-C++ (macOS).
      - Stem ends with _posix or _linux       -> POSIX/Linux file.
      - Anywhere inside src/<any>/platform/win<...>/
      - Anywhere inside src/<any>/platform/mac<...>/
      - Anywhere inside src/platform/linux/ or src/<any>/platform/linux/
    """
    stem_lower = path.stem.lower()
    if (
        stem_lower.endswith("_win")
        or stem_lower.endswith("_windows")
    ):
        return True
    if (
        stem_lower.endswith("_macos")
        or stem_lower.endswith("_mac")
        or path.suffix.lower() == ".mm"
    ):
        return True
    if (
        stem_lower.endswith("_posix")
        or stem_lower.endswith("_linux")
    ):
        return True

    # Directory-level check: look at the parts of the path.
    parts = [p.lower() for p in path.parts]
    for i, part in enumerate(parts):
        if part == "platform" and i + 1 < len(parts):
            next_part = parts[i + 1]
            if next_part.startswith("win"):
                return True
            if next_part.startswith("mac"):
                return True
            if next_part == "linux":
                return True

    return False


# ---------------------------------------------------------------------------
# Guard-state tracker
# ---------------------------------------------------------------------------

class _GuardTracker:
    """Track whether the current line is inside a platform guard block.

    When inside a guard (any of Win32 / Unix / Apple) platform tokens are
    intentional and must not be flagged.

    Nesting: every #if (of any kind) increments depth; #endif decrements.
    The guard is considered active from its own #if line until its matching
    #endif, even across #elif / #else (the platform scope covers them all).
    """

    def __init__(self) -> None:
        # Stack of (is_platform_guard: bool) -- True if the corresponding
        # #if opened a known platform guard.
        self._stack: list[bool] = []

    def is_guarded(self) -> bool:
        """Return True when we are currently inside a platform guard."""
        return any(self._stack)

    def process_line(self, line: str) -> None:
        """Update state for *line* (which is a preprocessor directive)."""
        # Check for a platform-guard opener.
        if (
            _GUARD_WIN32_OPEN.search(line)
            or _GUARD_UNIX_OPEN.search(line)
            or _GUARD_APPLE_OPEN.search(line)
        ):
            self._stack.append(True)
        elif _ANY_IF_OPEN.search(line):
            self._stack.append(False)
        elif _GUARD_ENDIF.search(line):
            if self._stack:
                self._stack.pop()
        # #elif / #else: the guard status of the current frame doesn't change --
        # the frame was opened by the #if and stays until #endif.


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
    guard = _GuardTracker()
    skip_until: int = -1   # Inclusive end index of an inline-waiived block.

    idx = 0
    while idx < len(lines):
        # Skip inline-waiived block.
        if skip_until >= 0 and idx <= skip_until:
            idx += 1
            continue
        skip_until = -1

        raw    = lines[idx]
        lineno = idx + 1

        # Handle inline waiver: skip this line; if it looks like a declaration
        # header (class/function/namespace), swallow the whole body too.
        if WAIVER_MARKER in raw:
            end = _find_body_end(lines, idx)
            if end is not None:
                skip_until = end
            idx += 1
            continue

        # Handle preprocessor directives.
        stripped = raw.lstrip()
        if stripped.startswith("#"):
            # Always update guard state for conditional directives.
            guard.process_line(stripped)

            # Check #include lines for forbidden platform headers.
            # A bare #include is the primary way to pull in a platform-specific
            # header; we flag it only when not inside a guard.
            if stripped.startswith("#include") and not guard.is_guarded():
                for token, pat, _kind in _ALL_PATTERNS:
                    if pat.search(raw):
                        msg = f"{path}:{lineno}: {token}"
                        violations.append(msg)
                        if not quiet:
                            print(msg)
                        break

            idx += 1
            continue

        # Pure comment lines carry no real dependency.
        if _is_comment_line(raw):
            idx += 1
            continue

        # Inside a platform guard: platform tokens are intentional.
        if guard.is_guarded():
            idx += 1
            continue

        # Check all platform-specific patterns.
        for token, pat, _kind in _ALL_PATTERNS:
            if pat.search(raw):
                msg = f"{path}:{lineno}: {token}"
                violations.append(msg)
                if not quiet:
                    print(msg)
                break   # One report per line.

        idx += 1

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
            msg = f"check_cross_platform: path not found: {root}"
            print(msg, file=sys.stderr)
            violation_count += 1
            continue
        for p in sorted(root.rglob("*")):
            if p.suffix not in EXTENSIONS or not p.is_file():
                continue
            if _is_filename_exempt(p):
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
            "Check cross-platform portability (AD-2): scans C++ sources for "
            "Win32/POSIX/macOS-specific APIs used outside a conditional guard."
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
        f"check_cross_platform: {files_scanned} files scanned, "
        f"{violation_count} violations"
    )
    print(summary)

    return 0 if violation_count == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
