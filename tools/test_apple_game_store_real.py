#!/usr/bin/env python3
"""Opt-in production importer round trip with a user-owned ISO; leaves no copy."""
import argparse
import hashlib
import plistlib
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
HARNESS = r'''
#import "game_store.h"
#include <stdio.h>
int main(int argc, char **argv) { @autoreleasepool {
    if (argc != 3) return 2;
    NSString *source = @(argv[1]), *root = @(argv[2]), *failure = nil;
    if (!halo_game_store_import([NSURL fileURLWithPath:source], root, NULL, NULL, &failure)) {
        fprintf(stderr, "%s\n", failure.UTF8String); return 1;
    }
    NSString *generation = halo_game_store_current(root);
    if (!generation) return 3;
    puts(generation.UTF8String);
    return 0;
} }
'''

def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    args = parser.parse_args()
    image = args.image.resolve(strict=True)
    before = image.stat()
    original = digest(image)
    with tempfile.TemporaryDirectory(prefix='halo-real-import-') as work:
        work = Path(work).resolve()
        source = work / 'probe.m'
        source.write_text(HARNESS)
        probe = work / 'probe'
        subprocess.run(['clang', '-fobjc-arc', '-fblocks', '-Wno-deprecated-declarations',
                        '-Iport/apple', str(source), 'port/apple/game_store.m', 'port/apple/xiso.c',
                        '-framework', 'Foundation', '-o', str(probe)], cwd=ROOT, check=True)
        private = work / 'private-container'
        private.mkdir()
        result = subprocess.run([str(probe), str(image), str(private)], capture_output=True,
                                text=True, check=True, timeout=600)
        generation = Path(result.stdout.strip()).resolve()
        assert generation.is_relative_to(private), 'Generation escaped private root'
        retained = generation / 'disc.iso'
        manifest = plistlib.loads((generation / 'integrity.plist').read_bytes())
        assert retained.stat().st_ino != before.st_ino, 'Importer reused original inode'
        assert retained.stat().st_size == before.st_size
        assert digest(retained) == original == manifest['sha256']
        assert digest(image) == original, 'Original image changed'
        assert image.stat().st_mtime_ns == before.st_mtime_ns
        maps = list((generation / 'maps').glob('*.map'))
        assert len(maps) >= 11
        print(f'PASS: retained {before.st_size} image bytes with matching SHA-256; '
              f'{len(maps)} maps extracted; original unchanged; temporary copy removed on exit.')


if __name__ == '__main__':
    main()
