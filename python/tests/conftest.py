"""pytest configuration and shared helpers for lecerf smoke tests."""

import os

# Firmware root: env override (set by docker runner) wins; else two levels up
# from tests/ -> python/ -> repo root -> firmware/
FIRMWARE_DIR = os.environ.get("LECERF_FW_DIR") or os.path.normpath(
    os.path.join(os.path.dirname(__file__), "..", "..", "firmware")
)


def fw(name: str) -> str:
    """Return absolute path to a firmware binary: firmware/<name>/<name>.bin"""
    return os.path.join(FIRMWARE_DIR, name, name + ".bin")
