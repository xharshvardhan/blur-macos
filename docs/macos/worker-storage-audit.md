# Native scheduler worker storage (MAC115/116)

The native CPU scheduler supports eight **logical slots**, main0 plus workers1..7.
`common/system/source/scheduler/jobqueue_xenon.cpp::_Init` caps the number of
background threads at seven; `_Start` selects each logical slot from the enabled
processor mask. `jobqueue_kernel_xenon.cpp::jqWorkerThread` publishes that slot
through thread-local `g_iWorkerNumber`, which `GetWorkerID()` returns. Worker
ordinal6 can therefore have logical ID7. Sparse masks preserve their logical
IDs: reducing thread count alone is not a storage-capacity contract. Native
`jqEnableWorkers` already excludes main bit0 and masks against `JQ_CORE_ALL`.

| Consumer | Original storage/use | Native reachability and resolution |
|---|---|---|
| Particle random state | `C_Manager::m_Random` / `UpdateConstants::pRandom` originally seven elements | Observed actual worker7 null-random failure. MAC115 owns native eight-slot capacity and checked access across Spawn/Emitter/SuperSpawn. |
| Scene contexts | `SceneGraphMaxContexts.h` originally7; `C_SceneGraph::m_Contexts`; `SceneGraph.cpp::GetContext`, `RenderTask.h::GetContext` indexed by actual ID | Game's medium/lighting policies admit7. MAC116 supplies eight native contexts and one checked index helper for both accessors; legacy capacity remains7. |
| Audio object scratch | `ObjectManager.cpp::Init` allocated7 strides; `GetDynamicBuffers` returned main's buffer for ID>=7 | MAC116 allocates eight native strides and checks ID/initialization without remapping. Current Amax native audio policy admits4/5 only, so this was a latent engine-contract fault, not the observed particle crash. A valid different policy admitting7 previously aliased main scratch. Legacy behavior is preserved. |
| Particle block writer ID | `dynamic/Block.h::C_RenderNode::m_WorkerID` U16; 13 Generic update writers assign actual worker ID | IDs0..7 fit. Repository source search found declaration/initialization/writers and no downstream C++ readers or ID-indexed scratch derived from this member. No speculative change. |
| Scheduler alerts/job names | `g_WorkerAlert[8]`, `g_iCPUWorkerAlert[8]`, `g_pszJobName[10]` | Already accommodate0..7. Packed alert operations address correct eight-element storage. No resize needed. |
| Legacy XMem scratch | `XMemCompress.cpp` compression/decompression context arrays6; packet-XMem has old Xenon ID6 remapping | Inactive native paths: context arrays are inside `USE_XMEM_COMPRESSION`, absent from native PCH/compile definitions. `PakCompressPacketXMem.cpp` is absent from native compile_commands; retail PC raw/ZLib/packet-ZLib readers are active and converter-only switch cases select XMem. No native correction to proprietary dormant path. |
| Physics independent workers | RacingPhysics `Scheduler::Worker::workerId` names threads; `%6` hardware-affinity helper inside Xenon branch | Separate scheduler identity domain; no corresponding GetWorkerID-indexed fixed storage found. Native Mercury logical-slot capacity does not set this count. |

`jobqueue_mem.cpp` implements SPU local-memory/DMA utilities with CPU memcpy
paths; this audit found no native logical-ID-indexed six/seven-element scratch
pool there. Broader worker/thread-index searches also covered original profiling
and PS3/GCM helpers; the latter are console paths, not native consumers.

One separate dormant contract remains: `jqAssistWithJobs` creates temporary
worker structures for non-JQ threads but does not assign those native caller
threads a unique `g_iWorkerNumber` (TLS defaults0). It has **no call sites** in
this repository beyond declaration/definition. It must not be introduced for
concurrent worker-local scene/audio/particle work without explicit logical-slot
ownership. This audit does not change an unused API or claim arbitrary external
threads can safely share main's storage. Its atomic temp index returns the old
value, so index0 is correctly selected for the first temporary worker.

Verification source: `tools/macos/worker_storage_test.cpp` uses the actual native
scheduler, actual full SceneGraph and ObjectManager types, real Display/FX
lifetime, and seven simultaneous jobs pinned separately to slots1..7. It checks
context pointer uniqueness, audio stride/sample independence versus main0,
exact job IDs/counts, and invalid scene context IDs in public accessors. It has
no game data, original online service, progress mutation or gameplay evidence.
All three changed source objects compiled in Debug, Debug+UBSan, Release and
Speed+UBSan (12 probes); actual coherent runtime result is recorded in the ledger.

Search inventories/probe logs are private under
`_work/build/macos/worker-storage-agent/`. This source audit is bounded to
available source and native compile membership; it is not a proof about absent
binary-only modules.

The first integrated worker fixture exposed a separate completion-event lifetime
race (MAC118): all actual storage checks completed, but scheduler shutdown waited
on a worker still signaling an event which DestroyGroup had already retired.
The sampled native worker was blocked in Event::Signal's mutex; main was joining
that worker. Native completion and event destruction now share the existing
batch-pool lock, with global completion published after grouped event work.
Temporary-worker completion uses the same path. Event waits remain outside that
lock; nonnative flow is unchanged. Caller ownership still prohibits concurrent
pool resizing/late task publication/group destruction by unrelated managers;
this does not turn auto-reset event groups into general multiple-waiter objects.

With corrected scheduler objects, five independent actual Debug worker-storage
processes exited0, each checking all seven concurrent workers plus256 immediate
event-group retire/reuse cycles and complete shutdown. Scheduler and thread-stack
regressions separately exited0 in all four configs (eight runs). Source-only
object probes passed in all four configs for both the storage and scheduler
changes. This verifies storage/event ownership, not a completed game race.

Root subsequently rebuilt coherent Debug actual-game/fixture targets and
confirmed integrated CTest17/17 PASS under Metal validation, including this
worker-storage regression and existing ownership/PAK/effect tests. The actual
Brighton/BMW race verification is separate and remains root's runtime gate.
