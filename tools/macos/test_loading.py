"""Independent PC-format archive fixtures for the actual engine loading APIs."""
from pathlib import Path
import subprocess
import sys
import tempfile
from test_pak import make_pak

payload = bytes(range(256)) * 300 + b"native engine loading"
with tempfile.TemporaryDirectory(prefix="blur-engine-loading-") as directory:
    root = Path(directory)
    (root / "payload.bin").write_bytes(payload)
    for version in (1, 2):
        for codec in (1, 2, 4):
            archive, _ = make_pak(payload, version=version, codec=codec)
            (root / f"v{version}-c{codec}.pak").write_bytes(archive)
    subprocess.run([sys.argv[1], str(root)], check=True, timeout=30)
