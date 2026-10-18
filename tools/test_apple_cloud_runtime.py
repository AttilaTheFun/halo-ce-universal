#!/usr/bin/env python3
"""Run cloud opt-in lifecycle tests with fake authentication, no account access."""
from pathlib import Path
import subprocess
ROOT = Path(__file__).resolve().parents[1]
def main():
    output = ROOT / 'build/apple-tests/cloud_runtime'
    output.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(['clang', '-fobjc-arc', '-fblocks', '-Wall', '-Wextra', '-Werror',
        '-Wno-deprecated-declarations', '-fsanitize=address,undefined',
        'port/apple/save_snapshot.m', 'port/apple/save_restore.m', 'port/apple/cloud_save.m',
        'port/apple/cloud_sync.m', 'port/apple/game_save.m', 'port/apple/tests/cloud_runtime.m',
        '-framework', 'Foundation', '-framework', 'CloudKit', '-framework', 'AppKit', '-weak_framework', 'GameSave',
        '-o', str(output)], cwd=ROOT, check=True)
    subprocess.run([str(output)], cwd=ROOT, check=True, timeout=30)
if __name__ == '__main__':
    main()
