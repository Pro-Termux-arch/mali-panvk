#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run only this project's isolated APK; verify identity and bounded presentation.

No root, run-as, log clearing, device permission change or global graphics state.
The ROM's run-as restrictions do not prevent PID-filtered logcat validation.
"""
import argparse
import os
from pathlib import Path
import select
import subprocess
import time

PACKAGE = "io.github.protermuxarch.exynosprobe"


def adb(*args):
    return subprocess.check_output(["adb", *args], text=True, timeout=30)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--mode", choices=("stock", "panvk"), required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--timeout", type=int, default=45)
    a = p.parse_args()
    if not 1 <= a.timeout <= 120: p.error("timeout must be 1..120 seconds")
    adb("shell", "am", "force-stop", PACKAGE)
    print(adb("shell", "am", "start", "-W", "-n", PACKAGE + "/.MainActivity", "--es", "mode", a.mode).strip())
    pid = adb("shell", "pidof", PACKAGE).strip()
    if not pid.isdecimal(): raise SystemExit("FAIL no unique test app process")
    proc = subprocess.Popen(["adb", "logcat", "--pid=" + pid, "-v", "raw", "-s",
                             "ExynosProbe:I", "PanVKSwap:I", "*:S"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = bytearray()
    deadline = time.monotonic() + a.timeout
    passed = False
    try:
        while time.monotonic() < deadline:
            ready, _, _ = select.select([proc.stdout], [], [], max(0, deadline-time.monotonic()))
            if not ready: break
            data = os.read(proc.stdout.fileno(), 65536)
            if not data: break
            output.extend(data)
            text = output.decode(errors="replace")
            if "exit:1" in text or "FAIL " in text: break
            if "exit:0" in text:
                passed = (f"PASS selected={a.mode}" in text and "G78" in text
                          and "PASS swapchain_lifecycle" in text)
                break
    finally:
        proc.terminate()
        proc.wait(timeout=10)
        proc.stdout.close()
        a.out.parent.mkdir(parents=True, exist_ok=True)
        a.out.write_bytes(output)
    print(output.decode(errors="replace").strip())
    if not passed:
        # Stop only our own test process if it failed/hung; never restart Android.
        adb("shell", "am", "force-stop", PACKAGE)
        raise SystemExit("FAIL identity/presentation/timeout; see captured test log")
    print("PASS isolated app driver selection, presentation, resize and swapchain cycling")


if __name__ == "__main__":
    main()
