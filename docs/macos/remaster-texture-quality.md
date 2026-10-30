# Faithful texture quality: authored anisotropy (MAC356)

This is an optional remaster image-quality feature, not a recovered retail bug
fix. It changes neither the art palette nor the textures, meshes, lighting,
shaders, colour spaces, motion/AA, simulation or render resolution. Original
Blur filtering remains available. Actual oblique-road improvement and GPU cost
must be qualified separately from a synthetic sampling oracle.

## Source contract

The upper FX sampler loader preserves filter modes and maximum anisotropy in
`FX::SamplerStateManager::CreateSamplerState`. Native `C_ContextBase` forwards
its per-slot maximum to the lower context for both pixel and vertex stages.
Lower callers can alternatively use `C_Context::SetMaxAnisotropy`. That global
default is1; it is not an existing global16x quality mode. A sampler whose
mag/min mode includes anisotropic already receives its requested native maximum.
Existing higher authored maxima must not be reduced.

Apple defines `MTLSamplerDescriptor.maxAnisotropy` as the sample maximum for
anisotropic footprints, with supported values1..16 and default1.
[Apple primary documentation](https://developer.apple.com/documentation/metal/mtlsamplerdescriptor/maxanisotropy).
A larger maximum can retain detail in a footprint that is long along one axis;
it can also cost additional texture work. It is not a higher-resolution asset,
a sharpen filter, or a guaranteed improvement for every material.

## Reversible selection

`BLUR_MAC_TEXTURE_ANISOTROPY` is read once when the context is constructed:

| Value | Policy |
| --- | --- |
| Unset, empty or `authored` | Preserve original per-slot/global maxima exactly |
| `4`, `8`, `16` | Apply that minimum only to eligible authored anisotropic samplers |

An eligible sampler already requests anisotropic minification or magnification,
has no point mag/min axis, and has point or linear mip filtering enabled.
Ordinary point, ordinary linear, mixed point-axis, and no-mipmap samplers are
unchanged. The feature never converts point to linear, enables mipmaps, changes
mip interpolation, or lowers an authored maximum. Both stages use the same
private `MetalTextureQuality.h` policy. Unsupported/malformed values throw a
clear configuration error; they do not silently choose a quality setting.

The lower-library default is authored. Root may enable16 for an isolated game
remaster launch only after full GPU/actual qualification. An explicit
`BLUR_MAC_TEXTURE_ANISOTROPY=authored` is the original-mode override. This first
implementation adds no in-game menu or profile/save schema field. Changing the
environment after device/context creation does not change a live scene.

Only the effective maximum used for native sampler creation changes. It occupies
the same existing eighth word in the exact sampler cache key; inserted keys
remain independently owned and scratch-key reuse remains intact. Requested
sampler state is not mutated. Addresses, gamma selection, borders, min/mag/mip
filters, signed bias, explicit-level clamps and current unsupported-gradient
checks retain their existing paths. No surface/queue/wait/cache policy, shader
math or experimental tile-pass default changes.

## Bounded evidence

`BLUR_MAC_TEXTURE_QUALITY_STATS=1` enables one context-retirement line:

```
blur2pc.texture_quality mode=16 requests=... eligible=... promoted=... eligible_max_requested=... eligible_max_effective=...
```

`requests` counts sampler selections, not frames or unique materials. `eligible`
counts requests whose authored filter modes qualify. `promoted` counts maxima
actually raised by the selected floor. Maxima refer only to eligible requests.
These are CPU selection counters; they do not identify roads, prove a successful
pixel draw or measure GPU time. Diagnostics are off by default and add no
per-draw allocation, timestamp or file output. With authored mode and diagnostics
off, the helper returns the existing maximum before classifying filter fields.

## Authored GPU oracle and acceptance limits

`tools/macos/texture_quality_test.mm` embeds authored shader bodies/metadata and
requires no original assets or generated shader headers. A256×256 texture has
eight black/eight white rows and an independently box-filtered mip chain. A
16:1 sample footprint is long along the constant-colour axis; the independent
minor-axis reference remains alternating bands. The fixture compares interior
stripe error/contrast for authored maximum1 and selected16. A positive result
must show actual retained detail, not merely a descriptor or counter change.
GPU-specific anisotropic implementations need not produce bit-identical output
to another GPU, so the detail test uses explicit bounded error/contrast metrics.

Each4/8/16 selected result must also be byte-identical to the same device's
explicitly authored equivalent maximum. Authored16 must stay unchanged under
floor4. Ordinary point/linear, no-mipmap and mixed point-axis images must remain
byte-identical. Tests cover per-slot/global maxima, real vertex/pixel sampling,
explicit mip/bias/border, strict option parsing, device recreation and diagnostics
off. These equivalence checks preserve current sampling semantics; they do not
claim universal Direct3D/Metal filtering equivalence.

Source/helper/fixture review is complete and frozen for root integration.
Pure metadata-fragment and independent box-mip arithmetic checks pass; these are
not native shader/GPU results. Root owns four-config compilation, strict GPU
runs and actual game image/performance acceptance, currently pending. The exact original
Context and SHA are retained under ignored
`_work/build/macos/occlusion-agent-texture-quality/pre356/`; production shaders,
surfaces and the previously playable build are untouched by this source change.

Actual acceptance must first establish nonzero eligible/promoted counts, then
compare road markings/asphalt at matching camera pose, mip transitions during
steering, wet-road and vehicle reflections, alpha textures, and HUD readability.
A quiet matched GPU-time/frame-interval comparison must report the quality cost;
more samples are not a performance optimization. If the selected materials never
request eligible anisotropy, this option cannot improve them and should not be
advertised as an overall texture remaster.

## MAC380 final qualification

Final Debug/Speed155/155, six focused lower UBSan checks and14 Metal-validation tests pass, including the real GPU independent band oracle. On this GPU authored MAE127.5/contrast0 becomes enhanced MAE0/contrast255 for that deliberately controlled footprint. Actual364 completed original normal exit with17,834,993 context-lifetime requests,1,353,719 eligible promotions and eligible maximum8→16. These are lookup counts over the run, not unique textures or road-only coverage. Final381 tests the compiled16/authored defaults with real muted audio, driving and ordinary exit0. The earlier pending-handoff paragraphs are historical. Exact cross-run camera/AI identity and universal wet-road/material improvement remain unproved. See [full acceptance and cost limits](faithful-remaster-acceptance.md).
