#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build a tiny isolated Android presentation probe, reusing SDK/NDK/JDK.

No Gradle, downloads, privileged installation, network permission or driver
replacement. External test/loader sources are pinned and their notices shipped.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[2]
PINS = {"panprobe": "dea5b138b62050181a4f50dbd755a5ac1aaf05d3",
        "adrenotools": "8fae8ce254dfc1344527e05301e43f37dea2df80",
        "linkernsbypass": "aa3975893d83ef1bc84c321ec60c65fbf1287887"}


def run(*args, **kwargs):
    subprocess.run([str(x) for x in args], check=True, **kwargs)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("sdk", "ndk", "jdk", "panprobe", "adrenotools"):
        p.add_argument("--" + name, required=True, type=Path)
    driver = p.add_mutually_exclusive_group(required=True)
    driver.add_argument("--driver", type=Path)
    driver.add_argument("--driver-zip", type=Path, help="verified device ZIP; extract only the validated library")
    p.add_argument("--out", type=Path, default=ROOT / "build-app")
    p.add_argument("--build-tools", default="36.0.0")
    a = p.parse_args()
    for name, path in (("panprobe", a.panprobe), ("adrenotools", a.adrenotools),
                       ("linkernsbypass", a.adrenotools / "lib/linkernsbypass")):
        actual = subprocess.check_output(["git", "-C", str(path), "rev-parse", "HEAD"], text=True).strip()
        if actual != PINS[name]: p.error(f"{name}: expected {PINS[name]}, found {actual}")
        if subprocess.check_output(["git", "-C", str(path), "diff", "--stat"], text=True):
            p.error(f"{name}: source must have no tracked modifications")
    # Same binary safety gate used for custom ZIPs.
    import importlib.util
    spec = importlib.util.spec_from_file_location("device_package", Path(__file__).with_name("package.py"))
    package = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(package)
    src = ROOT / "tools/exynos2100/app"
    out = a.out.resolve()
    for d in ("classes", "dex", "lib/arm64-v8a", "generated", "assets"):
        (out / d).mkdir(parents=True, exist_ok=True)
    if a.driver_zip:
        package.verify(a.driver_zip)
        a.driver = out / "verified-driver.so"
        with zipfile.ZipFile(a.driver_zip) as z:
            a.driver.write_bytes(z.read(package.LIBRARY))
    package.inspect_binary(a.driver)
    env = dict(os.environ, JAVA_HOME=str(a.jdk), PATH=str(a.jdk / "bin") + ":" + os.environ["PATH"])
    bt = a.sdk / "build-tools" / a.build_tools
    jar = a.sdk / "platforms/android-36/android.jar"
    cmake = a.sdk / "cmake/3.22.1/bin/cmake"
    ninja = shutil.which("ninja")
    if not ninja: p.error("ninja must be on PATH")
    run(cmake, "-S", a.adrenotools, "-B", out / "adrenotools", "-G", "Ninja",
        "-DCMAKE_MAKE_PROGRAM=" + ninja, "-DCMAKE_TOOLCHAIN_FILE=" + str(a.ndk / "build/cmake/android.toolchain.cmake"),
        "-DANDROID_ABI=arm64-v8a", "-DANDROID_PLATFORM=android-31", "-DANDROID_STL=c++_static",
        "-DCMAKE_BUILD_TYPE=Release", "-DBUILD_SHARED_LIBS=OFF")
    run(cmake, "--build", out / "adrenotools", "-j2")
    nbin = a.ndk / "toolchains/llvm/prebuilt/linux-x86_64/bin"
    includes = ["-I" + str(ROOT / "include"), "-I" + str(a.panprobe / "tests/dxvk/vulkan/clip-cull")]
    run(nbin / "aarch64-linux-android31-clang", "-std=c11", "-O2", "-fPIC", *includes,
        "-DJava_dev_zenithblue_panvktest_Native_swapchainTest=Java_io_github_protermuxarch_exynosprobe_Native_swapchainTest",
        "-DJava_dev_zenithblue_panvktest_Native_swapchainPhase=Java_io_github_protermuxarch_exynosprobe_Native_swapchainPhase",
        "-c", a.panprobe / "app/src/main/cpp/swapchain.c", "-o", out / "swapchain.o")
    run(nbin / "aarch64-linux-android31-clang++", "-std=c++17", "-O2", "-fPIC", "-shared", "-static-libstdc++",
        "-Wl,-z,max-page-size=16384", "-Wl,-soname,libexynospresent.so", *includes,
        "-I" + str(a.adrenotools / "include"), src / "loader_bridge.cpp", out / "swapchain.o",
        out / "adrenotools/libadrenotools.a", out / "adrenotools/lib/linkernsbypass/liblinkernsbypass.a",
        "-landroid", "-llog", "-ldl", "-o", out / "lib/arm64-v8a/libexynospresent.so")
    for name in ("hook_impl", "main_hook"):
        shutil.copyfile(out / f"adrenotools/src/hook/lib{name}.so", out / f"lib/arm64-v8a/lib{name}.so")
    run("python3", ROOT / "tools/exynos2100/build-panprobe-checks.py", "--panprobe", a.panprobe,
        "--ndk", a.ndk, "--out", out / "preflights", "--shared")
    for name in ("dxvk_reqs", "vkd3d_reqs"):
        shutil.copyfile(out / f"preflights/lib{name}.so", out / f"lib/arm64-v8a/lib{name}.so")
    shutil.copyfile(a.driver, out / "lib/arm64-v8a/libvulkan_panfrost.so")
    run(bt / "aapt2", "link", "-o", out / "base.apk", "-I", jar,
        "--manifest", src / "AndroidManifest.xml", "--java", out / "generated")
    run(a.jdk / "bin/javac", "--release", "8", "-classpath", jar, "-d", out / "classes", src / "MainActivity.java", src / "Native.java")
    run(bt / "d8", "--min-api", "31", "--lib", jar, "--output", out / "dex", *sorted((out / "classes").rglob("*.class")), env=env)
    notices = [(ROOT / "android/NOTICE.txt"), (ROOT / "tools/exynos2100/NOTICE.txt"),
               a.panprobe / "LICENSE", a.panprobe / "NOTICE.md", a.adrenotools / "LICENSE",
               a.adrenotools / "lib/linkernsbypass/LICENSE"]
    with zipfile.ZipFile(out / "base.apk", "a") as z:
        z.write(out / "dex/classes.dex", "classes.dex")
        for lib in sorted((out / "lib").rglob("*.so")): z.write(lib, lib.relative_to(out))
        z.writestr("assets/NOTICE.txt", b"\n\n".join(f.read_bytes() for f in notices))
        z.writestr("assets/provenance.json", json.dumps(dict(PINS,
            integration_revision=package.git("rev-parse", "HEAD"),
            integration_dirty=bool(package.git("status", "--porcelain", "--untracked-files=no")),
            driver_sha256=package.sha(a.driver.read_bytes())), indent=2))
    run(bt / "zipalign", "-P", "16", "-f", "4", out / "base.apk", out / "aligned.apk")
    key = out / "local-test.keystore"
    if not key.exists():
        run(a.jdk / "bin/keytool", "-genkeypair", "-keystore", key, "-storepass", "android", "-keypass", "android",
            "-alias", "localtest", "-keyalg", "RSA", "-keysize", "2048", "-validity", "3650",
            "-dname", "CN=Local Exynos GPU Test")
    apk = out / "exynos-panvk-probe.apk"
    run(bt / "apksigner", "sign", "--ks", key, "--ks-pass", "pass:android", "--out", apk, out / "aligned.apk", env=env)
    run(bt / "apksigner", "verify", "--verbose", apk, env=env)
    print(apk)


if __name__ == "__main__":
    main()
