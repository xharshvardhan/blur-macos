#!/usr/bin/env python3
"""[Blur2PC] Author lossless movies and PCM audio, then exercise the real native decoder."""
from pathlib import Path
import subprocess
import sys
import tempfile

executable, shaders, build, ffmpeg = sys.argv[1:5]
executable, shaders, build = (str(Path(value).resolve()) for value in (executable, shaders, build))
with tempfile.TemporaryDirectory(prefix="native-movie-", dir=build) as temporary:
    root = Path(temporary)
    raw = bytearray()
    for frame in range(8):
        for y in range(18):
            for x in range(34):
                raw.extend(((2*(x+y)+12*frame) % 256, (4*y+8*frame) % 256,
                            (2*x+16*frame) % 256, 255))
    (root/"frames.bgra").write_bytes(raw)
    common = [ffmpeg, "-hide_banner", "-loglevel", "error", "-y", "-f", "rawvideo",
              "-pixel_format", "bgra", "-video_size", "34x18", "-framerate", "10",
              "-i", str(root/"frames.bgra")]
    subprocess.run([*common, "-f", "lavfi", "-i", "sine=frequency=440:sample_rate=44100:duration=0.8",
                    "-c:v", "rawvideo", "-c:a", "pcm_f32le", "-shortest", "-f", "nut",
                    str(root/"authored.nut")], check=True, timeout=30)
    subprocess.run([*common, "-frames:v", "2", "-an", "-c:v", "rawvideo", "-f", "nut",
                    str(root/"video-only.nut")], check=True, timeout=30)
    (root/"invalid.nut").write_bytes(b"This is not a media container.\n")
    subprocess.run([executable, str(root), shaders], check=True, timeout=30)
