"""test_blink: GPIO + register assertions for test10_stm32_blink.bin.

Demonstrates the three assertion classes a typical user writes:
  1. result.exit_cause  — firmware halted cleanly
  2. result.r[0]        — loop counter reached expected value
  3. board.gpio['GPIOC'][13].value — LED pin is a readable bool
"""

import pytest
from .conftest import fw


@pytest.fixture
def blink_board():
    """stm32f103 board with blink firmware pre-flashed."""
    from lecerf import Board
    b = Board("stm32f103")
    b.flash(fw("test10_stm32_blink"))
    yield b
    del b


def test_blink_halts(blink_board):
    r = blink_board.run(timeout_ms=500)
    assert r.exit_cause == "halt", f"exit_cause={r.exit_cause!r}"


def test_blink_r0_counter(blink_board):
    """test10 increments R0 to 5 in its blink loop."""
    r = blink_board.run(timeout_ms=500)
    assert r.r[0] == 5, f"R0={r.r[0]:#x} (expected 0x5)"


def test_blink_gpio_pin(blink_board):
    """GPIOC pin 13 must be readable as a bool after firmware runs."""
    blink_board.run(timeout_ms=500)
    v = blink_board.gpio["GPIOC"][13].value
    assert isinstance(v, bool), f"expected bool, got {type(v)}"
