# MAC170 — Native audio runtime scratch layout

Actual optimized MAC167 failed in memmove reached from ReadVagStreamStereo.
MAC169 owns the paired decoder fetch-alignment fix: native had selected a U32
console pointer cast rather than the full-width PC branch. This companion audit
made no decoder edits and found no further reachable pointer narrowing in the
bounded native stream/cache/request path.

Resident WaveBankReader allocations, WaveBankManager::GetWave's psData,
WaveStreamData::pData, cDecodeVagInfo::mSourceVag, DecoderDataInfo and
WaveStreamReadRequest::m_pDestBuffer retain actual pointer types. Stream offsets,
byte counts, sample positions, request IDs and bank fields remain legitimate U32
values. Common/Allocator already aligns with INTPTR. DSP component storage uses
sizeof-based runtime sizes. No bank field or serialized PC format was widened.

Other narrowing matches were classified: InstanceUpdate's current-value casts
are PLATFORM_SPU-only; PCM FetchData is SPU-only; vector_load_unaligned belongs
to _AUDIO_MATHS_SPU_; XMA memory alignment is Xenon-only. Frame::TempAlloc has an
old narrowing cast but no repository caller found, so it was not expanded into
this repair. Commented casts and numeric sample-rate conversions are not pointers.

## Repaired producer/consumer class

A separate concrete native ABI gap placed pointer-bearing WaveStreamData and
AdpcmWaveInfo arrays after a four-byte update-counter prefix. Both records now
require native pointer alignment. The old Object allocation and Instance/core
accessors agreed about +4, but agreement did not make the addresses aligned.
These arrays are runtime scratch, not PC file records.

Native audio_core.h now derives each prefix from sizeof(U32) and that record's
alignof. The counter remains at offset zero. The full counter/padding/array size
is rounded to the existing 16-byte component/core-span alignment. A static
assertion rejects future record alignment incompatible with that span contract.
Object::CreateMasterInstance uses those full sizes for both allocations, including
the ADPCM prefix exactly once. All three Instance accessors and both Core getters
use the matching record-specific prefix. Original Windows/console branches and
record layouts remain unchanged. No clamp, pointer remap or audio disabling was
introduced.

## Verification

Object.cpp, Instance.cpp and core/core.cpp pass all four object configurations
(12 probes). The no-argument audio_scratch_test.cpp uses the leaf audio closure,
without force-loading the archive, application data or online services. Real
Instance producers and Core consumers agree for arrays of one, two and seven
records; every record is aligned, including the last, actual Object size helpers
bound the writes, counter padding and inter-array canaries remain intact, and
native pointers retain their original values. It then drives actual streaming
StereoADPCM FillBuffer against the resident counterpart, comparing all nonzero
finite PCM samples and exact stream byte/update accounting. Current MAC169 decoder
objects are included in private links, rather than the old shared archive copy.

Debug and Debug UBSan private runtime tests pass with true process exit zero.
Four fixture object probes also pass. This exercises actual Instance/core
accessors and the shared allocation-size helpers, not a full Object::CreateInstance
resource-loading lifecycle. Root owns coherent standard integration and actual
original-bank game playback; those remain separate evidence.

## Separate observed failure-publication gaps

No speculative repairs were made to these independent original behaviors:
FileStream::SubmitLoadRequest/ReleaseBuffer ignore the LoadData boolean at
filestream.cpp:555/574 and can publish request bookkeeping despite rejection.
WaveStreamLoadingThread currently reports requested bytes rather than the actual
UnderflowContext::bytesRead at WaveServer.cpp:175. These are source-proven
failure/short-read publication concerns, not demonstrated causes of MAC167's
low-address memmove, and require separate owner/policy review.
