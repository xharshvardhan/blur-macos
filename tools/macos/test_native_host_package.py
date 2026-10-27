#!/usr/bin/env python3
"""Exercise the SDK as a relocated installed package, outside the engine build.

The caller owns the native hardware/window service lease (including under CTest).
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def run(command, cwd, env):
    print("+ " + " ".join(map(str, command)), flush=True)
    subprocess.run(list(map(str, command)), cwd=cwd, env=env, check=True, timeout=70)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--work-root", type=Path, required=True)
    args = parser.parse_args()
    package = args.package.resolve()
    args.work_root.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="installed-client-", dir=args.work_root.resolve()))
    source, build = work / "sdk-source", work / "sdk-build"
    initial, relocated = work / "initial-prefix", work / "relocated-prefix"
    client, client_build = work / "external-client", work / "client-build"
    # Copy before hiding the SDK sources. No repository-relative CMake is available.
    shutil.copytree(package, source)
    shutil.copytree(package / "client", client)
    env = dict(os.environ, BLUR_MAC_AUDIO_MUTE="1")
    for name in ("CPATH", "CPLUS_INCLUDE_PATH", "C_INCLUDE_PATH", "OBJC_INCLUDE_PATH",
                 "LIBRARY_PATH", "CMAKE_PREFIX_PATH", "NativeHost_ROOT", "NativeHost_DIR"):
        env.pop(name, None)
    run(["cmake", "-S", source, "-B", build, "-DCMAKE_BUILD_TYPE=Release",
         f"-DCMAKE_INSTALL_PREFIX={initial}"], work, env)
    run(["cmake", "--build", build, "--parallel", "2"], work, env)
    run(["cmake", "--install", build], work, env)
    initial.rename(relocated)
    # An archive with incidental debug paths is fine; a client requiring those paths is not.
    shutil.rmtree(source)
    shutil.rmtree(build)
    for exported in relocated.rglob("*.cmake"):
        text = exported.read_text()
        for forbidden in (str(package), str(source), str(build), str(initial)):
            if forbidden in text:
                raise AssertionError(f"Non-relocatable export {exported}: {forbidden}")
    run(["cmake", "-S", client, "-B", client_build, "-DCMAKE_BUILD_TYPE=Release",
         "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON", "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF",
         "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF", f"-DCMAKE_PREFIX_PATH={relocated}"], work, env)
    cache = (client_build / "CMakeCache.txt").read_text().splitlines()
    selected = [line.split("=", 1)[1] for line in cache if line.startswith("NativeHost_DIR:PATH=")]
    if len(selected) != 1 or not Path(selected[0]).resolve().is_relative_to(relocated):
        raise AssertionError("Client did not resolve NativeHost from the relocated installation")
    commands = json.loads((client_build / "compile_commands.json").read_text())
    for command in commands:
        text = json.dumps(command)
        if str(relocated / "include") not in text:
            raise AssertionError("Client did not consume the installed public SDK headers")
        for forbidden in (str(package), str(source), str(build), str(initial), "Core.h", "mercury", "amax"):
            if forbidden in text:
                raise AssertionError(f"Client reaches the engine/source tree: {forbidden}")
    run(["cmake", "--build", client_build, "--parallel", "2"], work, env)
    run(["ctest", "--test-dir", client_build, "--output-on-failure", "--parallel", "1"], work, env)
    proof = dict(installed=True, relocated=True, sdk_sources_removed_before_client_build=True,
                 external_client_only=True, exported_target="NativeHost::Window",
                 tests=["native_host_conformance", "native_host_demo"], audio="none",
                 scope="Cocoa window/events/CAMetalLayer host; not a game converter")
    (work / "proof.json").write_text(json.dumps(proof, indent=2) + "\n")
    print(json.dumps(dict(result="PASS", proof=str(work / "proof.json"), **proof)), flush=True)


if __name__ == "__main__":
    main()
