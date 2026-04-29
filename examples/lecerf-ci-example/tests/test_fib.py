"""fib(10) firmware: computes the 10th Fibonacci number and halts with R0 == 55."""

from conftest import fwpath


def test_fib_10_equals_55(board):
    r = board.flash(fwpath("test1")).run(timeout_ms=200)
    assert r.exit_cause == "halt", f"expected halt, got {r.exit_cause}"
    assert r.r[0] == 55, f"expected R0=55 (fib(10)), got R0={r.r[0]}"
