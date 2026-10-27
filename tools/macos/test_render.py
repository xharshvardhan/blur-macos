#!/usr/bin/env python3
"""[Blur2PC][macOS] Render actual authored shaders on authored geometry without retail data."""
from pathlib import Path
import subprocess
import json
import sys
import tempfile
top,manifest,renderer,build=map(Path,sys.argv[1:5])
with tempfile.TemporaryDirectory(prefix='engine-render-',dir=build) as temporary:
    selected=json.loads(manifest.read_text());targets=[]
    for target in selected['targets']:
        shaders=target['shaders'] if target['name']=='display' else [s for s in target['shaders'] if s['symbol'] in ('psPerlin','psTextureCubeColour')]
        if shaders:targets.append(dict(target,shaders=shaders))
    selected['targets']=targets;focused=Path(temporary)/'manifest.json';focused.write_text(json.dumps(selected))
    subprocess.run([sys.executable,str(top/'tools/macos/compile_shaders.py'),'--manifest',str(focused),
        '--top',str(top),'--out',temporary,*sys.argv[5:]],check=True,timeout=60)
    subprocess.run([str(renderer),temporary],check=True,timeout=30)
