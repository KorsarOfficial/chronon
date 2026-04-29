"""Shared helpers for the lecerf-ci-example test suite.

The `board` fixture itself is provided by the lecerf pytest plugin (auto-loaded
via the pytest11 entry point once `pip install lecerf` succeeds).
"""

import os

FW = os.path.join(os.path.dirname(__file__), "..", "firmware")


def fwpath(name: str) -> str:
    """Resolve a firmware basename (no extension) to an absolute .bin path."""
    return os.path.abspath(os.path.join(FW, name + ".bin"))
