#!/usr/bin/env python3
"""[Blur2PC] Queue native game/service runs sharing the original Artery port.

Use for game processes, CTest, and Automation service fixtures. Do not wrap an
Automation client: it must connect while the game owns the slot.
"""
import argparse
import fcntl
import json
import time
from pathlib import Path
import signal
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--timeout", type=float, default=180,
                        help="Child runtime limit in seconds after acquiring the slot")
    parser.add_argument("--started-file", type=Path, help="Optional owned-run PID/monotonic-time handshake after acquiring the slot")
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    if not command or args.timeout <= 0:
        parser.error("Provide a command and positive runtime limit")
    root = Path(__file__).resolve().parents[2]
    lock_path = root / "_work/runtime/native-service-slot.lock"
    lock_path.parent.mkdir(parents=True, exist_ok=True)
    with lock_path.open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        child = subprocess.Popen(command, start_new_session=True)
        def interrupted(signum, _frame):
            raise KeyboardInterrupt
        previous = {sig: signal.signal(sig, interrupted)
                    for sig in (signal.SIGINT, signal.SIGTERM)}
        try:
            if args.started_file:
                staged = args.started_file.with_name(args.started_file.name + ".tmp")
                staged.write_text(json.dumps({"pid": child.pid, "started_monotonic": time.monotonic()}))
                staged.replace(args.started_file)
            return child.wait(timeout=args.timeout)
        except subprocess.TimeoutExpired:
            print("Native service run exceeded its runtime limit", file=sys.stderr)
            return 124
        except KeyboardInterrupt:
            return 130
        finally:
            # Only the process group created for this owned run is terminated.
            if child.poll() is None:
                import os
                os.killpg(child.pid, signal.SIGTERM)
                try:
                    child.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    os.killpg(child.pid, signal.SIGKILL)
                    child.wait()
            for sig, handler in previous.items():
                signal.signal(sig, handler)


if __name__ == "__main__":
    sys.exit(main())
