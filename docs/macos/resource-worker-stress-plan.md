# MAC225: bounded reproduction and live thread evidence plan

This prepares a follow-up to the unexplained218 Speed World fixture timeout.
It has not run a GPU/Artery fixture. Root's long normal-career game owns that
runtime slot; no queued background stress process has been started.

The unchanged optimized `blur_mac_world_transaction_test` exercises its actual
PAK/GPU postprocess false/throw rollback and worker file requests. The private
runner is `_work/runtime/research/world_transaction_stress.py`. Its default
`--plan` only reads executable metadata and prints the intended paths. Explicit
`--run` is required to launch any fixture. It takes the existing native service
lock nonblocking and refuses to launch while another run owns the slot. Do not
wrap it in `run_exclusive.py`, because that would nest the same lock.

## Bounded run after root releases the slot

Suggested command (not executed during preparation):

```text
python3 _work/runtime/research/world_transaction_stress.py --run --cycles 10 --capture-lldb --output _work/runtime/verification/mac225-stress-a
```

Each cycle starts a fresh copy of the current optimized executable with the
original data root and optimized source-built shader root, strict Metal
validation, and an independent scratch directory for its authored worker files.
The fixture source, CTest90-second timeout and original failure injection are
unchanged. Runtime records the actual executable path/size/mtime and every
child PID, exit, elapsed time, output and detected assertion markers.

The additional private per-cycle watchdog is10 seconds, including diagnostic
time. At5 seconds a still-running cycle triggers a bounded1-second native
all-thread sample. Optional LLDB attaches only to that owned PID for a bounded
data-only snapshot, then detaches. The runner kills only its child process group
before the total watchdog deadline and stops the series. A sampled/interrupted
cycle is not counted as an untouched passing cycle, even if it later exits0.
Missing/partial diagnostics remain explicit rather than implying proof.

`world_transaction_capture.py` uses only LLDB SB reads: all thread stacks and
available Resource manager/thread, AsyncFile and native request fields. It does
not evaluate target expressions or call resource/IO methods. Optimized-out
variables, unreadable fields or an expired capture budget can limit the record;
the native sample and raw LLDB transcript are retained alongside any JSON.

The output directory must be new. Existing failure artifacts are never replaced.
Passing fresh cycles would be additional bounded evidence, not an explanation
or erasure of the initial218 timeout. If a slow/stalled process recurs, use its
actual stacks/queue/IO values to distinguish pause checkpoint, accepted close,
main update, or Stop/Join/blocking-mutex candidates from222 before changing code.

## Preparation checks and limits

Both private scripts passed Python syntax compilation. CPU-only self-test
launched and reaped only an isolated Python sleep surrogate to check owned
process-group termination, and checked that an exhausted diagnostic budget
launches no subprocess. `--plan` printed the frozen optimized fixture inputs.
No actual World fixture, GPU service, Automation peer or Artery process was
launched by these checks. No production, test source, build archive or timeout
was changed. A real reproduction/thread proof is pending explicit slot release
and execution by the root task.
