#!/usr/bin/env python3
"""Unit tests for check_naming_convention.py -- eight cases per plan_29.

Cases:
  1. I-prefix + non-virtual method body           -> exit 1 (Rule 1 fail)
  2. I-prefix + data member                       -> exit 1 (Rule 1 fail)
  3. Abstract-prefix + state (data member)        -> exit 0 (Rule 2 pass)
  4. Abstract-prefix + all pure-virtual, no state -> exit 1 (Rule 2 fail)
  5. Waiver token on class line                   -> exit 0 (waiver respected)
  6. Forward declaration (no body)                -> exit 0 (skipped)
  7. Nested class inside a clean outer class      -> exit 0 (nested skipped)
  8. Missing scan path handled gracefully         -> exit 1, message on stderr
"""

import sys
import textwrap
from pathlib import Path

import pytest

# Add script/ to path so the module can be imported directly.
# The directory was renamed from `scripts/` to `script/` during the
# post-shipment cleanup — keep this path in sync.
sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent / "script"))

import check_naming_convention as cnc  # noqa: E402


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def write_header(tmp_dir: Path, filename: str, content: str) -> Path:
    p = tmp_dir / filename
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(textwrap.dedent(content), encoding="utf-8")
    return p


def run(argv: list[str]) -> int:
    return cnc.main(argv)


# ---------------------------------------------------------------------------
# Test 1 -- I-prefix + non-virtual method body (Rule 1 fail)
# ---------------------------------------------------------------------------

def test_i_prefix_non_virtual_body_fails(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    """A class named IFoo that has a non-virtual method with a body must be flagged."""
    d = tmp_path / "include" / "vigine"
    write_header(
        d,
        "bad_i.h",
        """\
        #pragma once
        namespace test {
        class IMyMixin {
          public:
            virtual void pure() = 0;
            void helper() { /* non-virtual body */ }
        };
        }
        """,
    )
    code = run(["--path", str(d)])
    captured = capsys.readouterr()
    assert code == 1, "Expected exit 1 for I-prefix with non-virtual body"
    assert "IMyMixin" in captured.out
    assert "non-virtual" in captured.out


# ---------------------------------------------------------------------------
# Test 2 -- I-prefix + data member (Rule 1 fail)
# ---------------------------------------------------------------------------

def test_i_prefix_data_member_fails(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    """A class named IFoo that carries a data member must be flagged."""
    d = tmp_path / "include" / "vigine"
    write_header(
        d,
        "bad_data.h",
        """\
        #pragma once
        namespace test {
        class IStateful {
          public:
            virtual void work() = 0;
          private:
            int _value{0};
        };
        }
        """,
    )
    code = run(["--path", str(d)])
    captured = capsys.readouterr()
    assert code == 1, "Expected exit 1 for I-prefix with data member"
    assert "IStateful" in captured.out
    assert "data member" in captured.out


# ---------------------------------------------------------------------------
# Test 3 -- Abstract-prefix + state (data member) passes (Rule 2 pass)
# ---------------------------------------------------------------------------

def test_abstract_prefix_with_state_passes(tmp_path: Path) -> None:
    """A class named AbstractFoo with a data member is a valid Abstract class."""
    d = tmp_path / "include" / "vigine"
    write_header(
        d,
        "good_abstract.h",
        """\
        #pragma once
        namespace test {
        class AbstractBase {
          public:
            virtual void work() = 0;
            virtual ~AbstractBase() = default;
          protected:
            AbstractBase() = default;
          private:
            int _id{0};
        };
        }
        """,
    )
    code = run(["--path", str(d), "--quiet"])
    assert code == 0, "Expected exit 0 for Abstract-prefix with data member"


# ---------------------------------------------------------------------------
# Test 4 -- Abstract-prefix + all pure-virtual, no state (Rule 2 fail)
# ---------------------------------------------------------------------------

def test_abstract_prefix_all_pure_fails(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    """A class named AbstractFoo where all methods are pure-virtual and there is
    no data member should be flagged: it belongs to the I tier."""
    d = tmp_path / "include" / "vigine"
    write_header(
        d,
        "bad_abstract.h",
        """\
        #pragma once
        namespace test {
        class AbstractPure {
          public:
            virtual ~AbstractPure() = delete;
            virtual void doA() = 0;
            virtual void doB() = 0;
        };
        }
        """,
    )
    code = run(["--path", str(d)])
    captured = capsys.readouterr()
    assert code == 1, "Expected exit 1 for Abstract-prefix that is fully pure-virtual"
    assert "AbstractPure" in captured.out
    assert "pure-virtual" in captured.out


# ---------------------------------------------------------------------------
# Test 5 -- Waiver token respected
# ---------------------------------------------------------------------------

def test_waiver_respected(tmp_path: Path) -> None:
    """A class with the waiver token on its declaration line is not reported."""
    d = tmp_path / "include" / "vigine"
    write_header(
        d,
        "waived.h",
        """\
        #pragma once
        namespace test {
        class IHasState // INV-10 EXEMPTION: predates convention
        {
          public:
            virtual void work() = 0;
          private:
            int _field{0};
        };
        }
        """,
    )
    code = run(["--path", str(d), "--quiet"])
    assert code == 0, "Expected exit 0 when waiver token is present"


# ---------------------------------------------------------------------------
# Test 6 -- Forward declarations are skipped
# ---------------------------------------------------------------------------

def test_forward_declaration_skipped(tmp_path: Path) -> None:
    """A forward declaration 'class IFoo;' without a body must not be flagged."""
    d = tmp_path / "include" / "vigine"
    write_header(
        d,
        "forward.h",
        """\
        #pragma once
        namespace test {
        class IForward;
        class AbstractForward;
        }
        """,
    )
    code = run(["--path", str(d), "--quiet"])
    assert code == 0, "Expected exit 0 for forward declarations (no body)"


# ---------------------------------------------------------------------------
# Test 7 -- Nested classes are not re-checked as top-level
# ---------------------------------------------------------------------------

def test_nested_classes_not_rechecked(tmp_path: Path) -> None:
    """A nested inner class inside a valid outer class body must not cause
    a false positive on the outer class."""
    d = tmp_path / "include" / "vigine"
    write_header(
        d,
        "nested.h",
        """\
        #pragma once
        namespace test {
        class AbstractOuter {
          public:
            virtual void work() = 0;
            // Inner class: would fail if checked at top level (no state),
            // but it must be skipped because it is nested.
            class InnerHelper {
              public:
                void doThing() {}
            };
          protected:
            AbstractOuter() = default;
            virtual ~AbstractOuter() = default;
          private:
            int _id{0};
        };
        }
        """,
    )
    # AbstractOuter has _id (data member) -- should pass Rule 2.
    code = run(["--path", str(d), "--quiet"])
    assert code == 0, "Expected exit 0 -- nested classes must not cause false positives"


# ---------------------------------------------------------------------------
# Test 8 -- Missing path handled gracefully (exit 1, message on stderr)
# ---------------------------------------------------------------------------

def test_missing_path_graceful(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    """A non-existent --path exits 1 with a clear message (no crash)."""
    nonexistent = tmp_path / "does_not_exist"
    code = run(["--path", str(nonexistent)])
    captured = capsys.readouterr()
    assert code == 1, "Expected exit 1 for missing scan path"
    assert "not found" in captured.err.lower() or "not found" in captured.out.lower()


# ---------------------------------------------------------------------------
# Test 9 -- Waiver covers nested class/struct declarations inside the body
# ---------------------------------------------------------------------------

def test_waiver_covers_nested_types(tmp_path: Path) -> None:
    """A nested I-prefix class inside a waivered parent must also be
    waivered. Previously the waiver only skipped the opening line, which
    let nested declarations trigger spurious Rule 1 violations at the
    outer scope."""
    d = tmp_path / "include" / "vigine"
    write_header(
        d,
        "waived_nested.h",
        """\
        #pragma once
        namespace test {
        class IHasState // INV-10 EXEMPTION: legacy ABI
        {
          public:
            // This nested interface would trip Rule 1 if re-checked at
            // the outer scope (non-virtual body inside an I-prefix
            // class), but the waiver on the parent must cover it too.
            class INestedBad {
              public:
                void helper() { /* non-virtual body */ }
                int  _field{0};
            };

            virtual void work() = 0;
          private:
            int _field{0};
        };
        }
        """,
    )
    code = run(["--path", str(d), "--quiet"])
    assert code == 0, "Expected exit 0 when waiver covers nested types"


# ---------------------------------------------------------------------------
# Test 10 -- Rule 1 does not misfire on non-method lines that contain '('
# ---------------------------------------------------------------------------

def test_rule_1_ignores_non_method_lines(tmp_path: Path) -> None:
    """Lines that carry `(` but are not member functions (type aliases,
    friend declarations, static-assert, typedef function-pointers) must
    not count as non-virtual methods for Rule 1 purposes."""
    d = tmp_path / "include" / "vigine"
    write_header(
        d,
        "ok_non_method_parens.h",
        """\
        #pragma once
        #include <functional>
        namespace test {
        class IWithAliases {
          public:
            // Type alias that mentions `(` inside the function-type
            // signature.
            using Callback = std::function<void(int)>;

            // Classical C-style typedef for a function pointer.
            typedef void (*RawCallback)(int);

            // Forward-friend declaration of a free function.
            friend bool operator==(const IWithAliases &, const IWithAliases &);

            // Compile-time check that is not a member function.
            static_assert(sizeof(int) == 4, "int must be 32 bits");

            virtual void doWork() = 0;

          protected:
            IWithAliases()                                 = default;
            virtual ~IWithAliases()                        = default;
            IWithAliases(const IWithAliases &)             = delete;
            IWithAliases &operator=(const IWithAliases &) = delete;
        };
        }
        """,
    )
    code = run(["--path", str(d), "--quiet"])
    assert code == 0, (
        "Expected exit 0 — using/typedef/friend/static_assert lines "
        "that happen to contain `(` must not trigger Rule 1"
    )
