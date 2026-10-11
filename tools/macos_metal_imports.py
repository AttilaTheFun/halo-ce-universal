#!/usr/bin/env python3
"""Retain platform services without the ANGLE/GL bridge in native Mac builds."""
from pathlib import Path
import sys


def native_imports(source):
    return "\n".join(line for line in source.splitlines()
                     if not line.strip().startswith(("hostgl_", "host_gl_", "host_sdl_gl_"))) + "\n"


def main():
    source, destination = map(Path, sys.argv[1:])
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(native_imports(source.read_text()))


if __name__ == "__main__":
    main()
