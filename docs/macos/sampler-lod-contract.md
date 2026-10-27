# Native PC sampler LOD contract (MAC134)

The actual MAC135 before-unwind capture identifies the first required case:
`C_MirrorView::Render` → SceneGraph material draw → pixel sampler2, encoded bias16,
linear min/mag/mip filters, effective anisotropy1. The previous native guard
rejected this legal PC draw. This evidence is in the private
`_work/runtime/verification/mac135-game.log`; it is not a completed-race result.

## Source authority and translation

The original `D3D11States.cpp` and `D3D11GameDeviceNative.cpp` convert the signed
engine value to mip units by division by16. They set MinLOD0 and MaxLOD0 for
mip-filter None, otherwise MaxLOD=FLT_MAX. Sampler bias affects implicit,
instruction-biased and explicit-level sampling. Microsoft's
[D3D11 functional specification](https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm)
§7.18.11 and §22.4.18 describe addition before the sampler's LOD clamps;
[D3D9 texldl](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/texldl---ps)
and its vertex equivalent preserve this explicit-level behavior too.

The existing open-source compilers remain responsible for decoding and
translation: glslang/SPIRV-Cross for authored source, pinned MojoShader for
D3D9 bytecode. The shared native loader adds PC sampler state to their MSL,
using a balanced source parser and actual `[[sampler(N)]]` arguments. It does
not infer slots from texture names. A corpus audit found1854 sample calls in
the693 authored shaders, all using direct entry sampler arguments. All693
shader blobs then passed the actual modified engine loader and Metal compiler.
No authored shader blobs or original assets are rewritten.

The private per-stage float2[16] buffer contains actual bias/16 and MaxLOD.
The loader chooses an unoccupied constant binding below16, preserving authored,
alpha-test and HDR bindings and avoiding vertex streams16+. Compiled pipeline
reflection still determines actual upload allocation sizes. Each draw uploads
the current stage's state; sampler cache identity need not include bias because
it is a shader operand, not a descriptor field on the baseline backend.

- Fragment implicit sampling uses MSL bias(state).
- Existing instruction bias adds the sampler state without inventing a clamp
  on their sum.
- Explicit level adds state then clamps to PC MinLOD0/MaxLOD. This matters
  because Metal explicit-level operations ignore descriptor LOD clamps.
- Nonfragment implicit sampling uses the documented default LOD0 plus state.
- Isotropic explicit gradients scale both gradients by exp2(state), giving the
  same additive logarithmic LOD shift before sampler clamps.

Apple's [MSL specification](https://developer.apple.com/metal/Metal-Shading-Language-Specification.pdf)
§6.13 documents bias, level and gradient qualifiers and nonfragment default
LOD0. This integration uses baseline MSL2.3 and does not require the newer,
GPU-family-limited descriptor lodBias feature.

## Legacy explicit-level correction

Pinned MojoShader's Metal profile originally emitted TEXLDL by calling its
implicit TEXLD emitter, losing source.w. `tools/macos/patch_mojoshader.py`
creates an exact-context-checked build copy of that one profile with TEXLDL
preserving `level(source.w)`. The vendor checkout remains pristine; all register
formatting, decoding, other opcodes and projection handling remain upstream.
Both pixel and vertex authored DXSO tests execute the corrected instruction
through the same load-time sampler ABI.

## Explicit limits

Anisotropic explicit-gradient sampling with nonzero sampler bias remains a
precise unsupported draw error. D3D computes anisotropic footprint adjustments
before adding sampler bias; scaling gradients can cross a magnification
boundary and is not established as identical. Zero-bias gradient operations and
ordinary anisotropic implicit sampling retain their existing path. This limit
must not be described as complete D3D filtering equivalence.

Opaque sampler parameters in helper functions, unsupported sampling methods or
unrecognized LOD qualifiers are marked at load time. If such a shader is used
with nonzero affected bias or mip-filter None (PC MaxLOD0), the context rejects
before issuing the GPU draw. Default zero-bias/linear-mip behavior is retained. No current authored-corpus sample has
this limitation. The fixture verifies an actual opaque explicit-level helper's
zero-bias/linear draw, nonzero-bias rejection and zero-bias/MipNone rejection,
then retries a direct integrated operation with actual mip0 GPU pixels. Malformed source/binding exhaustion fails shader
creation with a diagnostic; no slot is silently substituted.

## Verification and remaining gate

`tools/macos/sampler_lod_test.cpp` requires only the native Display2 target,
no arguments/assets. Actual mip-sentinel GPU pixels cover positive/negative
and fractional state, signed extremes, instruction addition, explicit negative
and mip-None clamps, separate slots/rebinding, nested sample expressions,
authored buffer15 preservation, separate vertex state, isotropic gradients,
unsupported-gradient/provenance rejection and retry, and real pixel/vertex
DXSO TEXLDL. Debug, Speed, DebugUBSan and SpeedUBSan private binaries exit0
with strict Metal validation; both sanitizer runs halt on any UBSan error.
Source objects and fixture/profile compile in all four configurations.

Private logs are under `_work/build/macos/occlusion-agent-lod/`; the all-corpus
engine-loader result is `macos/all-shaders.log`. Root's coherent build and
actual MirrorView/race rerun remain separate gates. These focused successes do
not establish a visible complete gameplay frame, keyboard motion, natural race
finish or clean actual-game exit.

The MAC134 review extension covers MaxLOD0 even when bias is zero. Its pre-fix
opaque helper was accepted; this M4 Pro's NotMipmapped sampler selected mip0.
That observation does not establish a universal explicit-level clamp contract,
and no wrong-pixel result is claimed for that host case. The adapter now
truthfully rejects unintegrated helper MaxLOD state rather than depending on
that hardware result. No authored-corpus adapter changes were needed.
