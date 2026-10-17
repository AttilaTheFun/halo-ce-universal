#!/usr/bin/env python3
"""Exercise production save snapshot code without cloud credentials or game data."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def main():
    output = ROOT / 'build/apple-tests/save_snapshot'
    output.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([
        'clang', '-fobjc-arc', '-fblocks', '-Wall', '-Wextra', '-Werror',
        '-fsanitize=address,undefined', '-DHALO_SAVE_RESTORE_TEST=1',
        'port/apple/save_snapshot.m', 'port/apple/save_restore.m', 'port/apple/save_checkpoint.m',
        'port/apple/tests/save_snapshot.m', '-framework', 'Foundation',
        '-o', str(output),
    ], cwd=ROOT, check=True)
    subprocess.run([str(output)], cwd=ROOT, check=True, timeout=30)

if __name__ == '__main__':
    main()
