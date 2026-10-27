# Native DDS 2D single-read upload (MAC326)

The previous native DDS 2D path read the complete original file to validate its
header and payload, deleted that buffer, then reopened the same file and streamed
the mip rows for upload. This repeated filesystem, PAK decompression and
verification work. The new native path holds one complete validated file buffer
through upload and then releases it. This is an I/O reduction; it is not a claim
of lower peak resident memory or improved actual-game frame rate.

## Validation and ownership

`MetalImageLoading.cpp` factors the existing DDS validator without changing its
format, dimensions, mip bounds, caps, pitch or payload rules. Existing generic
DDS metadata and volume/cube helpers retain their original reader/behavior. Only
`DEVICE_METAL` DDS handling in `CreateTexture2DFromFile` selects the new private
`Metal::CreateDDS2D` transaction. BMP, TGA and Windows/console paths are unchanged.

The private DDS reader selects the same original filesystem: the installed
logical/default filesystem for ordinary game paths, or the physical filesystem
for an explicitly selected app shader-resource path. It uses the same default
read access and critical Open parameters. Secure PAK reads therefore remain
inside the original file implementation and its verification policy. No raw
physical-file shortcut, hash bypass, noncritical fallback, retry or second open
was introduced. The original `I_BinaryStream::Read` facade returns a bool for
complete success; it is not a byte count. Its underlying partial-read handling
and failure propagation are preserved.

The file and its uninitialized byte array have RAII ownership during the one
complete read, including a thrown critical-reader error. The existing U32 file
length ceiling is checked before allocation. The shared validator transfers that
one array to the upload owner; it is never cloned or converted into a second full
image. Each mip uses actual tight format pitch and block-row count, copying rows
into the mapped native staging pitch with checked source bounds. L8 stays one
byte per texel, BGRA stays four, and compressed tail mips retain complete blocks.
The retail-style stale L8 pitch exception still requires the exact complete tight
payload; arbitrary padded or trailing layouts are not guessed.

An already-created destination is rejected intact. New GPU creation is inside
the transaction: false, null map, invalid staging pitch or an exception retires
any texture published by this call. A failed unlock discards the exclusively
owned texture/surfaces/staging rather than retrying the upload; command buffers
retain submitted native GPU resources through completion under the unchanged
lower ownership policy. The generic resource-tracker allocator's own exception
behavior is not redesigned by this DDS change.

Peak temporary image ownership is **one complete U32-sized original file** held
through all mip uploads, plus the lower backend's existing staging/resources.
The old streaming upload released its validation array earlier, so this tradeoff
can increase overlap between that CPU buffer and GPU staging. Existing maximum
dimensions and per-resource/device limits remain unchanged. This patch adds no
new global cache or retained decoded-image copy.

## Fixture and control

`dds_single_read_test.cpp` uses the real tracked native engine device and public
DDS file helper. Its authored `I_FileSystem` counts actual backend opens,
underflow/read requests, bytes, critical parameters and live file owners while
the original Read facade performs the load. Valid images require one open, one
complete backend transfer of exactly the file length, and one close. This does
not mean a compressed secure PAK performs only one OS read internally.

Actual GPU mip readbacks compare independently authored tight bytes for odd-sized
BGRA/L8 chains, numeric L8, a 64×64 ghost-mask-style stale-pitch L8 image, and
DXT1/DXT3/DXT5 chains with odd tail mips. Those are BC1/BC2/BC3 respectively;
ATI2/BC5 remains unsupported. Odd BC root dimensions retain their existing native
creation rejection. Malformed headers/masks/caps/levels, truncated payloads,
ambiguous stale-pitch payloads, short facade reads, throwing reads, oversized
reported lengths and missing files must leave file/GPU owners retired and permit
a healthy retry. Existing luminance sampling/graph tests continue to cover LLL1
view behavior. Genuine GPU staging-allocation failure is not artificially forced
through a new production hook; that map-failure cleanup has source coverage only.

Private compilation of the two production TUs and fixture passed in Debug,
Speed, Debug UBSan and Speed UBSan (12/12). The zero-initialization removal in the
reader separately passed four Utils probes, and the final representative-count
output passed four fixture probes. Root subsequently ran the real GPU fixture
successfully before that count-output refinement; coherent final integration
remains owned by root. No hardware/device runtime was run by this
agent. Target linkage is `blur_mac_display2_metal`, with no arguments, assets or
shader files; use the serialized hardware test lease and muted test environment.

Pristine pre-change files and hashes are pinned under ignored
`_work/build/macos/dds-single-read-agent/pristine` and `pristine.json`. After
coherent archives are stable, root can build an old-body control with:

```sh
python3 _work/build/macos/dds-single-read-agent/build_negative.py --configuration macos-release
```

This copies the lower archive privately, replaces only its original Utils and
MetalImageLoading objects, and links the same functional fixture with an extra
pre-assert diagnostic count line. It does not modify production archives or run
the executable. The expected first valid BGRA case reports two opens, six
backend reads, 272 transferred bytes for a 200-byte file, then exits 1 at the
one-owner/read assertion; normal cleanup runs before later throwing-reader cases.
The fixed path must report one open/read and 200 bytes. Expected counts are a
control plan until root executes and records the private differential.

## Actual-performance attribution remains open

The reduced-read GPU fixture is not an actual-game speed result. The combined
candidate's matched quiet race measurements regressed relative to the earlier
accepted baseline, so it is not accepted as a proven whole-game optimization.
A subsequent source review found identical DDS format/flags/mip metadata and
lock/upload behavior, with no persistent byte buffer or per-draw DDS hook.
Temporary overlap can still affect allocator high-water marks or native staging
residency indirectly; absence of a persistent pointer does not settle causality.

For one controlled DDS-only ablation, the private MAC336 raw full app keeps the
current final334 context, NativeHost and all other link inputs, replacing only
`Utils.cpp.o` and `MetalImageLoading.cpp.o` with their pinned pre326 objects. Its
manifest checks those are the only two changed lower-archive members, all shared
link inputs remain unchanged, and all 693 copied resources match. The builder
invokes only the captured link command, with no Ninja post-build/shared writes
or game/device execution. Root owns staging and the matched actual run.

A separate metadata-only inventory finds 139 DDS entries in the ignored original
root.pak, totaling 24,773,708 logical bytes; its largest single DDS is 2,097,280
bytes and belongs to DontShip content. Known source-selected ghost-mask, noise
and shock paths are 4,224, 8,320 and 87,508 bytes respectively. This is a bound on
that archive's inventory, not evidence that all entries load during the race.
The inventory reads no DDS payload or samples and makes no authenticity claim.
The secure-FAT gamedata archive has no DDS entries. Its authenticated Xt path
and the game mount's existing critical/security policy are unchanged.


The root-staged MAC336 ablation subsequently completed the original frontend,
20-car race, both quiet driving/no-input windows and normal exit (0,312.278s),
with source save unchanged and sound muted. The pinned old DDS objects yielded
48.560 submissions/s no-input and35.883 driving, versus final334's43.807/57.622.
The ablation's maximum no-input interval was1018.776ms. These mixed sequential
results do not establish a reliable DDS FPS effect or exclude a regression;
acceptance is the exact-format/security/lifetime checks and one original read,
not a whole-game speedup. See [the combined table](optimization-host-acceptance.md).
