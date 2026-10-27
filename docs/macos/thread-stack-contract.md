# Native desktop worker stack contract (MAC111)

The actual MAC108 Brighton1/BMW run reached original World and RaceLogic Ready/Running and Lua Race loaded, then failed before rendering proof: JQ Worker0 hit its stack guard entering `Gravity::CollideMesh`. This is a native thread allocation contract error; it is not evidence of a recursive physics algorithm.

## Source authority and mismatch

- `common/system/source/scheduler/jobqueue_xenon.cpp`: `_Start` requests 65536 through the original TL API, with only suspended creation. The source uses the same CPU queue for PC and native.
- `common/system/include/system/scheduler/core/tl_thread.h`: original PC `tlCreateThread` forwards `stacksize` directly to `CreateThread`; its default is 32768. It does not add the reserve-size flag.
- `common/core/source/threads/Thread.cpp`: original Windows `MT::Thread::Start` likewise forwards the size using only `CREATE_SUSPENDED`.
- No executable `/STACK`, `StackReserveSize` or `STACKSIZE` override was found in the current source build definitions. The desktop linker default is 1 MiB. This is the recovered source/default contract, not a measurement of a supplied retail PE header.

Microsoft documents that, without `STACK_SIZE_PARAM_IS_A_RESERVATION`, the requested bytes change the **initial commit** within the executable's default reserved stack. The default linker reserve is 1 MiB; requests at least that large round the reserve up to a whole MiB. See [Thread Stack Size](https://learn.microsoft.com/windows/win32/procthread/thread-stack-size) and [/STACK](https://learn.microsoft.com/en-us/cpp/build/reference/stack-stack-allocations).

Darwin `pthread_attr_setstacksize` specifies total stack capacity. The previous native wrapper used each small PC request as that total, limiting scheduler workers to 64 KiB instead of their original desktop reservation. MAC108's crash map showed only 96 KiB for that worker, adjacent to a 16 KiB guard; its `___chkstk_darwin` caller was the first, nonrecursive CollideMesh invocation.

## Native change and scope

Native `MT::Thread::Start` now reserves at least 1 MiB for all default/small requests and rounds larger requests up to whole MiB, followed by Darwin page alignment. This applies consistently to original MT users, TL users, and scheduler CPU workers. No Windows implementation, simulation cadence, EPA storage, collision algorithm, thread priority, job policy or guard-page mechanism was changed. This preserves available capacity; Darwin controls demand backing rather than emulating Windows commit accounting.

Compiling the actual Debug `SPUScene.cpp` with its original CMake flags plus `-fstack-usage -S` reports a 61904-byte fixed CollideMesh frame, with dynamic allocations. Its source-local EPA, 1024 pointer stack, 64 EPA facet buffer and result storage total 78208 bytes with native widths. The actual crash was at the dynamic allocation probe on line 1464. Deeper collision call-chain maximum and future workloads remain subject to actual game validation; this is not an arbitrary per-function capacity increase.

## Focused verification

`tools/macos/thread_stack_test.cpp` requires no assets, command arguments or GPU. Link the original Gravity and native system runtime targets. It checks actual pthread-reported stack capacity for original MT requests 0, 1, 32768, 65536, 1048576 and 1048577, including joined-object restart, and checks suspended TL lifetimes/return values. A source-sized volatile local workload touches and verifies the complete 78208-byte storage in every variant, including optimized builds.

It then starts the original scheduler twice, uses a worker-only policy, and executes the real `Gravity::CollideMesh` entry with a constructed original zero-pair query. The original EPA/alloca frame runs and returns normally; no mesh/body pointers are invented and no collision or race completion is claimed. It also verifies normal queue teardown.

Debug, Speed, DebugUBSan and SpeedUBSan private executions all pass (four exit0). Rebuilding the exact previous native total-stack implementation privately makes this same fixture fail safely at its capacity assertion with exit1, before touching the undersized storage. The change is therefore covered by a direct differential regression. Actual Brighton/BMW rerun remains parent-owned and necessary for gameplay proof.
