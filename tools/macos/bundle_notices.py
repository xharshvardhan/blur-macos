#!/usr/bin/env python3
# [Blur2PC] BundleUtilities supplies dependency resolution; this records provenance/notices, never rewrites Mach-O.
import argparse
import hashlib
import json
import plistlib
from pathlib import Path
import re
import shutil
import subprocess

NATIVE = ('SDL3.txt', 'SoundTouch.txt', 'MojoShader.txt')
MANIFEST = Path('Contents/Resources/ThirdParty/manifest.json')


def require(value, message):
    if not value:
        raise RuntimeError(message)


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def validate_source(app, licenses):
    require((app / 'Contents/MacOS/BlurNativeGame').is_file(), 'Source game executable is missing')
    require(not (app / 'Contents/Frameworks').exists(), 'Use the raw build app; already bundled dependencies have different provenance')
    for name in NATIVE:
        require((licenses / name).is_file(), 'Missing adjacent native notice: ' + name)
    forbidden = {'.pak', '.bik', '.bank', '.level', '.levelphysics', '.levelrender', '.levelstream', '.pdb', '.o', '.a', '.save'}
    for path in app.rglob('*'):
        relative = path.relative_to(app)
        require(not path.is_symlink(), 'Raw source app contains an unsupported symlink: ' + str(relative))
        require(not any(p.lower() in {'.git', '_work', 'profiles', 'saves'} for p in relative.parts), 'Private/build directory in source app: ' + str(relative))
        require(path.suffix.lower() not in forbidden and path.name.lower() not in {'blursave', 'profiles.bin', 'local-profile-id', 'cmakecache.txt', 'build.ninja'}, 'Game/private/build payload in source app: ' + str(relative))
        if path.is_file():
            permitted = (relative in [Path('Contents/Info.plist'), Path('Contents/PkgInfo'), Path('Contents/MacOS/BlurNativeGame')]
                         or relative.parts[:3] == ('Contents', 'Resources', 'Shaders')
                         or relative.parts[:2] == ('Contents', '_CodeSignature')
                         or (relative.parent == Path('Contents/Resources') and path.suffix == '.icns'))
            require(permitted, 'Unexpected source app payload (not copied): ' + str(relative))


def package_root(library):
    parts = library.resolve().parts
    require('Cellar' in parts, 'No Homebrew provenance for dependency: ' + library.name)
    at = parts.index('Cellar')
    require(len(parts) > at + 3, 'Malformed Cellar path: ' + library.name)
    return Path(*parts[:at + 3])


def copy_notices(root, destination):
    sbom = root / 'sbom.spdx.json'
    require(sbom.is_file(), 'Missing installed SPDX for ' + root.parent.name + '/' + root.name)
    document = json.loads(sbom.read_text())
    require(str(document.get('spdxVersion', '')).startswith('SPDX-'), 'Invalid installed SPDX for ' + root.parent.name)
    notices = sorted(p for p in root.rglob('*') if p.is_file() and re.match(r'^(license|copying|notice|copyright|authors)([._-]|$)', p.name, re.I))
    require(notices, 'Missing license/notice files for ' + root.parent.name + '/' + root.name)
    files = []
    for source in [sbom] + notices:
        require(source.resolve().is_relative_to(root.resolve()), 'Notice symlink escapes package: ' + str(source))
        relative = source.relative_to(root)
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        files.append({'path': str(relative), 'sha256': sha(target)})
    return {'installed_spdx_version': document['spdxVersion'], 'files': files}


def collect(source_app, app, licenses, dependency_file):
    validate_source(source_app, licenses)
    third = app / MANIFEST.parent
    require(not third.exists(), 'ThirdParty destination must be new')
    third.mkdir(parents=True)
    manifest = {'schema': 1, 'purpose': 'Native development bundle provenance; individual notices retain their terms',
                'original_executable_sha256': sha(source_app / 'Contents/MacOS/BlurNativeGame'),
                'native_notices': [], 'packages': [], 'dependencies': []}
    for name in NATIVE:
        target = third / 'Native' / name
        target.parent.mkdir(exist_ok=True)
        shutil.copyfile(licenses / name, target)
        manifest['native_notices'].append({'path': str(target.relative_to(third)), 'sha256': sha(target)})
    packages = {}
    for line in dependency_file.read_text().splitlines():
        columns = line.split('\t')
        require(len(columns) == 2, 'Malformed BundleUtilities dependency record')
        original, embedded = map(Path, columns)
        original = original.resolve(strict=True)
        require(embedded.is_relative_to(app) and embedded.relative_to(app).parts[:2] == ('Contents', 'Frameworks'), 'Dependency destination escapes Frameworks')
        root = package_root(original)
        key = root.parent.name + '/' + root.name
        if key not in packages:
            record = copy_notices(root, third / 'Homebrew' / key)
            record.update(name=root.parent.name, version=root.name, directory='Homebrew/' + key)
            packages[key] = record
        manifest['dependencies'].append({'bundle_path': str(embedded.relative_to(app)),
                                         'origin': 'Cellar/' + key + '/' + str(original.relative_to(root)),
                                         'original_sha256': sha(original), 'package': key})
    require(manifest['dependencies'], 'No external dependency provenance found; use the original unbundled game app')
    manifest['packages'] = list(packages.values())
    (app / MANIFEST).write_text(json.dumps(manifest, indent=2) + '\n')


def inspect(app):
    manifest = json.loads((app / MANIFEST).read_text())
    paths = [app / 'Contents/MacOS/BlurNativeGame'] + [app / d['bundle_path'] for d in manifest['dependencies']]
    expected = {p.resolve(strict=True) for p in paths}
    actual = {p.resolve(strict=True) for p in (app / 'Contents/Frameworks').rglob('*') if p.is_file()}
    require(actual == expected - {paths[0].resolve()}, 'Unexpected or missing bundled library payload')
    versions = []
    for path in expected:
        require(path.is_relative_to(app.resolve()), 'Bundled symlink escapes app')
        arch = subprocess.check_output(['/usr/bin/lipo', '-archs', str(path)], text=True).split()
        require(arch == ['arm64'], 'Expected Apple Silicon-only Mach-O: ' + path.name)
        load = subprocess.check_output(['/usr/bin/otool', '-l', str(path)], text=True)
        found = re.findall(r'\bminos ([\d.]+)', load) or re.findall(r'cmd LC_VERSION_MIN_MACOSX\s+cmdsize \d+\s+version ([\d.]+)', load)
        require(found, 'Missing Mach-O minimum OS: ' + path.name)
        versions += found
        dependencies = subprocess.check_output(['/usr/bin/otool', '-L', str(path)], text=True).splitlines()[1:]
        for line in dependencies:
            item = line.strip().split(' (compatibility', 1)[0]
            require(item.startswith(('/usr/lib/', '/System/Library/', '@executable_path/', '@loader_path/', '@rpath/')), 'External runtime load path: ' + item)
        for rpath in re.findall(r'cmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset', load):
            require(rpath.startswith(('@executable_path/', '@loader_path/')), 'External runtime search path: ' + rpath)
    return manifest, max(versions, key=lambda v: tuple(map(int, v.split('.'))))


def verify_notices(app, manifest):
    third = app / MANIFEST.parent
    files = [(third / x['path'], x['sha256']) for x in manifest['native_notices']]
    files += [(third / p['directory'] / x['path'], x['sha256']) for p in manifest['packages'] for x in p['files']]
    for path, digest in files:
        require(path.is_file() and sha(path) == digest, 'Notice missing or modified: ' + str(path.relative_to(app)))


def metadata(app, identifier, name, version, build, minimum):
    require(bool(re.fullmatch(r'[A-Za-z0-9-]+(?:\.[A-Za-z0-9-]+)+', identifier or '')), 'Invalid bundle identifier')
    require(bool(name) and len(name) <= 100 and all(ord(c) >= 32 for c in name), 'Invalid bundle name')
    for value in (version, build, minimum):
        require(bool(re.fullmatch(r'\d+(?:\.\d+){0,2}', value or '')), 'Invalid bundle version/minimum OS')
    path = app / 'Contents/Info.plist'
    info = plistlib.loads(path.read_bytes())
    info.update(CFBundleIdentifier=identifier, CFBundleName=name, CFBundleDisplayName=name,
                CFBundleShortVersionString=version, CFBundleVersion=build,
                CFBundleGetInfoString=name + ' ' + version + ' (development)', LSMinimumSystemVersion=minimum)
    path.write_bytes(plistlib.dumps(info, sort_keys=False))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=['validate-source', 'collect', 'inspect', 'metadata', 'seal', 'verify'])
    parser.add_argument('--app', type=Path)
    parser.add_argument('--source-app', type=Path)
    parser.add_argument('--licenses', type=Path)
    parser.add_argument('--dependencies', type=Path)
    parser.add_argument('--identifier')
    parser.add_argument('--name')
    parser.add_argument('--version')
    parser.add_argument('--build-version')
    parser.add_argument('--minimum-os')
    args = parser.parse_args()
    try:
        require(args.source_app and args.licenses if args.operation in ('validate-source', 'collect') else args.app, 'Required app/source/notice arguments are missing')
        if args.operation == 'validate-source':
            validate_source(args.source_app.resolve(), args.licenses.resolve())
        elif args.operation == 'collect':
            collect(args.source_app.resolve(), args.app.resolve(), args.licenses.resolve(), args.dependencies)
        elif args.operation == 'metadata':
            metadata(args.app, args.identifier, args.name, args.version, args.build_version, args.minimum_os)
        else:
            manifest, minimum = inspect(args.app.resolve())
            if args.operation == 'seal':
                manifest['minimum_macos'] = minimum
                for record in manifest['dependencies']:
                    record['bundled_sha256'] = sha(args.app / record['bundle_path'])
                (args.app / MANIFEST).write_text(json.dumps(manifest, indent=2) + '\n')
            elif args.operation == 'verify':
                require(manifest.get('minimum_macos') == minimum, 'Manifest deployment version mismatch')
                verify_notices(args.app, manifest)
                for record in manifest['dependencies']:
                    require(sha(args.app / record['bundle_path']) == record['bundled_sha256'], 'Signed dependency hash mismatch')
            print(minimum)
    except (RuntimeError, OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        parser.exit(1, 'Bundle provenance error: ' + str(error) + '\n')

if __name__ == '__main__':
    main()
