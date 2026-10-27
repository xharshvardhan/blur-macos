#!/usr/bin/env python3
"""[Blur2PC][macOS] Translate Jam-selected authored HLSL to SPIR-V and Metal.

Outputs are development compiler artifacts. Successful translation alone does
not establish runtime shader/layout/scene correctness.
"""
from __future__ import annotations
import argparse
import ast
from concurrent.futures import ThreadPoolExecutor
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import re
import shutil
import subprocess
import struct
import sys

INCLUDE = re.compile(r'(^\s*#\s*include\s*)["<]([^">]+)[">]', re.MULTILINE)
CONSTANT = re.compile(r'^[ \t]*(?:extern\s+)?(\w+\s+\w+(?:\s*\[[^\]]+\])*)\s*:\s*register\s*\(\s*c(\d+)\s*\)\s*;[ \t]*$', re.MULTILINE)

def pc_precision(text):
    """SM5 maps half to float; merge only token-identical resulting helpers.

    https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-scalar
    Glslang otherwise diagnoses the source's float/half helper overload pairs
    as duplicate definitions. Different bodies are deliberately left to fail.
    """
    text = re.sub(r'\bhalf([1-4](?:x[1-4])?)?\b', lambda m: 'float' + (m[1] or ''), text)
    text = re.sub(r'(?<=\d)[hH]\b', 'f', text)
    functions = re.compile(r'\b(?:float\w*|int\w*|void|bool\w*)\s+(\w+)\s*\(([^{};]*)\)\s*\{')
    seen, remove = {}, []
    for match in functions.finditer(text):
        depth, end = 1, match.end()
        while depth and end < len(text):
            depth += (text[end] == '{') - (text[end] == '}'); end += 1
        if depth: raise RuntimeError('Unbalanced HLSL function body')
        key = (match[1], re.sub(r'\s+', '', match[2]))
        body = re.sub(r'\s+', '', text[match.start():end])
        if seen.get(key) == body: remove.append((match.start(), end))
        else: seen[key] = body
    for start, end in reversed(remove): text = text[:start] + text[end:]
    return text

def constants(text):
    """Retain original 16-byte register offsets, including overlapping aliases."""
    groups, needed = [], set()
    structures = {m[1]:m for m in re.finditer(r'\bstruct\s+(\w+)\s*\{([^{}]*)\}\s*;', text)}
    def integer(expression):
        def evaluate(node):
            if isinstance(node, ast.Constant) and type(node.value) is int: return node.value
            if isinstance(node, ast.BinOp):
                left, right = evaluate(node.left), evaluate(node.right)
                if isinstance(node.op, ast.Add): return left + right
                if isinstance(node.op, ast.Sub): return left - right
                if isinstance(node.op, ast.Mult): return left * right
                if isinstance(node.op, ast.Div) and right: return left // right
            raise RuntimeError('Unsupported shader array extent: ' + expression)
        value = evaluate(ast.parse(expression.strip(), mode='eval').body)
        if not 0 < value <= 65536: raise RuntimeError('Shader array extent out of range')
        return value
    def layout(declaration, stack=()):
        match = re.fullmatch(r'(\w+)\s+\w+((?:\s*\[[^\]]+\])*)', declaration.strip())
        if not match: raise RuntimeError('Unsupported uniform declaration: ' + declaration)
        kind, dimensions = match[1], re.findall(r'\[([^\]]+)\]', match[2])
        matrix = re.fullmatch(r'(?:float|int|bool)([1-4])x([1-4])', kind)
        scalar = re.fullmatch(r'(?:float|int|uint|bool)([1-4])?', kind)
        if matrix: size, aligned = int(matrix[1]) * 16, True  # row-major storage
        elif scalar: size, aligned = int(scalar[1] or 1) * 4, False
        elif kind in structures and kind not in stack:
            needed.add(kind); offset = 0
            body = structures[kind][2]
            for field in body.split(';'):
                if not field.strip(): continue
                field_size, field_aligned = layout(field, (*stack, kind))
                if field_aligned or offset % 16 + field_size > 16: offset = (offset + 15) // 16 * 16
                offset += field_size
            size, aligned = (offset + 15) // 16 * 16, True
        else: raise RuntimeError('Unknown or recursive uniform type: ' + kind)
        if dimensions: size, aligned = (size + 15) // 16 * 16 * math.prod(integer(d) for d in dimensions), True
        return size, aligned
    def move(match):
        declaration, register = match[1], int(match[2])
        span = (layout(declaration)[0] + 15) // 16
        occupied = set(range(register, register + span))
        for group in groups:
            if not occupied.intersection(group[0]):
                group[0].update(occupied); group[1].append((declaration, register)); break
        else:
            groups.append((occupied, [(declaration, register)]))
        return ''
    text = CONSTANT.sub(move, text)
    if re.search(r'register\s*\(\s*c\d+', text): raise RuntimeError('Unconverted legacy constant-register declaration')
    type_definitions = []
    for kind, match in structures.items():
        if kind in needed:
            type_definitions.append(match[0]); text = text.replace(match[0], '')
    # Existing explicit cbuffers retain their own binding/packoffset contracts.
    used = {int(n) for n in re.findall(r'register\s*\(\s*b(\d+)\s*\)', text)}
    blocks = []
    for occupied, members in groups:
        binding = next(i for i in range(32) if i not in used); used.add(binding)
        blocks.append(f'cbuffer NativeLegacyConstants{binding} : register(b{binding}) {{\n' +
                      '\n'.join(f'{d} : packoffset(c{r});' for d, r in members) + '\n};\n')
    return '#pragma pack_matrix(row_major)\n' + '\n'.join(type_definitions) + '\n' + ''.join(blocks) + text

def face_semantic(text):
    """Keep D3D9's signed VFACE value when consuming Metal's boolean facing."""
    match = re.search(r'\bmain\s*\(([^{}]*)\)\s*(?::\s*\w+)?\s*\{', text)
    if not match: raise RuntimeError('HLSL entry point not found')
    declarations = []
    def face(m):
        name = m[1]; declarations.append(f'float {name} = native_front_{name} ? 1.0f : -1.0f;')
        return f'bool native_front_{name} : SV_IsFrontFace'
    parameters = re.sub(r'\bfloat\s+(\w+)\s*:\s*VFACE\b', face, match[1])
    text = (text[:match.start(1)] + parameters + text[match.end(1):match.end()] +
            '\n'.join(declarations) + text[match.end():])
    if re.search(r':\s*VFACE\b', text): raise RuntimeError('Unsupported VFACE declaration')
    return text

def link_semantics(path, stage):
    """Bind raster/system values and interstage locations by HLSL semantic.

    Glslang's independent auto-locations use declaration order, which does not
    link arbitrary engine VS/PS pairs. SPV_GOOGLE_hlsl_functionality1 preserves
    the authoritative semantic strings, so no field-name guesses are needed.
    Opcode/decoration values are from the Khronos SPIR-V specification.
    """
    data = path.read_bytes()
    if len(data) % 4: raise RuntimeError('Unaligned SPIR-V')
    words = list(struct.unpack('<%dI' % (len(data) // 4), data))
    instructions, storage, semantics, builtins = [], {}, {}, {}
    entry, depth_replacing = None, False
    cursor = 5
    while cursor < len(words):
        count, opcode = words[cursor] >> 16, words[cursor] & 65535
        if not count or cursor + count > len(words): raise RuntimeError('Invalid SPIR-V instruction')
        instruction = words[cursor:cursor + count]; instructions.append(instruction)
        if opcode == 15: entry = instruction[2]
        if opcode == 16 and instruction[2] == 12: depth_replacing = True
        if opcode == 59: storage[instruction[2]] = instruction[3]  # OpVariable
        if opcode == 71 and instruction[2] == 11: builtins[instruction[1]] = instruction[3]
        if opcode == 5632 and instruction[2] == 5635:  # OpDecorateString / UserSemantic
            semantics[instruction[1]] = struct.pack('<%dI' % (count - 3), *instruction[3:]).split(b'\0')[0].decode().upper()
        cursor += count
    interfaces, used, writes_depth = [], set(), False
    for instruction in instructions:
        if instruction[0] & 65535 != 71 or instruction[2] != 30: continue  # Location
        variable = instruction[1]; semantic = semantics.get(variable, '')
        direction = storage.get(variable)
        if stage == 'vert' and direction == 3 and semantic in ('POSITION', 'POSITION0', 'PSIZE', 'PSIZE0'):
            instruction[2:] = [11, 0 if semantic.startswith('POSITION') else 1]  # BuiltIn
            builtins[variable] = instruction[3]
        elif (stage == 'vert' and direction == 3) or (stage == 'frag' and direction == 1):
            match = re.fullmatch(r'(TEXCOORD|COLOR)(\d*)', semantic)
            if not match: raise RuntimeError('Unknown active interstage semantic: ' + semantic)
            index = int(match[2] or 0)
            if index >= (16 if match[1] == 'TEXCOORD' else 2): raise RuntimeError('Interstage semantic index unsupported: ' + semantic)
            location = index + (16 if match[1] == 'COLOR' else 0)
            if location in used: raise RuntimeError('Duplicate interstage semantic: ' + semantic)
            used.add(location); instruction[3] = location
        elif stage == 'frag' and direction == 3:
            if semantic in ('DEPTH', 'DEPTH0'):
                instruction[2:] = [11, 22]  # BuiltIn FragDepth
                writes_depth = True
            else:
                match = re.fullmatch(r'(?:COLOR|SV_TARGET)(\d*)', semantic)
                if not match: raise RuntimeError('Unknown render-target semantic: ' + semantic)
                instruction[3] = int(match[1] or 0)
        interfaces.append({'semantic': semantic, 'storage': direction, 'decoration': instruction[2], 'value': instruction[3]})
    if stage == 'vert' and sum(storage.get(v) == 3 and b == 0 for v, b in builtins.items()) != 1:
        raise RuntimeError('Vertex shader must write exactly one raster position')
    if writes_depth and not depth_replacing:
        if entry is None: raise RuntimeError('Fragment entry point missing')
        # Execution modes precede debug/annotation/type instructions.
        at = next(i for i, instruction in enumerate(instructions) if instruction[0] & 65535 == 15) + 1
        instructions.insert(at, [(3 << 16) | 16, entry, 12])  # OpExecutionMode DepthReplacing
    words = words[:5] + [w for instruction in instructions for w in instruction]
    path.write_bytes(struct.pack('<%dI' % len(words), *words))
    return interfaces

def metal_varyings(msl, stage):
    """D3D9 interpolators are four-component registers, even for float2/3 reads.

    Metal requires identical interstage types. Give each floating varying the
    PC register width, then preserve the original component reads and writes.
    Vertex attributes, render targets and constant layouts are unaffected.
    """
    kind = 'out' if stage == 'vert' else 'in'
    structure = re.search(r'struct blur_main_' + kind + r'\s*\{(.*?)\n\};', msl, re.S)
    if not structure: return msl
    fields = {}
    def widen(match):
        scalar, width, name = match[1], int(match[2] or 1), match[3]
        fields[name] = 'xyzw'[:width]
        return scalar + '4 ' + name + match[4]
    body = re.sub(r'\b(float|half)([123]?)\s+(\w+)(\s+\[\[user\(locn\d+\))', widen, structure[1])
    if not fields: return msl
    msl = msl[:structure.start(1)] + body + msl[structure.end(1):]
    variable = re.search(r'\bblur_main_' + kind + (r'\s+(\w+)\s*=\s*\{\}' if stage == 'vert' else r'\s+(\w+)\s*\[\[stage_in\]\]'), msl)
    if not variable: raise RuntimeError('Native interstage variable not found')
    prefix = variable[1] + '.'
    for name, components in fields.items():
        msl = re.sub(r'\b' + re.escape(prefix + name) + r'\b', lambda m: m[0] + '.' + components, msl)
    return msl

def shader_blob(msl, reflection, interfaces, stage):
    """Versioned native blob; register reflection accompanies executable source."""
    def byte_size(member):
        if member.get('array'):
            if not all(member.get('array_size_is_literal', [])): raise RuntimeError('Nonliteral uniform array')
            return member['array_stride'] * math.prod(member['array'])
        kind = member['type']
        matrix = re.fullmatch(r'mat([2-4])(?:x([2-4]))?', kind)
        if matrix:
            columns, rows = int(matrix[1]), int(matrix[2] or matrix[1])
            return member['matrix_stride'] * (rows if member.get('row_major') else columns)
        vector = re.fullmatch(r'[biu]?vec([2-4])', kind)
        if vector: return int(vector[1]) * 4
        if kind in ('float', 'int', 'uint', 'bool'): return 4
        if kind in reflection.get('types', {}):
            return max(m['offset'] + byte_size(m) for m in reflection['types'][kind]['members'])
        raise RuntimeError('Unsupported uniform type: ' + kind)
    parameters = {}
    for buffer in reflection.get('ubos', []):
        # Private fixed-function emulation uses packed scalar fields, bound by
        # the backend directly; it is not an authored float4-register parameter.
        if buffer['name'] == 'BlurPCAlphaTestState' and buffer['binding'] == 13: continue
        for member in reflection['types'][buffer['type']]['members']:
            if member['offset'] % 16: raise RuntimeError('Subregister uniform cannot use the engine register API')
            location = [member['offset'] // 16, (byte_size(member) + 15) // 16]
            if member['name'] in parameters and parameters[member['name']] != location:
                raise RuntimeError('Ambiguous uniform name: ' + member['name'])
            parameters[member['name']] = location
    # PC D3DDECLUSAGE identifiers retained by the engine vertex format.
    usages = {'POSITION':0, 'BLENDWEIGHT':1, 'BLENDINDICES':2, 'NORMAL':3, 'PSIZE':4,
              'TEXCOORD':5, 'TANGENT':6, 'BINORMAL':7, 'TESSFACTOR':8, 'POSITIONT':9,
              'COLOR':10, 'FOG':11, 'DEPTH':12, 'SAMPLE':13}
    inputs = []
    if stage == 'vert':
        for item in interfaces:
            if item['storage'] != 1 or item['decoration'] != 30: continue
            match = re.fullmatch(r'([A-Z_]+)(\d*)', item['semantic'])
            if not match or match[1] not in usages: raise RuntimeError('Unknown vertex input semantic: ' + item['semantic'])
            inputs.append([usages[match[1]], int(match[2] or 0), item['value']])
    metadata = json.dumps({'parameters': parameters, 'input_semantics': inputs,
                           'reflection': reflection}, separators=(',', ':')).encode()
    source, entry = msl.encode(), b'blur_main'
    source_offset, entry_offset = 40 + len(metadata), 40 + len(metadata) + len(source)
    header = struct.pack('<4s9I', b'BMSL', 1, 0 if stage == 'vert' else 1,
                         entry_offset + len(entry), 40, len(metadata), source_offset,
                         len(source), entry_offset, len(entry))
    return header + metadata + source + entry

class ShadowIncludes:
    """Normalize Windows include separators in ignored copies, never source."""
    def __init__(self, top, out):
        self.top, self.out, self.cache = top, out, {}
    def source(self, path, includes):
        path = path.resolve()
        # A source can resolve a shared include differently in different targets.
        key = (path, tuple(includes))
        if key in self.cache: return self.cache[key]
        namespace = hashlib.sha256('\0'.join(map(str, includes)).encode()).hexdigest()[:16]
        output = self.out / namespace / path.relative_to(self.top)
        self.cache[key] = output
        def include(match):
            relative = match[2].replace('\\', '/')
            for parent in (path.parent, *includes):
                candidate = parent / relative
                if candidate.is_file():
                    return match[1] + '"' + str(self.source(candidate, includes)) + '"'
            return match[0]  # The real preprocessor diagnoses active missing includes.
        content = INCLUDE.sub(include, path.read_text(encoding='latin-1'))
        output.parent.mkdir(parents=True, exist_ok=True); output.write_text(content)
        return output

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--manifest', type=Path, required=True)
    ap.add_argument('--top', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--target')
    ap.add_argument('--symbols', help='Comma-separated authoritative Jam symbols for a focused native target')
    ap.add_argument('--jobs', type=int, default=4)
    ap.add_argument('--glslang', default=shutil.which('glslangValidator'))
    ap.add_argument('--spirv-cross', default=shutil.which('spirv-cross'))
    ap.add_argument('--spirv-val', default=shutil.which('spirv-val'))
    ap.add_argument('--clang', default=shutil.which('clang'))
    args = ap.parse_args()
    selected_symbols=set(args.symbols.split(',')) if args.symbols else None
    top, out = args.top.resolve(), args.out.resolve()
    glslang, cross, clang, validator = args.glslang, args.spirv_cross, args.clang, args.spirv_val
    if not all((glslang, cross, clang, validator)): ap.error('Install glslang, spirv-cross, spirv-tools and Xcode Command Line Tools')
    manifest = json.loads(args.manifest.read_text())
    # Regenerate ignored register headers from authoritative CP/CV/SP inputs.
    spec = importlib.util.spec_from_file_location('cpgen', top / 'tools/jam2cmake/cpgen.py')
    cpgen = importlib.util.module_from_spec(spec); spec.loader.exec_module(cpgen)
    spec = importlib.util.spec_from_file_location('alpha_test', top / 'tools/dx11_shadercompile/alpha_test.py')
    alpha_test = importlib.util.module_from_spec(spec); spec.loader.exec_module(alpha_test)
    for item in manifest['cpfiles']: cpgen.generate(str(top / item['file']))
    shadow = ShadowIncludes(top, out / 'includes')
    jobs = []
    for target in manifest['targets']:
        if args.target and target['name'] != args.target: continue
        for shader in target['shaders']:
            if selected_symbols is not None and shader['symbol'] not in selected_symbols: continue
            source = top / shader['file']
            includes = list(dict.fromkeys([source.parent, source.parent.parent] +
                       [top / e['path'] for e in target['includes'] if e['exists']]))
            jobs.append((target['name'], shader, shadow.source(source, includes)))
    if selected_symbols is not None:
        missing=selected_symbols-{job[1]['symbol'] for job in jobs}
        if missing:ap.error('Unknown or excluded Jam shader symbols: '+', '.join(sorted(missing)))
    if not jobs: ap.error('No Jam-selected shaders match')
    def compile_one(job):
        target, shader, source = job
        stage = 'frag' if Path(shader['file']).name.lower().startswith('ps') else 'vert'
        output = out / target / shader['symbol']; output.parent.mkdir(parents=True, exist_ok=True)
        for extension in ('.spv', '.metal', '.reflection.json', '.bmsl'):
            output.with_suffix(extension).unlink(missing_ok=True)
        log = []
        def run(command):
            result = subprocess.run(command, capture_output=True, text=True, timeout=120)
            log.append(' '.join(map(str, command)) + '\n' + result.stdout + result.stderr)
            if result.returncode: raise RuntimeError('compiler failed')
            return result.stdout
        report = {'target': target, 'symbol': shader['symbol'], 'source': shader['file'], 'stage': stage}
        try:
            # Native Metal links the original D3D9 semantic contract. The DX11
            # branch also changes some signatures to register-order contracts;
            # mixing those with unchanged semantic-based shaders mislinks pairs.
            definitions = ['BLUR2PC=1', 'BLUR_METAL=1', *[d if '=' in d else d + '=1' for d in shader['defines']]]
            text = run([clang, '-E', '-P', '-x', 'c', '-Wno-unknown-pragmas',
                        *['-D' + d for d in definitions], str(source)])
            text = pc_precision(text)
            if stage == 'frag': text = face_semantic(text)
            text = constants(text)
            if stage == 'frag': text = alpha_test.inject_alpha_test(text, {'main'})
            hlsl = output.with_suffix('.hlsl'); hlsl.write_text(text)
            spv = output.with_suffix('.spv')
            run([glslang, '-D', '-V', '--target-env', 'vulkan1.1', '--hlsl-dx9-compatible',
                 '--hlsl-iomap', '--hlsl-offsets', '--auto-map-bindings', '--auto-map-locations',
                 '-fhlsl_functionality1', '-S', stage, '-e', 'main', '-o', str(spv), str(hlsl)])
            report['interfaces'] = link_semantics(spv, stage)
            run([validator, '--target-env', 'vulkan1.1', str(spv)])
            reflection = run([cross, str(spv), '--reflect'])
            output.with_suffix('.reflection.json').write_text(reflection)
            msl = output.with_suffix('.metal')
            run([cross, str(spv), '--msl', '--msl-version', '23000', '--msl-decoration-binding',
                 '--rename-entry-point', 'main', 'blur_main', stage, '--output', str(msl)])
            msl.write_text(metal_varyings(msl.read_text(), stage))
            parsed = json.loads(reflection)
            blob = output.with_suffix('.bmsl'); blob.write_bytes(shader_blob(msl.read_text(), parsed, report['interfaces'], stage))
            report.update(passed=True, metal=str(msl), blob=str(blob), entry='blur_main', reflection=parsed)
        except (RuntimeError, ValueError, subprocess.TimeoutExpired) as error:
            report.update(passed=False, error=str(error))
        output.with_suffix('.log').write_text('\n'.join(log))
        return report
    with ThreadPoolExecutor(max_workers=args.jobs) as pool: reports = list(pool.map(compile_one, jobs))
    succeeded = sum(r['passed'] for r in reports)
    embedded = []
    if succeeded == len(reports):
        names = [r['symbol'] for r in reports]
        if len(set(names)) != len(names): raise RuntimeError('Duplicate global built-in shader symbols')
        # Native file-backed shader APIs read the same compiled programs from
        # app Resources/Shaders, independently of the user's PC data directory.
        runtime = out / 'Shaders'
        runtime.mkdir(exist_ok=True)
        expected = {name + '.bmsl' for name in names}
        for stale in runtime.glob('*.bmsl'):
            if stale.name not in expected: stale.unlink()
        for row in reports:
            shutil.copyfile(row['blob'], runtime / (row['symbol'] + '.bmsl'))
        for target in sorted({r['target'] for r in reports}):
            lines = ['// [Blur2PC][macOS] Generated authored Metal shader blobs. Do not edit.', '#include <cstdint>', 'extern "C" {']
            for row in [r for r in reports if r['target'] == target]:
                blob = Path(row['blob']).read_bytes(); symbol = row['symbol']
                lines.append(f'alignas(4) extern const std::uint8_t _binary_{symbol}_bin_start[] = {{')
                lines += ['    ' + ','.join(f'0x{byte:02x}' for byte in blob[i:i + 24]) + ',' for i in range(0, len(blob), 24)]
                lines += ['};', f'extern const std::uint32_t _binary_{symbol}_bin_size = {len(blob)};']
            lines.append('}')
            cpp = out / f'{target}_metal_shaders.cpp'; cpp.write_text('\n'.join(lines) + '\n'); embedded.append(str(cpp))
    report = {'total': len(reports), 'succeeded': succeeded, 'shaders': reports, 'embedded_sources': embedded}
    (out / 'report.json').write_text(json.dumps(report, indent=2))
    print(f'Authored HLSL -> SPIR-V -> MSL: {succeeded}/{len(reports)} translated (runtime validation separate)')
    for row in [r for r in reports if not r['passed']][:12]: print(row['symbol'], row['source'])
    return int(succeeded != len(reports))

if __name__ == '__main__': sys.exit(main())
