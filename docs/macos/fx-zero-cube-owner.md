# MAC185 — Required PC 2D-to-cube compatibility texture owner

Actual MAC180 reached a cube sampler with a bound 2D texture and correctly
failed the strict native lower texture-type guard. MAC181 owns the upper FX
adapter and reflection policy. Its authority is the measured original PC result
recorded by NativeFX11.cpp and docs/blur1pcdx11/analysis.md T127: sampling any
bound 2D texture as a cube yields RGBA 0000 in every direction. A valid cube must
remain unchanged; NULL/unbound and other texture mismatches are distinct policies.
This owner repair does not reinterpret the lower guard or create a generic
texture fallback.

## Required resource transaction

Native FX::System::Initialise now acquires three real owners locally: the existing
white 2D default, the existing zero float4 vertex volume, and a new 1x1 single-level
A8R8G8B8 cube used only for that compatibility case. It checks allocation success
and IsCreated, maps/writes/unmaps each of the six cube faces to U32 zero, including
alpha, then publishes all three globals together. GetPC2DToCubeTexture returns
the actual cube GPU wrapper only after complete initialization; before/after
initialization/shutdown it returns null. Original white and zero-volume contents
and all nonnative branches remain unchanged.

Each local wrapper has a unique owner with the correct actual device destroy
operation. A false create, null map or thrown operation anywhere before the final
publication unwinds all three pending wrappers. In particular, failure at the
sixth face cannot publish the five earlier faces or retain the already acquired
white/volume/cube owners. No write follows a null map; only a successful map is
unlocked. Resource naming also occurs before publication. Repeated live Init is
rejected; failed Init leaves no published owners; repeated Shutdown is empty;
normal shutdown retires the cube, volume and colour while the real device lives.
No production fault-injection hooks were introduced.

## Verification

System.cpp and the extended existing fx_test.cpp pass all four object configurations
(8/8). A private replacement of only System.cpp.o at the standard force-loaded FX
archive position links and runs the actual FX fixture with Metal API validation.
The process and static finalization exit zero. It verifies a real uncreated-device
required initialization rejection/no publication, a successful retry, exact existing
white/zero-volume contents, actual cube type and all six exact zero face texels,
tracked texture count baseline+3, repeated live-init rejection, actual existing
shader/cache/GPU-draw tests, repeated shutdown to the tracked baseline and a second
complete initialization/retirement cycle.

Logs: _work/build/macos/fx-zero-cube-agent/runtime.log and configuration source logs.
Late allocation/map exceptions and final-face map failure were not artificially
injected; rollback for those branches is source/RAII evidence. Sampling all six
directions in both shader stages and source-type discrimination belongs to the
separate MAC181 mixed-resource GPU fixture. The owner's successful readback and
lifecycle fixture alone is not a completed original-game frame/race proof. Parent
owns coherent MAC182 integration and actual next-game verification.
