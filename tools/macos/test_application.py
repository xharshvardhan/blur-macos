#!/usr/bin/env python3
"""[Blur2PC][macOS] Stage only authored source-built shaders for the real framework host fixture."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix='native-application-',dir=sys.argv[3]) as temporary:
    root=Path(temporary);(root/'Shaders').mkdir()
    for source in Path(sys.argv[2]).glob('*/*.bmsl'):
        shutil.copyfile(source,root/'Shaders'/source.name)
    subprocess.run([sys.argv[1],str(root)],check=True,timeout=30)
