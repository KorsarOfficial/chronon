"""RunResult — immutable snapshot of CPU state after Board.run().

Captured once so callers can inspect all registers without repeated
ctypes round-trips. exit_cause is a string: 'halt', 'timeout', 'fault'.
"""

from dataclasses import dataclass, field
from typing import List


@dataclass
class RunResult:
    exit_cause: str          # 'halt' | 'timeout' | 'fault'
    cycles:     int          # instructions executed
    r:          List[int] = field(default_factory=lambda: [0] * 16)
    apsr:       int        = 0

    def __repr__(self) -> str:
        return (
            f"RunResult(exit_cause={self.exit_cause!r}, cycles={self.cycles}, "
            f"r0={self.r[0]:#010x}, apsr={self.apsr:#010x})"
        )
