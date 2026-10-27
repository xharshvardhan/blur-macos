# Original VAG decoder pointer contract (MAC169)

The actual optimized MAC167 native game reaches RaceLoaded, then crashes on an
audio worker in `ADPCM4bitDecoder::ReadVagStreamStereo`, through Wave/Mixer and
the scheduler. The reported fault is a memmove source at a low32-only address
(`0x00000000a3fac080`). Root MAC168 then captures memmove source register x1
exactly `0x00000000fa9b0080`, destination x0 `0x000000016b879780` and length x2
128, with no SB errors, in the same original stereo decoder call chain. Evidence
is private `_work/runtime/verification/mac168/audio-proof.json`. The optimized
binary lacks decoder local-variable information, so this capture does not prove
the original untruncated source pointer value. The source selectors and the
high-pointer regression establish the narrowing mechanism. MAC169's focused
CPU validation below does not establish subsequent whole-game progress.

`code/mercury/library/audio/source/core/decoderadpcm4bit.cpp:79–87,179–187`
contains the same issue in both mono and stereo stream readers. The original
PC branch obtains the low four alignment bits, masks the complete pointer via
U64, then calls `Spu::Fetch`. The console alternative casts the source address
to U32 before masking and reconstructing it. Native previously selected that
console path, discarding its pointer's upper32 bits. The resulting low address
matches the reported failure class exactly.

Both selectors now include `PLATFORM_MACOS` alongside `PLATFORM_PC`, with
`[Blur2PC]` annotations. The authoritative existing PC alignment expression is
unchanged. Low four bits remain the packet offset inside the aligned fetch;
packet size remains33 bytes per64 samples per channel (66 bytes per stereo
packet pair). The existing64/128-byte padded fetch windows, predictors,
loop/sample counters, fader, PCM writes and serialized data remain unchanged.
No decoder fallback, disabled audio or new resampler is introduced. The only
other pointer-to-U32 cast in this decoder file is inside inactive
`PLATFORM_SPUA` vector code after the existing scalar DecodeBlock3 return; it
is not selected by native.

## Focused validation

`tools/macos/adpcm_stream_test.cpp` uses actual original MonoADPCM4bitDecoder
and StereoADPCM4bitDecoder objects and their public ReadVagStream methods.
It needs no game assets, output device, GPU, Artery service or original banks.
Its source follows the existing audio_core fixture's empty application Xt
registration boundary, without substituting a decoder or streaming callback.

For each channel count1/2 and every source alignment0–15, the fixture requires
a source pointer above4GiB. Authored predictor0/shift0 packets have analytically
known signed samples and distinct packet/channel contents. Checks cover:

- An unaligned partial packet followed by the next33/66-byte packet pair.
- Exact consumed samples/returned position and independent left/right output.
- Original loop boundary and predictor/fader reset through a second read block.
- End-of-stream zero fill and an already exhausted stream with no consumption.
- Untouched output guard samples and immutable source/padding bytes.

The buffer explicitly owns and initializes enough aligned fetch padding; this
test does not claim the original decoder validates truncated/corrupt VAG data.
It exercises actual stream decoding, not the entire WaveServer/bank/FillBuffer
pipeline. Predictor0 vectors intentionally isolate pointer, stride and channel
selection; exhaustive predictor-quality validation is outside this fix.

Private production+fixture compilation, actual original-audio closure linking
and CPU runtime pass in Debug, Speed, DebugUBSan and SpeedUBSan (all4 exit0).
Logs/objects are under `_work/build/macos/physics-agent-adpcm-<configuration>/`;
the private driver is `_work/build/macos/physics-agent-adpcm-build.py`. Scoped
`git diff --check` passes. No shared build or actual game run was started by
MAC169. Shared archives were read only while parent declared them stable.

Integration uses the original `blur_mac_audio_original` target and a private
include root `code/mercury/library/audio`, with no arguments/assets. The private
link overrides only the changed decoder object before the original archive;
normal integrated builds should use the newly built archive directly. The
fixture does not require force-loading the complete audio archive.

The next gate is the parent's coherent actual native game run. Separate
WaveStreamData/AdpcmWaveInfo scratch-buffer prefix alignment is MAC170; this
change neither edits those structs nor claims that separate ABI class solved.
