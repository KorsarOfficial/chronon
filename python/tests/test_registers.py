"""test_registers: exact register state after canonical firmware runs.

All values verified by running actual firmware before hardcoding:
  test1  fib(10)=55=0x37          R0=0x37
  test2  sort (9 elements)        R0=0x2802d0, R1=1, R2=9
  test4  systick counter          R0=5
  test10 stm32_blink loop         R0=5  (stm32f103 profile)
  test11 nvic_irq handler count   R0=1, R1=10
"""

import pytest
from .conftest import fw

# (firmware_name, board_profile, {reg_index: expected_value})
CASES = [
    ("test1",              "generic-m4", {0: 0x37}),
    ("test2",              "generic-m4", {0: 0x2802d0, 1: 0x1, 2: 0x9}),
    ("test4",              "generic-m4", {0: 5}),
    ("test10_stm32_blink", "stm32f103",  {0: 5}),
    ("test11_nvic_irq",    "generic-m4", {0: 1, 1: 10}),
]


@pytest.mark.parametrize("fwname,profile,expected", CASES, ids=[c[0] for c in CASES])
def test_register_state(fwname, profile, expected):
    from lecerf import Board
    b = Board(profile)
    r = b.flash(fw(fwname)).run(timeout_ms=2000)
    assert r.exit_cause in ("halt", "timeout"), (
        f"{fwname}: unexpected exit_cause={r.exit_cause!r}"
    )
    for reg, val in expected.items():
        assert r.r[reg] == val, (
            f"{fwname}: R{reg}={r.r[reg]:#x} expected {val:#x}"
        )
    del b
