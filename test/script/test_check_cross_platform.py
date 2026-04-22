#!/usr/bin/env python3
"""Unit tests for check_cross_platform.py.

Cases:
  1. Clean file with no platform tokens          -> exit 0
  2. #include <windows.h> outside a guard        -> exit 1, windows.h reported
  3. Filename-based auto-waiver (*_win.cpp)       -> exit 0 (file skipped)
  4. Inline // CROSS-PLATFORM EXEMPTION: waiver  -> exit 0 (class body skipped)
  5. #include <windows.h> inside #if defined(_WIN32) guard -> exit 0
  6. POSIX token (pid_t) outside a guard         -> exit 1
  7. Missing scan path handled gracefully        -> exit 1, stderr message
"""

import sys
import textwrap
from pathlib import Path

import pytest

# Add script/ to sys.path so the module can be imported directly.
sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent / "script"))

import check_cross_platform as ccp  # noqa: E402


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def write_source(tmp_dir: Path, filename: str, content: str) -> Path:
    p = tmp_dir / filename
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(textwrap.dedent(content), encoding="utf-8")
    return p


def run(argv: list[str]) -> int:
    return ccp.main(argv)


# ---------------------------------------------------------------------------
# Test 1 -- clean file with no platform tokens passes
# ---------------------------------------------------------------------------

def test_clean_file_passes(tmp_path: Path) -> None:
    """A file with only standard C++ must exit 0."""
    d = tmp_path / "src" / "graph"
    write_source(
        d,
        "mygraph.cpp",
        """\
        #include <vector>
        #include <string>

        namespace vigine {
        class MyGraph {
          public:
            void addNode(int id) { _nodes.push_back(id); }
          private:
            std::vector<int> _nodes;
        };
        }
        """,
    )
    code = run(["--path", str(d)])
    assert code == 0, "Expected exit 0 for clean file"


# ---------------------------------------------------------------------------
# Test 2 -- #include <windows.h> outside a guard fails
# ---------------------------------------------------------------------------

def test_windows_include_outside_guard_fails(
    tmp_path: Path, capsys: pytest.CaptureFixture
) -> None:
    """An unguarded #include <windows.h> must be flagged as a violation."""
    d = tmp_path / "src"
    write_source(
        d,
        "platform_bad.cpp",
        """\
        #include <windows.h>
        #include <vector>

        namespace vigine {
        void doSomething() {}
        }
        """,
    )
    code = run(["--path", str(d)])
    captured = capsys.readouterr()
    assert code == 1, "Expected exit 1 for unguarded <windows.h>"
    assert "<windows.h>" in captured.out


# ---------------------------------------------------------------------------
# Test 3 -- filename-based auto-waiver skips the file entirely
# ---------------------------------------------------------------------------

def test_filename_based_waiver_skips_file(tmp_path: Path) -> None:
    """A file named *_win.cpp must be exempt from all checks."""
    d = tmp_path / "src"
    write_source(
        d,
        "signal_win.cpp",
        """\
        #include <windows.h>

        namespace vigine {
        HANDLE hEvent = CreateFileW(L"test", 0, 0, nullptr, 0, 0, nullptr);
        }
        """,
    )
    code = run(["--path", str(d)])
    assert code == 0, "Expected exit 0 for *_win.cpp (filename-based auto-waiver)"


# ---------------------------------------------------------------------------
# Test 4 -- inline CROSS-PLATFORM EXEMPTION: waiver skips class body
# ---------------------------------------------------------------------------

def test_inline_waiver_skips_class_body(tmp_path: Path) -> None:
    """The // CROSS-PLATFORM EXEMPTION: marker on a class declaration must
    cause the entire class body to be skipped, including any Win32 tokens."""
    d = tmp_path / "src"
    write_source(
        d,
        "legacy.cpp",
        """\
        #include <vector>

        namespace vigine {

        // CROSS-PLATFORM EXEMPTION: Windows legacy code, migration tracked in #999
        class WinLegacy {
          public:
            void init() {
                HANDLE h = CreateFileW(L"x", 0, 0, nullptr, 0, 0, nullptr);
            }
        };

        class CleanClass {
          public:
            void doWork() {}
        };

        }
        """,
    )
    code = run(["--path", str(d)])
    assert code == 0, "Expected exit 0 when inline waiver covers the class body"


# ---------------------------------------------------------------------------
# Test 5 -- Win32 include inside a _WIN32 guard is clean
# ---------------------------------------------------------------------------

def test_windows_include_inside_guard_passes(tmp_path: Path) -> None:
    """A #include <windows.h> inside #if defined(_WIN32) must not be flagged."""
    d = tmp_path / "src"
    write_source(
        d,
        "platform_guarded.cpp",
        """\
        #if defined(_WIN32)
        #include <windows.h>

        namespace vigine {
        HANDLE openFile(const wchar_t *path) {
            return CreateFileW(path, GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        }
        }

        #endif  // _WIN32
        """,
    )
    code = run(["--path", str(d)])
    assert code == 0, "Expected exit 0 for Win32 tokens inside #if defined(_WIN32)"


# ---------------------------------------------------------------------------
# Test 6 -- POSIX token outside a guard fails
# ---------------------------------------------------------------------------

def test_posix_token_outside_guard_fails(
    tmp_path: Path, capsys: pytest.CaptureFixture
) -> None:
    """A pid_t declaration outside any platform guard must be flagged."""
    d = tmp_path / "src"
    write_source(
        d,
        "process.cpp",
        """\
        #include <cstdlib>

        namespace vigine {
        pid_t spawnWorker() {
            return 0;
        }
        }
        """,
    )
    code = run(["--path", str(d)])
    captured = capsys.readouterr()
    assert code == 1, "Expected exit 1 for unguarded pid_t"
    assert "pid_t" in captured.out


# ---------------------------------------------------------------------------
# Test 7 -- missing scan path is an error
# ---------------------------------------------------------------------------

def test_missing_path_is_error(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    """Passing a path that does not exist must cause exit 1 with a stderr message."""
    nonexistent = str(tmp_path / "does" / "not" / "exist")
    code = run(["--path", nonexistent])
    captured = capsys.readouterr()
    assert code == 1, "Expected exit 1 when scan path does not exist"
    assert "path not found" in captured.err
