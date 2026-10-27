#!/usr/bin/env python3
"""[Blur2PC][macOS] Authored-shader regressions for raster position and stage linkage."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

top, manifest, checker, build = map(Path, sys.argv[1:5])
original = json.loads(manifest.read_text())
symbols = {'vsBasicRenderColoured3D', 'psBasicRenderColoured', 'psZCull', 'vsPointLights', 'psPointLights1',
           'vsOceanOld', 'vsCrowdFlag', 'psPerlin', 'psDoFClassifyBlurAll',
           'vsBasicRenderTextured3D', 'psBasicRenderTextured'}
focused = dict(original)
focused['targets'] = []
for target in original['targets']:
    shaders = [s for s in target['shaders'] if s['symbol'] in symbols or s['file'].lower().endswith('/pscloth.psh')]
    if shaders: focused['targets'].append(dict(target, shaders=shaders))
with tempfile.TemporaryDirectory(prefix='authored-shader-test-', dir=build) as temporary:
    root = Path(temporary); selected = root / 'manifest.json'
    selected.write_text(json.dumps(focused))
    subprocess.run([sys.executable, str(top / 'tools/macos/compile_shaders.py'), '--manifest', str(selected),
                    '--top', str(top), '--out', str(root / 'out'), *sys.argv[5:]], check=True, timeout=60)
    report_path = root / 'out/report.json'; rows = json.loads(report_path.read_text())['shaders']
    by_symbol = {r['symbol']: r for r in rows}
    assert symbols.issubset(by_symbol), 'Authoritative Jam shader selection changed'
    assert not any(b['name'].startswith('$') for r in rows for b in r['reflection'].get('ubos', [])), 'Legacy register uniforms fell back to automatic packing'
    vertex, pixel = by_symbol['vsPointLights'], by_symbol['psPointLights1']
    assert {'semantic':'POSITION','storage':3,'decoration':11,'value':0} in vertex['interfaces'], 'Legacy position was not bound to rasterization'
    outputs = {r['location']:r['type'] for r in vertex['reflection'].get('outputs', [])}
    inputs = {r['location']:r['type'] for r in pixel['reflection'].get('inputs', [])}
    assert inputs == outputs and inputs == {0:'vec2', 1:'vec3'}, 'Actual point-light stages cannot link'
    outputs={r['location']:r['type'] for r in by_symbol['vsBasicRenderTextured3D']['reflection']['outputs']}
    inputs={r['location']:r['type'] for r in by_symbol['psBasicRenderTextured']['reflection']['inputs']}
    assert inputs==outputs=={0:'vec4',16:'vec4'}, 'Original textured stage signatures no longer link by semantic'
    assert any(b['name']=='BlurPCAlphaTestState' and b['binding']==13 for b in pixel['reflection']['ubos']), 'Fixed-function alpha testing was omitted'
    depth = by_symbol['psZCull']
    assert {'semantic':'DEPTH0','storage':3,'decoration':11,'value':22} in depth['interfaces'], 'Depth output became a color target'
    cloth = [r for r in rows if r['source'].lower().endswith('/pscloth.psh')]
    assert cloth, 'Cloth front/back-face regression is missing'
    for row in cloth:
        text = Path(row['metal']).read_text()
        assert '[[front_facing]]' in text and '? 1.0 : (-1.0)' in text, 'Cloth lost signed VFACE behavior'
    subprocess.run([str(checker), str(report_path)], check=True, timeout=60)
print('Authored shader raster position, stage linking, depth, VFACE and engine resource loading PASS')
