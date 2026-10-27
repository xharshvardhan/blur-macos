# Original PC dynamic geometry policy (MAC156)

Actual frozen149 diagnostic MAC151 stops in ToPrimitiveType2 with
E_PrimitiveType_QuadList, called by DynamicVertexBuffer::DrawPrimitiveBlock
through T_DynamicVertexBuffer<SpriteRotating>::Render. The captured queue has a
real vertex buffer and no index buffer. This is not a Metal triangle conversion
problem: the native build selected original console geometry policy.

## Paired count and writer contract

Eleven particle shape metadata headers select one quad/four vertices on console
(or six quads for Cube), and two triangles/three vertices on PC (12 for Cube).
Thirteen corresponding CPU writer sources contain the matching original PC
vertex duplication. For a rectangle, the sequence is0,1,2,2,3,0 with position,
colour, atlas mapping and animation coordinates preserved. EffectInterface
computes actual reserved vertex count as primitives times vertices. Selecting
TriangleList while retaining four vertices would violate that reservation and
lose a corner. Native now selects the existing complete PC branch in all those
metadata/writer guards; Windows and console geometry is unchanged.

The actual dynamic buffer had a related console memory policy: GetAddress and
no Unlock instead of PC Lock/Unlock. Native now selects the same PC pair. This
lets the existing checked backend preserve its real mapped-resource lifetime;
no lower topology or mapped-resource guard is relaxed.

The particle manager's UseSoft predicate explicitly returns false for original
PC; native now shares that selection instead of enabling the console-only
material policy. The other shape shaders/quality selection remain the original
code. No new soft-particle approximation or shader replacement is introduced.

## Other dynamic QuadList producers

A scoped complete dynamic subtree inventory finds six other light shape
metadata/inline-writer pairs: Beam2d/3d, Cross2d, Quad2d/3d and Decal3d. Each has
the same existing PC count/duplicated-triangle writer distinction. Native now
selects that full PC branch. Minimal.cpp's debug cube has the same two paired
count/writer selectors and is included. Track and tracer contain no QuadList
producers. The original triangle-only shapes remain unchanged. Remaining
QuadList tokens in this subtree are now in console-only branches. No direct
quad reinterpretation is added to ToPrimitiveType2 or lower Metal.

## Validation and limits

New tools/macos/particle_geometry_test.cpp is a no-argument Game fixture using
the same actual Game/bootstrap-first/generated-Lua link closure as
particle_trail_test. It uses a real native device, real Scheduler lifetime,
real camera view and existing TextureCoords singleton. It verifies:

- All11 particle and six light metadata count/topology pairs.
- Actual SpriteRotating effect initialization yields TriangleList and six
  reserved vertices per element.
- Original sorted and block CPU writers produce two particles' exact positions,
  colours, flipped UV mappings and animation coordinates, with returned byte
  count and untouched guard record.
- An actual light Quad3d writer produces six complete vertices, repeated
  diagonal endpoints and untouched guard record.
- Original dynamic buffer Lock/Build/Unlock/Draw on three frames yields both
  expected rectangles and cleared gaps in real GPU readback under Metal
  validation.

All18 source probes (17 original source TUs plus fixture) across Debug, Speed,
DebugUBSan and SpeedUBSan pass. Private standard full Debug closure passes
strict Metal and exits0, including static retirement. Runtime logs and probes
are under _work/build/macos/occlusion-agent-particle-geometry. Earlier fixture
attempts omitted Scheduler/camera/singleton prerequisites and failed before
these assertions; those were fixture preconditions, not production fixes.
Other three Game runtime archive closures are stale, so no four-configuration
runtime claim is made. Coherent actual game execution remains root's separate
gate; this does not establish a completed gameplay frame, race or clean game
exit.
