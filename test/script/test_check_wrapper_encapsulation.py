#!/usr/bin/env python3
"""Unit tests for check_wrapper_encapsulation.py.

Six test cases per plan_28:

  1. Wrapper mode clean   -- a directory with no forbidden tokens exits 0.
  2. Wrapper mode violation -- NodeId in a service header triggers exit 1.
  3. Waiver respected      -- // INV-11 EXEMPTION: on a violating line is skipped.
  4. Facade mode clean     -- a directory with no forbidden facade tokens exits 0.
  5. Facade mode violation -- IBusControlBlock in a facade header triggers exit 1.
  6. Facade waiver         -- facade token covered by // INV-11 EXEMPTION: is skipped.

Each test uses a temporary directory so tests are fully isolated and
leave no state behind.
"""

import sys
import textwrap
from pathlib import Path

import pytest

# Make the script/ directory importable without installing the package.
sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent / "scripts"))

import check_wrapper_encapsulation as cwe  # noqa: E402


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _write(tmp: Path, rel: str, content: str) -> Path:
    """Create a file at *tmp/rel* with the given (dedented) content."""
    p = tmp / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(textwrap.dedent(content), encoding="utf-8")
    return p


def _run(argv: list[str]) -> int:
    """Invoke cwe.main() and return the exit code."""
    return cwe.main(argv)


# ---------------------------------------------------------------------------
# Test 1: wrapper mode -- clean directory
# ---------------------------------------------------------------------------


def test_wrapper_clean(tmp_path: Path) -> None:
    """A service header with no graph types exits 0 (wrapper mode default)."""
    svc_dir = tmp_path / "include" / "vigine" / "service"
    svc_dir.mkdir(parents=True)
    _write(
        svc_dir,
        "iwidget.h",
        """\
        #pragma once
        #include <memory>
        #include "vigine/service/serviceid.h"

        namespace vigine::service {
        class IWidget {
        public:
            virtual ~IWidget() = default;
            virtual int value() const noexcept = 0;
        };
        } // namespace vigine::service
        """,
    )
    assert _run(["--path", str(svc_dir), "--quiet"]) == 0


# ---------------------------------------------------------------------------
# Test 2: wrapper mode -- violation (NodeId)
# ---------------------------------------------------------------------------


def test_wrapper_violation_node_id(
    tmp_path: Path, capsys: pytest.CaptureFixture
) -> None:
    """NodeId in a wrapper header triggers exit 1 with a path:line diagnostic."""
    svc_dir = tmp_path / "include" / "vigine" / "service"
    svc_dir.mkdir(parents=True)
    _write(
        svc_dir,
        "ibadsurface.h",
        """\
        #pragma once
        #include "vigine/service/serviceid.h"

        namespace vigine::service {
        class IBadSurface {
        public:
            virtual ~IBadSurface() = default;
            virtual NodeId nodeRef() const = 0;
        };
        } // namespace vigine::service
        """,
    )
    code = _run(["--path", str(svc_dir)])
    out  = capsys.readouterr().out
    assert code == 1, "Expected exit 1 for NodeId in wrapper header"
    assert "ibadsurface.h" in out
    assert "NodeId" in out


# ---------------------------------------------------------------------------
# Test 3: waiver respected (wrapper mode)
# ---------------------------------------------------------------------------


def test_wrapper_waiver_respected(
    tmp_path: Path, capsys: pytest.CaptureFixture
) -> None:
    """A line containing // INV-11 EXEMPTION: is not reported as a violation."""
    svc_dir = tmp_path / "include" / "vigine" / "service"
    svc_dir.mkdir(parents=True)
    _write(
        svc_dir,
        "iwaivedservice.h",
        """\
        #pragma once
        #include "vigine/service/serviceid.h"

        namespace vigine::service {
        class IWaivedService {
        public:
            virtual ~IWaivedService() = default;
            virtual NodeId nodeRef() const = 0;  // INV-11 EXEMPTION: legacy cross-layer pinning, approved by arch
        };
        } // namespace vigine::service
        """,
    )
    code = _run(["--path", str(svc_dir), "--quiet"])
    out  = capsys.readouterr().out
    assert code == 0, "Expected exit 0 when the only violation carries a waiver"
    assert "0 violations" in out


# ---------------------------------------------------------------------------
# Test 4: facade mode -- clean directory
# ---------------------------------------------------------------------------


def test_facade_clean(tmp_path: Path) -> None:
    """A facade header with no forbidden tokens exits 0 in facade mode."""
    facade_dir = tmp_path / "include" / "vigine" / "signalemitter"
    facade_dir.mkdir(parents=True)
    _write(
        facade_dir,
        "isignalemitter.h",
        """\
        #pragma once
        #include <memory>

        namespace vigine::signalemitter {
        class ISignalEmitter {
        public:
            virtual ~ISignalEmitter() = default;
            virtual void emit(int signal) = 0;
        };
        } // namespace vigine::signalemitter
        """,
    )
    assert _run(["--path", str(facade_dir), "--mode", "facade", "--quiet"]) == 0


# ---------------------------------------------------------------------------
# Test 5: facade mode -- violation (IBusControlBlock)
# ---------------------------------------------------------------------------


def test_facade_violation_bus_control_block(
    tmp_path: Path, capsys: pytest.CaptureFixture
) -> None:
    """IBusControlBlock in a facade header triggers exit 1 in facade mode."""
    facade_dir = tmp_path / "include" / "vigine" / "connector"
    facade_dir.mkdir(parents=True)
    _write(
        facade_dir,
        "iconnector.h",
        """\
        #pragma once
        #include <memory>

        namespace vigine::connector {
        class IConnector {
        public:
            virtual ~IConnector() = default;
            virtual void attach(IBusControlBlock &bus) = 0;
        };
        } // namespace vigine::connector
        """,
    )
    code = _run(["--path", str(facade_dir), "--mode", "facade"])
    out  = capsys.readouterr().out
    assert code == 1, "Expected exit 1 for IBusControlBlock in facade header"
    assert "iconnector.h" in out
    assert "IBusControlBlock" in out


# ---------------------------------------------------------------------------
# Test 6: facade mode -- waiver respected
# ---------------------------------------------------------------------------


def test_facade_waiver_respected(
    tmp_path: Path, capsys: pytest.CaptureFixture
) -> None:
    """A facade token covered by // INV-11 EXEMPTION: is skipped in facade mode."""
    facade_dir = tmp_path / "include" / "vigine" / "hub"
    facade_dir.mkdir(parents=True)
    _write(
        facade_dir,
        "ihub.h",
        """\
        #pragma once
        #include <memory>

        namespace vigine::hub {
        class IHub {
        public:
            virtual ~IHub() = default;
            virtual void attach(IBusControlBlock &bus) = 0;  // INV-11 EXEMPTION: hub is the integration seam
        };
        } // namespace vigine::hub
        """,
    )
    code = _run(["--path", str(facade_dir), "--mode", "facade", "--quiet"])
    out  = capsys.readouterr().out
    assert code == 0, "Expected exit 0 when the only violation carries a facade waiver"
    assert "0 violations" in out
