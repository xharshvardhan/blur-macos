# MAC233 private keyboard transport resilience

The ignored `drive_keyboard_guarded.py` retains MAC224's finite ordinary-key
planner and guards, but records CLI transport details and sends each key in a
separate command with a short game-time execution deadline. Production and the
old operators are unchanged. Root owns the live MAC232 continuation.

## Source-backed classification

`tools/macos/automation_client.cpp:58-70` constructs a connection and requires
both greeting and AutomationVersion before the CLI calls ExecuteLuaCommand.
The server `system/source/automation/Automation.cpp:93-137` sends
AddAutomationPeer failed only when that connection was never registered;
NetworkUpdate at173-210 can execute commands only for registered live peers.
Thus that exact bootstrap rejection, inability to connect/allocate/start, or a
greeting timeout prove that the requested Lua was not dispatched. They permit
a new decision only after another fresh safe snapshot.

Send failure, disconnected service, command-reply timeout and process timeout
have ambiguous phase/execution. They do not permit a blind key retry. The new
helper retains exit status, stdout, stderr, elapsed duration and classification
for snapshots and each key attempt. Exact tagged Lua output is separate evidence
from transport acknowledgement.

For legacy MAC232 KJ247, log16362 shows completed KP247/end and its peer removal.
An accepted concurrent peer then prints root's successful STATE9; the second
TryAddPeer has no OK and leaves immediately. With the known owned driver and
root probe as the two callers, this maps the driver's failure to bootstrap
rejection before LuaSend. No KJ247 tag exists. The old CLI status was discarded,
so this attribution uses that source and known-client correlation, not an
invented transport error/command ID in the old log. Root reviewed the fence
before continuing its live session.

## Execution and expiry protocol

Each command reads existing VehicleState and prints its unique tag, actual
execution raceTime and Hold result. Hold executes only while raceTime remains
below the fresh observed value +1.5 simulated seconds. The complete command is
checked against the original127-byte limit; it contains one ordinary finite
Q/A/Left/Right Hold, never a progression/pose/AI/result setter.

Normal accepted holds retain MAC224's post-ACK fence, now anchored relative to
the returned execution raceTime. For uncertain execution:

1. Stop issuing all keys; never reissue the ambiguous command.
2. Read fresh safe advancing telemetry until raceTime is beyond its deadline.
3. From that post-deadline/post-input-poll observation, wait the full requested
   hold duration plus .08s wall time, and require another advancing sample.
4. Only then resume fresh decisions. If no such proof arrives within15s, stop.

Within this monotonic owned race, any later delayed command fails its deadline;
any earlier accepted hold has expired after the full post-poll wall fence.
An exact expired/nil-state tag proves a skipped command. A Hold false before
its deadline leaves native key ownership unknown and stops input. Multiple or
malformed tag results are treated as uncertain. This protocol does not infer
release from Lua ACK, rendered frames or a missing tag.

PLAYER0/nonAI flags, layout0 preflight, controls/AIR/on-route/wrong-way, finite
indices/heading, snapshot age<=1s and advancing-time guards remain. The helper
sends no input while a state is unsafe/unavailable, observes Finished without
creating it, and has no quit or progress command. Original physical key state
is untouched. Use only one driving operator and the owned `*-game.log`/matching
probe-status pair; no external race reset is part of this operator scope.

## Interface and verification limits

```sh
python3 _work/runtime/research/drive_keyboard_guarded.py \
  --game-log /absolute/path/to/mac232-game.log \
  --seconds 2400 --drive --layout0-confirmed
```

Pure replay uses852 actual MAC183/MAC215 telemetry records. Command capacity,
deadline acceptance/skip/rejection, ambiguous/multiple tags and full post-poll
wall-expiry tests pass. Seven mocked invocations exercise the real transport
classifier and preserve stdout/stderr: AddPeer rejection/greeting timeout,
disconnect, ambiguous send/reply timeout, process timeout and dispatch ACK.
Syntax checks pass. These tests make no IPC call or dynamics prediction;
ignored `mac233/transport-pure-checks.json` records the transport cases.

Read-only audit of the first411 live root-owned resilient records found exact
accepted execution tags before their deadlines and correct no-input handling
of unavailable/reversing/adjacent-behind states. The audit found244 accepted key attempts and19
explicit guard skips, with no deadline/control/capacity violation. It observed no
transport reconciliation event yet, so adverse live transport recovery and natural
career finish/results remain unproven. Root alone owns runtime verification;
helper exit0 is not game completion evidence.
