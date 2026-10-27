"""[Blur2PC] Independent authored secure PAKs and OpenSSL RSA-PSS signatures.

The generated private key and authored archives live only in the supplied scratch
folder. No game installation is needed. Optional final argument verifies an
existing private installation through the original native engine as well.
"""
import hashlib
import pathlib
import struct
import subprocess
import sys
from test_pak import make_pak


def main():
    executable, folder = sys.argv[1:3]
    folder = pathlib.Path(folder)
    folder.mkdir(parents=True, exist_ok=True)
    key = folder / "private.pem"
    subprocess.run(["/usr/bin/openssl", "genpkey", "-algorithm", "RSA", "-pkeyopt", "rsa_keygen_bits:1024", "-out", str(key)], check=True, capture_output=True)
    subprocess.run(["/usr/bin/openssl", "rsa", "-in", str(key), "-RSAPublicKey_out", "-outform", "DER", "-out", str(folder / "public.der")], check=True, capture_output=True)
    payload = bytes((i * 29 + i // 71) % 256 for i in range(150_037))
    (folder / "payload.bin").write_bytes(payload)
    for version in (1, 2):
        for codec in (1, 2, 4):
            # make_pak is the existing independent PC-format serializer.
            data, _ = make_pak(payload, version=version, codec=codec, secure=True)
            sector = struct.unpack_from("<I", data, 8)[0]
            cursor = 16 + struct.unpack_from("<I", data, 12)[0] if version == 1 else sector
            cursor += 4 + struct.unpack_from("<I", data, cursor)[0]
            cursor += sector - cursor % sector
            count, length = struct.unpack_from("<II", data, cursor)
            assert count == 1
            stem = folder / f"v{version}-c{codec}"
            stem.with_suffix(".pak").write_bytes(data)
            stem.with_suffix(".sha1").write_bytes(hashlib.sha1(data[cursor + 8:cursor + 8 + length]).digest())
            subprocess.run(["/usr/bin/openssl", "pkeyutl", "-sign", "-inkey", str(key), "-in", str(stem.with_suffix(".sha1")), "-out", str(stem.with_suffix(".sig")), "-pkeyopt", "digest:sha1", "-pkeyopt", "rsa_padding_mode:pss", "-pkeyopt", "rsa_pss_saltlen:8"], check=True)
    subprocess.run([executable, str(folder), *sys.argv[3:]], check=True)


if __name__ == "__main__":
    main()
