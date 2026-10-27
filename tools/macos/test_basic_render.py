#!/usr/bin/env python3
"""[Blur2PC][macOS] Compile authored BasicRender shader programs and execute the actual original renderer."""
from pathlib import Path
import json
import shutil
import subprocess
import sys
import tempfile

def main():
    top, manifest, renderer, build = map(Path, sys.argv[1:5])
    symbols = {'vsBasicRenderColoured2D', 'vsBasicRenderColoured3D', 'vsBasicRenderTextured2D', 'vsBasicRenderTextured3D', 'psBasicRenderColoured', 'psBasicRenderTextured'}
    with tempfile.TemporaryDirectory(prefix='basic-render-', dir=build) as temporary:
        root = Path(temporary)
        selected = json.loads(manifest.read_text())
        target = next(t for t in selected['targets'] if t['name'] == 'display')
        shaders = [s for s in target['shaders'] if s['symbol'] in symbols]
        if len(shaders) != len(symbols):
            raise RuntimeError('Expected original Jam BasicRender shader programs missing')
        selected['targets'] = [dict(target, shaders=shaders)]
        focused = root / 'manifest.json'
        focused.write_text(json.dumps(selected))
        subprocess.run([sys.executable, str(top / 'tools/macos/compile_shaders.py'), '--manifest', str(focused), '--top', str(top), '--out', temporary, *sys.argv[5:]], check=True, timeout=60)
        (root / 'Shaders').mkdir(exist_ok=True)
        for symbol in symbols:
            shutil.copyfile(root / 'display' / f'{symbol}.bmsl', root / 'Shaders' / f'{symbol}.bmsl')
        subprocess.run([str(renderer), temporary], check=True, timeout=30)

if __name__ == '__main__':
    main()
