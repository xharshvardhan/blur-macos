#!/usr/bin/env python3
# [Blur2PC] CLI-only staging regression; no executable/game/GPU/service is launched.
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('bundle_notices', HERE / 'bundle_notices.py')
helper = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helper)


def tree_digest(root):
    values = []
    for path in sorted(root.rglob('*')):
        if path.is_symlink():
            values.append((str(path.relative_to(root)), 'symlink', str(path.readlink())))
        elif path.is_file():
            values.append((str(path.relative_to(root)), path.stat().st_mode, helper.sha(path)))
    return hashlib.sha256(json.dumps(values).encode()).hexdigest()


def require(value, why):
    if not value:
        raise RuntimeError(why)


def verify_relocated_copy(app, destination):
    # [Blur2PC] Bundle aliases are sealed resources: preserve them, never re-sign a bad copy.
    before = tree_digest(app)
    subprocess.run(['/usr/bin/codesign', '--verify', '--deep', '--strict', str(app)], check=True)
    subprocess.run([sys.executable, str(HERE / 'bundle_notices.py'), 'verify', '--app', str(app)], check=True)
    shutil.copytree(app, destination, symlinks=True)
    links = {str(p.relative_to(app)): str(p.readlink()) for p in app.rglob('*') if p.is_symlink()}
    copied_links = {str(p.relative_to(destination)): str(p.readlink()) for p in destination.rglob('*') if p.is_symlink()}
    require(links == copied_links, 'Relocated bundle symlink topology changed')
    require(tree_digest(destination) == before, 'Relocated bundle contents/modes changed')
    subprocess.run(['/usr/bin/codesign', '--verify', '--deep', '--strict', str(destination)], check=True)
    subprocess.run([sys.executable, str(HERE / 'bundle_notices.py'), 'verify', '--app', str(destination)], check=True)
    require(tree_digest(app) == before, 'Original staged app was modified')
    return {'app': str(destination), 'preserved_symlinks': len(links),
            'strict_signature': 'PASS', 'manifest_and_dependency_hashes': 'PASS',
            'original_staged_app_unchanged': True, 're_signed': False}


def pure_failures():
    with tempfile.TemporaryDirectory(prefix='blur-package-check-') as temp:
        root = Path(temp)
        package = root / 'missing-package'
        package.mkdir()
        try:
            helper.copy_notices(package, root / 'copy')
        except RuntimeError as e:
            require('Missing installed SPDX' in str(e), 'Missing SPDX diagnostic changed')
        else:
            raise RuntimeError('Missing SPDX accepted')
        (package / 'sbom.spdx.json').write_text('{"spdxVersion":"SPDX-2.3"}')
        try:
            helper.copy_notices(package, root / 'copy')
        except RuntimeError as e:
            require('Missing license/notice' in str(e), 'Missing notice diagnostic changed')
        else:
            raise RuntimeError('Missing notices accepted')
        raw = root / 'raw.app'
        (raw / 'Contents/MacOS').mkdir(parents=True)
        (raw / 'Contents/MacOS/BlurNativeGame').write_text('authored preflight only')
        licenses = root / 'native-notices'
        licenses.mkdir()
        for name in helper.NATIVE:
            (licenses / name).write_text('authored test notice')
        helper.validate_source(raw, licenses)
        forbidden = raw / 'Contents/Resources/Shaders/retail.pak'
        forbidden.parent.mkdir(parents=True)
        forbidden.write_text('authored forbidden-payload check, no retail bytes')
        try:
            helper.validate_source(raw, licenses)
        except RuntimeError as e:
            require('Game/private/build payload' in str(e), 'Forbidden payload diagnostic changed')
        else:
            raise RuntimeError('Retail-payload path accepted')
        app = root / 'sample.app'
        (app / 'Contents').mkdir(parents=True)
        (app / 'Contents/Info.plist').write_bytes(plistlib.dumps({}))
        try:
            helper.metadata(app, 'invalid id', 'Development', '0.1.0', '1', '26.0')
        except RuntimeError:
            pass
        else:
            raise RuntimeError('Invalid identifier accepted')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    inputs = parser.add_mutually_exclusive_group(required=True)
    inputs.add_argument('--source-app', type=Path, help='Stage a raw app, then verify a relocated copy')
    inputs.add_argument('--staged-app', type=Path, help='Only verify relocation of an existing signed stage')
    parser.add_argument('--stage-root', type=Path, required=True)
    parser.add_argument('--cmake', default='cmake')
    args = parser.parse_args()
    stage = args.stage_root.resolve()
    require(not stage.exists(), 'Test stage must be new')
    if args.staged_app:
        source = args.staged_app.resolve(strict=True)
        require(not stage.is_relative_to(source), 'Relocation test destination must be outside the app')
        stage.mkdir(parents=True)
        result = {'relocated_copy': verify_relocated_copy(source, stage / source.name), 'runtime_launched': False}
        (stage / 'verification.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result, indent=2))
        return
    source = args.source_app.resolve(strict=True)
    pure_failures()
    before = tree_digest(source)
    command = [args.cmake, '-DBLUR_SOURCE_APP=' + str(source), '-DBLUR_STAGE_DIR=' + str(stage),
               '-DBLUR_BUNDLE_IDENTIFIER=io.github.xharshvardhan.blur-macos.packaging-test',
               '-DBLUR_BUNDLE_NAME=Blur Native Packaging Test', '-DBLUR_BUNDLE_VERSION=0.1.247',
               '-DBLUR_BUNDLE_BUILD_VERSION=247', '-P', str(HERE / 'stage_bundle.cmake')]
    subprocess.run(command, check=True)
    app = stage / source.name
    manifest, minimum = helper.inspect(app)
    helper.verify_notices(app, manifest)
    info = plistlib.loads((app / 'Contents/Info.plist').read_bytes())
    require(info['CFBundleIdentifier'].endswith('.packaging-test') and info['CFBundleDisplayName'] == 'Blur Native Packaging Test', 'Metadata override missing')
    require(info['CFBundleVersion'] == '247' and info['CFBundleShortVersionString'] == '0.1.247', 'Version override missing')
    require(info['LSMinimumSystemVersion'] == minimum, 'Minimum OS is not derived')
    require(len(manifest['native_notices']) == 3 and manifest['packages'] and manifest['dependencies'], 'Notice/provenance inventory missing')
    require('/Users/' not in (app / helper.MANIFEST).read_text(), 'Personal source path leaked into manifest')
    subprocess.run(['/usr/bin/codesign', '--verify', '--deep', '--strict', str(app)], check=True)
    relocated = verify_relocated_copy(app, stage / 'relocated' / source.name)
    repeat = subprocess.run(command, text=True, capture_output=True)
    require(repeat.returncode != 0 and 'new destination directory' in repeat.stderr, 'Existing destination was overwritten')
    require(tree_digest(source) == before, 'Source app was modified')
    result = {'source_tree_sha256_before_and_after': before, 'staged_app': str(app),
              'minimum_macos': minimum, 'native_notices': 3, 'dependency_records': len(manifest['dependencies']),
              'packages_with_notices_and_spdx': len(manifest['packages']), 'strict_signature': 'PASS',
              'relocated_copy': relocated,
              'new_destination_and_missing_provenance_checks': 'PASS', 'runtime_launched': False,
              'limits': 'Local ad-hoc development staging; clean-machine launch and notarization unverified'}
    (stage / 'verification.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))

if __name__ == '__main__':
    main()
