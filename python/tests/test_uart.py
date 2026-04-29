"""test_uart: UART output capture for test3.bin (UDIV 42/7=6, MUL 42*7=294)."""

from .conftest import fw


def test_uart_test3_division(board):
    """test3 computes UDIV 42/7=6 and MUL 42*7=294, prints via UART."""
    board.flash(fw("test3"))
    r = board.run(timeout_ms=200)
    assert r.exit_cause == "halt", f"exit_cause={r.exit_cause!r}"
    assert r.r[0] == 6,   f"UDIV result R0={r.r[0]} (expected 6)"
    assert r.r[1] == 294, f"MUL result R1={r.r[1]} (expected 294)"
    out = board.uart.output()
    assert isinstance(out, bytes)
    assert b"div=6" in out, f"expected 'div=6' in UART output, got {out!r}"
    assert b"mul=294" in out, f"expected 'mul=294' in UART output, got {out!r}"
    # drain is idempotent: second call returns empty
    assert board.uart.output() == b"", "second drain must be empty"
