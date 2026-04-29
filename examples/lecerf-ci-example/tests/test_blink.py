"""STM32F103 GPIO blink: toggles a GPIO pin five times, then halts with R0 == 5.

This test overrides the default `board` fixture from the lecerf pytest plugin
to lock the profile to stm32f103 (the firmware was linked against its memory
map and peripheral layout).
"""

import pytest
from lecerf import Board

from conftest import fwpath


@pytest.fixture
def board():
    """Override the plugin's `board` fixture to force the stm32f103 profile."""
    b = Board("stm32f103")
    yield b
    del b


def test_blink_completes(board):
    r = board.flash(fwpath("test10_stm32_blink")).run(timeout_ms=500)
    assert r.exit_cause == "halt", f"expected halt, got {r.exit_cause}"
    assert r.r[0] == 5, f"expected R0=5 (blink count), got R0={r.r[0]}"
