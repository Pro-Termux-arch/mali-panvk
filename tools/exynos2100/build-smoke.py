#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build only the device probe, using an installed NDK and glslangValidator."""
import argparse
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[2]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("ndk", type=Path)
    p.add_argument("--out", type=Path, default=ROOT / "build-smoke")
    a = p.parse_args()
    a.out.mkdir(parents=True, exist_ok=True)
    src = ROOT / "tools/exynos2100"
    with (a.out / "shaders.h").open("w") as header:
        for shader, name in (("compute.comp", "compute"), ("triangle.vert", "vertex"), ("triangle.frag", "fragment")):
            spv = a.out / (shader + ".spv")
            subprocess.run(["glslangValidator", "-V", "--target-env", "vulkan1.0", str(src / shader), "-o", str(spv)], check=True)
            data = spv.read_bytes()
            words = struct.unpack(f"<{len(data)//4}I", data)
            header.write(f"static const uint32_t {name}_spv[] = {{\n")
            for i in range(0, len(words), 8):
                header.write(",".join(f"0x{w:08x}" for w in words[i:i+8]) + ",\n")
            header.write("};\n")
    cc = a.ndk / "toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android31-clang"
    subprocess.run([str(cc), "-std=c11", "-O2", "-g", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(ROOT / "include"), "-I" + str(a.out), str(src / "smoke.c"),
                    "-ldl", "-o", str(a.out / "exynos-smoke")], check=True)
    subprocess.run([str(cc), "-std=c11", "-O2", "-g", "-Wall", "-Wextra", "-Werror",
                    str(src / "kbase-probe.c"), "-o", str(a.out / "kbase-probe")], check=True)

if __name__ == "__main__":
    main()
