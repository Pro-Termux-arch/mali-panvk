#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reuse pinned MIT PanProbe requirement gates; adapt only G78 identification.

JimVulkan v1.6 calls the GPU PanVK-G78, so name-only 'Mali' selection misses it.
The generated copies use vendor/type/G78 identity; no feature check is relaxed.
These are query-only preflights, NOT successful DXVK/VKD3D execution tests.
"""
import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
PIN = "dea5b138b62050181a4f50dbd755a5ac1aaf05d3"


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--panprobe", type=Path, required=True)
    p.add_argument("--ndk", type=Path, required=True)
    p.add_argument("--out", type=Path, default=ROOT / "build-panprobe-checks")
    p.add_argument("--shared", action="store_true", help="app-local query modules using panprobe_main")
    a = p.parse_args()
    actual = subprocess.check_output(["git", "-C", str(a.panprobe), "rev-parse", "HEAD"], text=True).strip()
    if actual != PIN: p.error("PanProbe revision mismatch")
    if subprocess.check_output(["git", "-C", str(a.panprobe), "diff", "--stat"], text=True):
        p.error("PanProbe source must be unmodified")
    a.out.mkdir(parents=True, exist_ok=True)
    cc = a.ndk / "toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android31-clang"
    old = 'if (strstr(p2.properties.deviceName, "Mali")) {'
    new = ('if (p2.properties.vendorID == 0x13b5 && '
           'p2.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU && '
           'strstr(p2.properties.deviceName, "G78")) {')
    for name, source in (("dxvk_reqs", "tests/dxvk/vulkan/dxvk-reqs/dxvk_reqs.c"),
                         ("vkd3d_reqs", "tests/vkd3d/vkd3d_reqs.c")):
        text = (a.panprobe / source).read_text()
        if text.count(old) != 1: p.error(f"{source}: unexpected selection code")
        generated = a.out / (name + ".c")
        generated.write_text(text.replace(old, new))
        flags = ["-shared", "-fPIC", "-Dmain=panprobe_main", "-Wl,-z,max-page-size=16384"] if a.shared else []
        output = a.out / (f"lib{name}.so" if a.shared else name)
        subprocess.run([str(cc), "-std=c11", "-O2", "-Wall", "-Wextra", *flags, "-I" + str(ROOT / "include"),
                        str(generated), "-ldl", "-o", str(output)], check=True)
    shutil.copyfile(a.panprobe / "LICENSE", a.out / "PanProbe-LICENSE.txt")
    shutil.copyfile(a.panprobe / "NOTICE.md", a.out / "PanProbe-NOTICE.md")


if __name__ == "__main__":
    main()
