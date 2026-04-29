#!/usr/bin/env python3
"""lecerf-runner: run pytest against mounted firmware and tests directories.

Usage:
    lecerf-runner /fw /tests          run pytest; exit code mirrors pytest
    lecerf-runner --version           print version and exit 0
    lecerf-runner --help              print this help and exit 0

Environment:
    LECERF_FW_DIR   overridden to <fw_dir> automatically; pytest_plugin picks
                    it up via the board fixture.

Exit codes:
    0   all tests passed
    1   one or more tests failed
    2   usage error (bad arguments)
    5   no tests collected (pytest exit 5)
"""
import os
import sys
import subprocess

VERSION = "0.1.0"


def usage() -> None:
    print(__doc__)


def main(argv: list) -> int:
    if len(argv) < 2 or argv[1] in ("--help", "-h"):
        usage()
        return 0

    if argv[1] == "--version":
        print(f"lecerf {VERSION}")
        return 0

    if len(argv) < 3:
        print("error: expected <fw_dir> <tests_dir>", file=sys.stderr)
        return 2

    fw_dir, tests_dir = argv[1], argv[2]

    if not os.path.isdir(tests_dir):
        print(f"error: tests directory not found: {tests_dir!r}", file=sys.stderr)
        return 2

    # fw_dir absence is non-fatal: tests may not need actual firmware files
    if not os.path.isdir(fw_dir):
        print(f"warning: firmware directory not found: {fw_dir!r}", file=sys.stderr)

    env = dict(os.environ, LECERF_FW_DIR=fw_dir)
    cmd = [
        "python", "-m", "pytest",
        tests_dir,
        "-v",
        "--tb=short",
    ]
    return subprocess.call(cmd, env=env)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
