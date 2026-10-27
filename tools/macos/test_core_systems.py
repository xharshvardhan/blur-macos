#!/usr/bin/env python3
"""[Blur2PC][macOS] Run actual core startup services in separate owned/borrowed I/O processes."""
from pathlib import Path
import subprocess
import sys
import tempfile

executable, build = map(Path, sys.argv[1:3])
with tempfile.TemporaryDirectory(prefix='core-systems-', dir=build) as temporary:
    root = Path(temporary)
    (root / 'payload.bin').write_bytes(bytes((i * 17 + 3) & 255 for i in range(64)))
    for mode in ('owned', 'borrowed'):
        subprocess.run([str(executable), temporary, mode], check=True, timeout=30)
