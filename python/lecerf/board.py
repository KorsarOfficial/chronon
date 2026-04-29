"""Board — ergonomic wrapper around liblecerf C ABI.

Usage:
    b = Board("stm32f103")
    result = b.flash("firmware.bin").run(timeout_ms=500)
    print(result.r[0], b.cpu.r[0], b.uart.output(), b.gpio["GPIOC"][13].value)
    del b  # safe; __del__ calls lecerf_board_destroy exactly once
"""

import ctypes
import os
from typing import Optional, Union

from ._core import _lib, c_int, c_uint32, c_void_p
from .run_result import RunResult


_EXIT_CAUSE = {0: "halt", 1: "timeout", 2: "fault"}


class _RegFile:
    """Proxy for CPU registers R0-R15. Reads live from board state."""

    __slots__ = ("_b",)

    def __init__(self, b: int) -> None:
        self._b = b

    def __getitem__(self, n: int) -> int:
        if not 0 <= n < 16:
            raise IndexError(f"register index {n} out of range 0-15")
        return int(_lib.lecerf_board_cpu_reg(self._b, c_uint32(n)))


class _Cpu:
    """CPU register file and APSR proxy."""

    __slots__ = ("_b", "_regs")

    def __init__(self, b: int) -> None:
        self._b = b
        self._regs = _RegFile(b)

    @property
    def r(self) -> _RegFile:
        return self._regs

    @property
    def apsr(self) -> int:
        return int(_lib.lecerf_board_cpu_reg(self._b, c_uint32(16)))


class _Pin:
    """Single GPIO pin. .value returns bool (live read)."""

    __slots__ = ("_b", "_port", "_pin")

    def __init__(self, b: int, port: int, pin: int) -> None:
        self._b    = b
        self._port = port
        self._pin  = pin

    @property
    def value(self) -> bool:
        return bool(_lib.lecerf_board_gpio_get(
            self._b, c_uint32(self._port), c_uint32(self._pin)))


class _Port:
    """GPIO port (A/B/C). Supports pin[0-15] subscript."""

    __slots__ = ("_b", "_port")

    def __init__(self, b: int, port: int) -> None:
        self._b    = b
        self._port = port

    def __getitem__(self, pin: int) -> _Pin:
        if not 0 <= pin < 16:
            raise IndexError(f"pin index {pin} out of range 0-15")
        return _Pin(self._b, self._port, pin)


class _Gpio:
    """GPIO peripheral proxy. Supports gpio['GPIOA']['GPIOB']['GPIOC'] subscript."""

    _PORTS = {"GPIOA": 0, "GPIOB": 1, "GPIOC": 2}

    __slots__ = ("_b",)

    def __init__(self, b: int) -> None:
        self._b = b

    def __getitem__(self, name: str) -> _Port:
        port_idx = self._PORTS.get(name)
        if port_idx is None:
            raise KeyError(
                f"Unknown GPIO port {name!r}. Valid: {list(self._PORTS)}"
            )
        return _Port(self._b, port_idx)


class _Uart:
    """UART TX drain. output() returns bytes captured since last drain."""

    _CAP = 64 * 1024  # 64 KB drain buffer

    __slots__ = ("_b", "_buf")

    def __init__(self, b: int) -> None:
        self._b   = b
        self._buf = ctypes.create_string_buffer(self._CAP)

    def output(self) -> bytes:
        """Drain UART TX buffer. Returns bytes (may be empty)."""
        n = _lib.lecerf_board_uart_drain(self._b, self._buf, c_uint32(self._CAP))
        if n == 0:
            return b""
        return bytes(self._buf.raw[:n])


class Board:
    """Cortex-M emulator board.

    b = Board("stm32f103")          # or "stm32f407", "generic-m4"
    result = b.flash("fw.bin").run(timeout_ms=500)
    r0 = b.cpu.r[0]
    out = b.uart.output()
    pin_val = b.gpio["GPIOC"][13].value
    del b                           # lecerf_board_destroy called safely
    """

    __slots__ = ("_b", "cpu", "gpio", "uart")

    def __init__(self, board_name: str) -> None:
        handle = _lib.lecerf_board_create(board_name.encode("utf-8"))
        if not handle:
            raise RuntimeError(
                f"Unknown board: {board_name!r}. "
                "Valid names: 'stm32f103', 'stm32f407', 'generic-m4'."
            )
        self._b   = handle
        self.cpu  = _Cpu(self._b)
        self.gpio = _Gpio(self._b)
        self.uart = _Uart(self._b)

    def flash(self, src: Union[str, os.PathLike, bytes, bytearray]) -> "Board":
        """Load firmware into flash. Accepts a file path or raw bytes.

        Returns self so calls can be chained: b.flash(path).run().
        Raises RuntimeError on flash error.
        """
        if isinstance(src, (str, os.PathLike)):
            with open(src, "rb") as fh:
                data = fh.read()
        else:
            data = bytes(src)
        rc = _lib.lecerf_board_flash(self._b, data, c_uint32(len(data)))
        if rc == 0:
            raise RuntimeError(
                f"lecerf_board_flash failed (rc=0). "
                f"Firmware size={len(data)} bytes."
            )
        return self

    def run(
        self,
        timeout_ms: int = 1000,
        max_steps: Optional[int] = None,
    ) -> RunResult:
        """Run up to max_steps instructions (default: timeout_ms * 100_000).

        Returns RunResult with exit_cause, cycles, and full register snapshot.
        """
        # ~100M IPS → 100K insns per ms is a conservative instruction budget
        steps = max_steps if max_steps is not None else timeout_ms * 100_000
        cause = c_int(0)
        n = _lib.lecerf_board_run(self._b, ctypes.c_uint64(steps),
                                  ctypes.byref(cause))
        ec = _EXIT_CAUSE.get(cause.value, "fault")
        # Snapshot all registers once to avoid 17 separate ctypes calls on access
        regs = [int(_lib.lecerf_board_cpu_reg(self._b, c_uint32(i)))
                for i in range(16)]
        apsr = int(_lib.lecerf_board_cpu_reg(self._b, c_uint32(16)))
        return RunResult(exit_cause=ec, cycles=int(n), r=regs, apsr=apsr)

    def __del__(self) -> None:
        try:
            if getattr(self, "_b", None):
                _lib.lecerf_board_destroy(self._b)
                self._b = None
        except Exception:
            pass  # never raise from __del__

    def __repr__(self) -> str:
        return f"<Board handle={self._b:#x}>"
