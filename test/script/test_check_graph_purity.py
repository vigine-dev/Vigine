#!/usr/bin/env python3
"""Unit tests for check_graph_purity.py.

Five test cases per plan_27:
  1. No violation -- clean directory exits 0, reports 0 violations.
  2. One violation -- a single forbidden include triggers a report.
  3. Multiple violations -- several hits in one file are all reported.
  4. Waiver respected -- lines with // INV-9 EXEMPTION: are skipped.
  5. Path missing handled gracefully -- non-existent path exits 1 with a message.
"""

import io
import sys
import tempfile
import textwrap
from pathlib import Path

import pytest

# Add the scripts directory to sys.path so the module can be imported directly.
sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent / "scripts"))

import check_graph_purity as cgp  # noqa: E402


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def write_header(tmp_dir: Path, filename: str, content: str) -> Path:
    """Write a .h file and return its Path."""
    p = tmp_dir / filename
    p.write_text(textwrap.dedent(content), encoding="utf-8")
    return p


def run(argv: list[str]) -> int:
    """Run cgp.main() with the given argv; return exit code."""
    return cgp.main(argv)


# ---------------------------------------------------------------------------
# Test 1 -- no violations
# ---------------------------------------------------------------------------


def test_no_violation(tmp_path: Path) -> None:
    """A directory with only clean headers exits 0 and reports 0 violations."""
    include_dir = tmp_path / "include" / "vigine" / "graph"
    include_dir.mkdir(parents=True)
    src_dir = tmp_path / "src" / "graph"
    src_dir.mkdir(parents=True)
    write_header(
        include_dir,
        "inode.h",
        """\
        #pragma once
        #include <cstdint>
        namespace vigine::graph { class INode {}; }
        """,
    )
    code = run(["--root", str(tmp_path), "--quiet"])
    assert code == 0, "Expected exit 0 for clean headers"


# ---------------------------------------------------------------------------
# Test 2 -- one violation (forbidden include)
# ---------------------------------------------------------------------------


def test_one_violation_forbidden_include(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    """A single forbidden include in a header triggers exit 1."""
    graph_dir = tmp_path / "include" / "vigine" / "graph"
    graph_dir.mkdir(parents=True)
    write_header(
        graph_dir,
        "bad.h",
        """\
        #pragma once
        #include <vigine/messaging/kind.h>
        namespace vigine::graph {}
        """,
    )
    code = run(["--root", str(tmp_path)])
    captured = capsys.readouterr()
    assert code == 1, "Expected exit 1 for forbidden include"
    assert "bad.h" in captured.out
    assert "vigine/messaging/" in captured.out


# ---------------------------------------------------------------------------
# Test 3 -- multiple violations
# ---------------------------------------------------------------------------


def test_multiple_violations(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    """Multiple forbidden tokens in one file are all reported, exit 1."""
    graph_dir = tmp_path / "include" / "vigine" / "graph"
    graph_dir.mkdir(parents=True)
    write_header(
        graph_dir,
        "multi.h",
        """\
        #pragma once
        #include <vigine/ecs/entity.h>
        #include <vigine/fsm/state.h>
        namespace vigine::graph {}
        """,
    )
    code = run(["--root", str(tmp_path)])
    captured = capsys.readouterr()
    assert code == 1
    # Both lines should appear.
    lines_with_path = [l for l in captured.out.splitlines() if "multi.h" in l]
    assert len(lines_with_path) >= 2, f"Expected >=2 violation lines, got: {captured.out}"


# ---------------------------------------------------------------------------
# Test 4 -- waiver respected
# ---------------------------------------------------------------------------


def test_waiver_respected(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    """Lines containing // INV-9 EXEMPTION: are not reported as violations."""
    graph_dir = tmp_path / "include" / "vigine" / "graph"
    graph_dir.mkdir(parents=True)
    src_dir = tmp_path / "src" / "graph"
    src_dir.mkdir(parents=True)
    write_header(
        graph_dir,
        "waived.h",
        """\
        #pragma once
        #include <vigine/messaging/bus.h>  // INV-9 EXEMPTION: justified by design doc
        namespace vigine::graph {}
        """,
    )
    code = run(["--root", str(tmp_path), "--quiet"])
    assert code == 0, "Expected exit 0 when the only hit is waiived"
    captured = capsys.readouterr()
    assert "0 violations" in captured.out


# ---------------------------------------------------------------------------
# Test 5 -- missing path handled gracefully
# ---------------------------------------------------------------------------


def test_missing_path_graceful(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    """A non-existent --path exits 1 with a clear error message (no crash)."""
    nonexistent = tmp_path / "does_not_exist"
    code = run(["--root", str(tmp_path), "--path", str(nonexistent)])
    captured = capsys.readouterr()
    assert code == 1, "Expected exit 1 for missing scan path"
    assert "not found" in captured.err.lower() or "not found" in captured.out.lower()
