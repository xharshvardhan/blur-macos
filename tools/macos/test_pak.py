"""Independent serialized fixtures for the native reader; no game assets.

Layout authority: PakHeader.h, PakFatEntry.h, PakRead.cpp, PakUtil.cpp,
PakCompressPacketZLib.cpp. Compare decoded bytes via SHA-256, not self-reported PASS.
"""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

PROBE = None  # Set by the CLI; fixtures can also be imported by engine integration tests.
CIPHER = b"xVXo40j3@$%\\%`"
SECTOR = 512
NAME = b"\\Fixture\\Entry.BIN"


def packet_encode(payload, raw=False):
    encoded = bytearray(struct.pack("<I", 65536))
    for offset in range(0, len(payload), 65536):
        block = payload[offset:offset + 65536].ljust(65536, b"\0")
        if raw:
            encoded += struct.pack("<I", 0xFFFFFFFF) + block
        else:
            compressor = zlib.compressobj(wbits=-15)
            compressed = compressor.compress(block) + compressor.flush()
            encoded += struct.pack("<I", len(compressed)) + compressed
    return bytes(encoded)


def make_pak(payload=b"native fixture", version=2, codec=1, secure=False, raw_packets=False,
             strings=None):
    if codec == 1:
        stored = payload
    elif codec == 2:
        stored = zlib.compress(payload)
    elif codec == 4:
        stored = packet_encode(payload, raw_packets)
    else:
        stored = payload
    plain = NAME + b"\0" if strings is None else strings
    encoded = bytes(c ^ CIPHER[(i + 1) % len(CIPHER)] for i, c in enumerate(plain))
    header_size = 40 if version == 1 else 32
    string_start = 16 + header_size if version == 1 else SECTOR
    string_end = string_start + 4 + len(encoded)
    fat_header_start = string_end + SECTOR - string_end % SECTOR
    stride = (32 if version == 1 else 40) + (24 if secure else 0)
    data_start = (fat_header_start + 8 + stride + SECTOR - 1) // SECTOR * SECTOR
    crc = zlib.crc32(NAME.lower()) ^ 0xFFFFFFFF
    entry = struct.pack("<IIIIQQ", crc, 0, 0xFFFFFFFF, len(payload), 0, data_start // SECTOR)
    if version == 2:
        entry += struct.pack("<II", 0, len(stored))
    if secure:
        entry += struct.pack("<I", 0) + hashlib.sha1(stored).digest()
    if version == 1:
        header = struct.pack("<20sBBBBIIQ", bytes(20), codec, int(secure), ord("\\"), 0, 0, 0, 0)
    else:
        header = struct.pack("<BBBBIIQIII", codec, int(secure), ord("\\"), 0, 0, 0, 0,
                             fat_header_start - string_start, SECTOR, 0)
    archive = bytearray(struct.pack("<IIII", 0x50414B32, version, SECTOR, header_size) + header)
    archive += bytes(string_start - len(archive))
    archive += struct.pack("<I", len(encoded)) + encoded
    archive += bytes(fat_header_start - len(archive))
    archive += struct.pack("<II", 1, len(entry)) + entry
    archive += bytes(data_start - len(archive))
    if version == 1:
        archive += struct.pack("<I", len(stored))
    archive += stored
    return archive, fat_header_start


class PakTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="blur-native-test-")
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / "fixture.pak"

    def invoke(self, data, read=True):
        self.path.write_bytes(data)
        arguments = [PROBE, "--pak", str(self.path)]
        if read:
            arguments += ["--read", "fixture/ENTRY.bin"]
        return subprocess.run(arguments, text=True, capture_output=True, timeout=15)

    def decoded(self, payload, **kwargs):
        data, _ = make_pak(payload, **kwargs)
        result = self.invoke(data)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["sha256"], hashlib.sha256(payload).hexdigest())
        self.assertEqual(report["read_bytes"], len(payload))
        self.assertTrue(report["engine_stream"])
        self.assertFalse(report["fat_authenticity_verified"])

    def test_all_supported_versions_codecs_and_secure_variants(self):
        payload = bytes(range(256)) * 300  # crosses the 64 KiB packet boundary
        for version in (1, 2):
            for codec in (1, 2, 4):
                for secure in (False, True):
                    with self.subTest(version=version, codec=codec, secure=secure):
                        self.decoded(payload, version=version, codec=codec, secure=secure)

    def test_raw_packet_and_final_partial_packet(self):
        for version in (1, 2):
            self.decoded(b"last partial" * 9000, version=version, codec=4, raw_packets=True)

    def test_empty_files(self):
        for version in (1, 2):
            for codec in (1, 2, 4):
                self.decoded(b"", version=version, codec=codec)

    def test_string_table_exact_alignment_consumes_full_sector(self):
        # The upstream reader advances a whole extra sector at exact alignment.
        self.decoded(b"aligned", strings=(NAME + b"\0").ljust(508, b"\0"))

    def test_every_truncation_before_payload_is_rejected(self):
        data, _ = make_pak(b"payload", secure=True)
        for length in (0, 4, 15, 16, 47, 512, 515, 1024, 1040, len(data) - 1):
            with self.subTest(length=length):
                self.assertNotEqual(self.invoke(data[:length], read=False).returncode, 0)

    def test_bad_magic_version_sector_header_and_fat_size(self):
        original, fat = make_pak()
        for offset, value in ((0, 0), (4, 3), (8, 0), (8, 513), (12, 31), (fat + 4, 39)):
            with self.subTest(offset=offset, value=value):
                data = original.copy()
                struct.pack_into("<I", data, offset, value)
                self.assertNotEqual(self.invoke(data, read=False).returncode, 0)

    def test_bad_filename_and_parent_offsets(self):
        original, fat = make_pak()
        for offset in (fat + 8 + 4, fat + 8 + 8):
            data = original.copy()
            struct.pack_into("<I", data, offset, 123456)
            self.assertNotEqual(self.invoke(data, read=False).returncode, 0)

    def test_unterminated_filename(self):
        data, _ = make_pak(strings=NAME)
        self.assertNotEqual(self.invoke(data, read=False).returncode, 0)

    def test_payload_overflow_and_overlap(self):
        original, fat = make_pak()
        for sector in (0, 0xFFFFFFFFFFFFFFFE):
            data = original.copy()
            struct.pack_into("<Q", data, fat + 8 + 24, sector)
            self.assertNotEqual(self.invoke(data, read=False).returncode, 0)

    def test_secure_hash_detects_payload_corruption(self):
        data, _ = make_pak(b"corruption detection", secure=True)
        data[-1] ^= 1
        result = self.invoke(data)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("SHA-1 mismatch", result.stderr)

    def test_unsupported_codec_rejected_on_read(self):
        data, _ = make_pak(codec=3)
        self.assertNotEqual(self.invoke(data).returncode, 0)

    def test_corrupt_zlib_is_rejected(self):
        data, _ = make_pak(codec=2)
        data[-1] ^= 1
        self.assertNotEqual(self.invoke(data).returncode, 0)

    def test_invalid_packet_size_and_trailing_data(self):
        original, fat = make_pak(codec=4)
        data = original.copy()
        struct.pack_into("<I", data, len(data) - len(packet_encode(b"native fixture")), 32768)
        self.assertNotEqual(self.invoke(data).returncode, 0)
        data = original + b"trailing"
        struct.pack_into("<I", data, fat + 8 + 36, len(packet_encode(b"native fixture")) + 8)
        self.assertNotEqual(self.invoke(data).returncode, 0)

    def test_read_allocation_limit(self):
        data, fat = make_pak()
        struct.pack_into("<I", data, fat + 8 + 12, 0xFFFFFFFF)
        result = self.invoke(data)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("read limit", result.stderr)

    def test_data_root_missing_assets_fails(self):
        result = subprocess.run([PROBE, "--data-root", self.directory.name], text=True,
                                capture_output=True, timeout=15)
        self.assertNotEqual(result.returncode, 0)


if __name__ == "__main__":
    PROBE = str(Path(sys.argv.pop(1)).resolve())
    unittest.main(verbosity=2)
