#!/usr/bin/env python3
"""Unit tests for check_strict_encapsulation.py.

Cases:
  1. Class with public data member                          -> exit 1
  2. Class with protected data member                       -> exit 1
  3. Class with only private data members                   -> exit 0
  4. Struct with public data (aggregate intent)             -> exit 0
  5. Struct with protected data member                      -> exit 1
  6. Waiver token on class declaration skips whole body     -> exit 0
  7. POD-id filename exempt (e.g. nodeid.h)                 -> exit 0
  8. kind.h filename exempt                                 -> exit 0
  9. Forward declaration (no body) not flagged              -> exit 0
 10. Missing scan path handled gracefully (exit 1, stderr)  -> exit 1
"""

import sys
import textwrap
from pathlib import Path

import pytest

# Add script/ to path so the module can be imported directly.
sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent / "scripts"))

import check_strict_encapsulation as cse  # noqa: E402


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def write_header(tmp_dir: Path, filename: str, content: str) -> Path:
    p = tmp_dir / filename
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(textwrap.dedent(content), encoding="utf-8")
    return p


def run(argv: list[str]) -> int:
    return cse.main(argv)


# ---------------------------------------------------------------------------
# Test 1 -- class with public data member fails
# ---------------------------------------------------------------------------

def test_class_public_data_member_fails(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    """A class that exposes a public data member must be flagged."""
    d = tmp_path / "include" / "vigine"
    write_header(
        d,
        "bad_public.h",
        """\
        #pragma once
        namespace test {
        class Foo {
          public:
            int value;
            void doSomething();
        };
        }
        """,
    )
    code = run(["--path", str(d)])
    captured = capsys.readouterr()
    assert code == 1, "Expected exit 1 for class with public data member"
    assert "public data member in class" in captured.out


# ---------------------------------------------------------------------------
# Test 2 -- class with protected data member fails
# ---------------------------------------------------------------------------

def test_class_protected_data_member_fails(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    """A class with a protected data member must be flagged."""
    d = tmp_path / "include" / "vigine"
    write_header(
        d,
        "bad_protected.h",
        """\
        #pragma once
        namespace test {
        class Bar {
          public:
            void doSomething();
          protected:
            int _count;
        };
        }
        """,
    )
    code = run(["--path", str(d)])
    captured = capsys.readouterr()
    assert code == 1, "Expected exit 1 for class with protected data member"
    assert "protected data member in class" in captured.out


# ---------------------------------------------------------------------------
# Test 3 -- class with only private data members passes
# ---------------------------------------------------------------------------

def test_class_private_data_member_passes(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    """A class that keeps all data members private must pass."""
    d = tmp_path / "include" / "vigine"
    write_header(
        d,
        "good_private.h",
        """\
        #pragma once
        namespace test {
        class Baz {
          public:
            void doSomething();
            int getValue() const { return _value; }
          private:
            int _value;
            bool _flag;
        };
        }
        """,
    )
    code = run(["--path", str(d)])
    assert code == 0, "Expected exit 0 for class with all-private data members"


# ---------------------------------------------------------------------------
# Test 4 -- struct with public data (aggregate) passes
# ---------------------------------------------------------------------------

def test_struct_public_data_passes(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    """A plain struct with public data members (aggregate pattern) must pass."""
    d = tmp_path / "include" / "vigine"
    write_header(
        d,
        "good_struct.h",
        """\
        #pragma once
        namespace test {
        struct Config {
            int width;
            int height;
            bool fullscreen{false};
        };
        }
        """,
    )
    code = run(["--path", str(d)])
    assert code == 0, "Expected exit 0 for aggregate struct with public data"


# ---------------------------------------------------------------------------
# Test 5 -- struct with protected data member fails
# ---------------------------------------------------------------------------

def test_struct_protected_data_fails(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    """A struct with a protected data member (unusual, suspect) must be flagged."""
    d = tmp_path / "include" / "vigine"
    write_header(
        d,
        "bad_struct_protected.h",
        """\
        #pragma once
        namespace test {
        struct Odd {
          protected:
            int _secret;
        };
        }
        """,
    )
    code = run(["--path", str(d)])
    captured = capsys.readouterr()
    assert code == 1, "Expected exit 1 for struct with protected data member"
    assert "protected data member in struct" in captured.out


# ---------------------------------------------------------------------------
# Test 6 -- waiver token skips class body (including nested types)
# ---------------------------------------------------------------------------

def test_waiver_token_skips_class(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    """A class with the waiver marker on its declaration line must be skipped."""
    d = tmp_path / "include" / "vigine"
    write_header(
        d,
        "waiived.h",
        """\
        #pragma once
        namespace test {
        class LegacyFoo // ENCAP EXEMPTION: pending cleanup
        {
          public:
            int exposed;
          protected:
            int _state;
        };
        }
        """,
    )
    code = run(["--path", str(d)])
    assert code == 0, "Expected exit 0 when waiver marker is present"


# ---------------------------------------------------------------------------
# Test 7 -- POD-id filename exempt
# ---------------------------------------------------------------------------

def test_pod_id_filename_exempt(tmp_path: Path) -> None:
    """A file named nodeid.h (or any id-header) must be skipped entirely."""
    d = tmp_path / "include" / "vigine" / "graph"
    write_header(
        d,
        "nodeid.h",
        """\
        #pragma once
        namespace vigine {
        struct NodeId {
            unsigned int value{0};
        };
        }
        """,
    )
    code = run(["--path", str(tmp_path / "include" / "vigine")])
    assert code == 0, "Expected exit 0 for nodeid.h (exempt POD-id filename)"


# ---------------------------------------------------------------------------
# Test 8 -- kind.h filename exempt
# ---------------------------------------------------------------------------

def test_kind_filename_exempt(tmp_path: Path) -> None:
    """A file whose name ends with 'kind.h' must be skipped entirely."""
    d = tmp_path / "include" / "vigine" / "graph"
    write_header(
        d,
        "kind.h",
        """\
        #pragma once
        namespace vigine {
        enum class EdgeKind { Forward, Backward };
        struct SomeKind {
            int rawKind;
        };
        }
        """,
    )
    code = run(["--path", str(tmp_path / "include" / "vigine")])
    assert code == 0, "Expected exit 0 for kind.h (exempt kind filename)"


# ---------------------------------------------------------------------------
# Test 9 -- forward declaration not flagged
# ---------------------------------------------------------------------------

def test_forward_declaration_not_flagged(tmp_path: Path) -> None:
    """A class forward declaration (no body) must not trigger any violation."""
    d = tmp_path / "include" / "vigine"
    write_header(
        d,
        "fwd.h",
        """\
        #pragma once
        namespace test {
        class SomeClass;
        class AnotherClass;
        }
        """,
    )
    code = run(["--path", str(d)])
    assert code == 0, "Expected exit 0 for forward declarations only"


# ---------------------------------------------------------------------------
# Test 10 -- missing scan path is an error
# ---------------------------------------------------------------------------

def test_missing_path_is_error(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    """Passing a path that does not exist must cause exit 1 with a message."""
    nonexistent = str(tmp_path / "does" / "not" / "exist")
    code = run(["--path", nonexistent])
    captured = capsys.readouterr()
    assert code == 1, "Expected exit 1 when scan path does not exist"
    assert "path not found" in captured.err
