#!/usr/bin/env python3
"""Fail when a built test executable is not registered with CTest.

The aggregate `hawk-tests` / `minima-tests` target builds every `test_*`
executable. If a new test is added to CMake but `add_test` is forgotten, CTest
silently reports a green run that never executed it. This check closes that gap.

Usage: tests/check_ctest_registration.py [build-dir]
"""
import os
import pathlib
import re
import subprocess
import sys


def main() -> int:
    build = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "build")
    if not build.is_dir():
        print(f"error: build dir not found: {build}", file=sys.stderr)
        return 2

    result = subprocess.run(
        ["ctest", "--test-dir", str(build), "-N"],
        capture_output=True, text=True)
    if result.returncode != 0:
        print(result.stdout)
        print(result.stderr, file=sys.stderr)
        return 2

    registered = set()
    for line in result.stdout.splitlines():
        match = re.match(r"\s*Test\s+#\d+:\s+(.+?)\s*$", line)
        if match:
            registered.add(match.group(1))

    def normalise(name: str) -> str:
        return name[len("test_"):] if name.startswith("test_") else name

    built = sorted(
        p.name for p in build.iterdir()
        if p.is_file() and p.name.startswith("test_") and os.access(p, os.X_OK))

    unregistered = [name for name in built if normalise(name) not in {normalise(r) for r in registered}]
    if unregistered:
        print("error: test executables built but not registered with CTest:")
        for name in unregistered:
            print(f"  {name}  (add_test(NAME ...) missing)")
        return 1

    print(f"ok: {len(built)} test executables, {len(registered)} CTest registrations")
    return 0


if __name__ == "__main__":
    sys.exit(main())
