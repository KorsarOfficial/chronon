"""UART + UDIV/MUL firmware: prints 'div=6 mul=294\\n' over UART then halts.

Exercises three things at once:
  - integer division (UDIV)  -> R0 = 42 / 7 = 6
  - integer multiply (MUL)   -> R1 = 42 * 7 = 294
  - memory-mapped UART_DR sink at 0x40004000 capturing bytes for inspection
"""

from conftest import fwpath


def test_uart_division_result(board):
    r = board.flash(fwpath("test3")).run(timeout_ms=200)
    assert r.exit_cause == "halt", f"expected halt, got {r.exit_cause}"
    assert r.r[0] == 6, f"expected R0=6 (42/7), got R0={r.r[0]}"
    assert r.r[1] == 294, f"expected R1=294 (42*7), got R1={r.r[1]}"

    out = board.uart.output()
    assert isinstance(out, (bytes, bytearray)), f"uart.output() returned {type(out)}"
    # The firmware prints 'div=6 mul=294\n'; bytes object should contain '6' and '294'.
    assert b"6" in out
    assert b"294" in out
