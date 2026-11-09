# Rebuild on Arch Linux x86_64

## Pinned input and tools

Upstream v1.6.0: `58f2c55854bd48b18df756edb11f187af9cd2fde`.
The device commits are on `device/exynos2100-g78`; see `git log upstream/main..HEAD`.
No claim is made that JimVulkan's subset preserves Mesa's entire git history;
all history present in JimVulkan's repository is retained by this fork.

Inspect before installing:

```sh
command -v git gh adb clang llvm-config meson ninja glslangValidator python3
pacman -Q clang llvm libclc spirv-llvm-translator meson ninja python-mako python-packaging python-yaml
```

If absent, Arch packages are `base-devel git clang llvm libclc
spirv-llvm-translator meson ninja python-mako python-packaging python-yaml
glslang cmake android-tools`. Keep LLVM, clang, libclc and LLVM-SPIRV on the
same major. This build used LLVM 23.1.3, Meson 1.12.1, Ninja 1.13.2,
NDK **28.2.13676358 / clang 19.0.1**, Android API 31.
No packages/toolchains were installed system-wide during this task.

The local host already had extracted Arch packages under
`/home/proadmin/mali-build-tools/root`, matching host tools built from unchanged
Mesa compiler sources in `/home/proadmin/mali-panvk-research/build-linux-host`,
and NDK r28c. These were reused rather than downloading NDK r29.
Check the compiler-source diff before sharing host tools across revisions:

```sh
git diff 58f2c55854bd48b18df756edb11f187af9cd2fde -- src/compiler src/panfrost/compiler src/poly
```

## Android driver

```sh
git clone https://github.com/Pro-Termux-arch/mali-panvk.git
cd mali-panvk
git switch device/exynos2100-g78
# For an exact rebuild, checkout the source_revision from build-manifest.json.
export ANDROID_NDK_HOME=/path/to/ndk/28.2.13676358
JOBS=4 sh ./build.sh "$ANDROID_NDK_HOME"
python3 tools/exynos2100/build-smoke.py "$ANDROID_NDK_HOME"
python3 -m unittest discover -s tools/exynos2100 -p 'test_*.py' -v
TMPDIR=/tmp/opencode python3 tools/exynos2100/package.py \
  build-android/libvulkan_panfrost.so --ndk "$ANDROID_NDK_HOME" \
  --source-revision "$(git rev-parse HEAD)"
```

`build.sh` stages the runtime into `build-android/staging` without root, strips
that installed DSO, and leaves debug symbols in the build tree. Link stubs
are not shipped. `GRALLOC=0` is an investigation option, not the tested default.
Do not change GRALLOC or NDK inside an existing build tree without reconfiguration
verification; use a separate BUILD_DIR for a different toolchain.

Local cache environment used here (not required with installed Arch packages):

```sh
export PATH=/home/proadmin/mali-build-tools/root/usr/bin:$PATH
export PYTHONPATH=/home/proadmin/mali-build-tools/root/usr/lib/python3.14/site-packages
export LD_LIBRARY_PATH=/home/proadmin/mali-build-tools/root/usr/lib
export HOST_BUILD_DIR=/home/proadmin/mali-panvk-research/build-linux-host
```

Meson verifies downloaded wrap archive hashes. The dependency versions are
pinned in `subprojects/*.wrap`; cached libdrm 2.4.133 and zlib 1.3.1 sources
were reused here. First-time downloads are dependency archives, not device blobs.
The generated gralloc `__config_site` keeps the selected NDK's LLVM license.

## Device ZIP verification and reproducibility

```sh
python3 tools/exynos2100/package.py --verify dist/exynos2100/exynos2100-panvk-REVISION.zip
sha256sum -c dist/exynos2100/exynos2100-panvk-REVISION.zip.sha256
```

The ZIP records exact binary hash, compilation revision, packaging revision,
NDK and dynamic dependencies. `SOURCE_DATE_EPOCH` defaults to the compilation
commit timestamp; member ordering/permissions/timestamps are fixed. Packaging the
same binary with identical inputs must produce identical ZIP bytes. This does
**not** prove two independent clean compiler builds are byte-identical. Record
that separately; never use a new packaging HEAD as the old binary's source hash.

## Native Linux (no window system yet)

Use a glibc AArch64 sysroot/cross file; do not use the NDK for this target:

```sh
export CROSS_FILE=/path/to/aarch64-linux.ini
export HOST_TOOLS_DIR=/path/to/matching/mesa/host-tools/bin
JOBS=4 sh tools/exynos2100/build-linux.sh
```

The local sysroot/cross-file cache is `/home/proadmin/mali-build-tools`.
See the limitations in `tools/exynos2100/linux-README.md` before using the ICD.

## Isolated Android presentation APK

This helper uses the Android system Vulkan **loader** plus libadrenotools' custom
driver hook, not direct-ICD Android surface calls. Android WSI is in the loader.
Only the custom-driver flag is used; GPU mapping/clock APIs are not called.

External source pins (about 7 MB reported PanProbe git size and 105 KB reported
adrenotools git size; network transfers may differ):

* https://github.com/zenithblue-oss/panprobe — `dea5b138b62050181a4f50dbd755a5ac1aaf05d3` (MIT tests)
* https://github.com/bylaws/libadrenotools — `8fae8ce254dfc1344527e05301e43f37dea2df80` (BSD-2-Clause)
* Its liblinkernsbypass submodule — `aa3975893d83ef1bc84c321ec60c65fbf1287887` (license shipped in APK)

Clone/check out these revisions, including the submodule, then:

```sh
python3 tools/exynos2100/build-app.py \
  --sdk /path/to/Android/sdk --ndk "$ANDROID_NDK_HOME" --jdk /path/to/jdk21 \
  --panprobe /path/to/panprobe --adrenotools /path/to/libadrenotools \
  --driver-zip dist/exynos2100/exynos2100-panvk-REVISION.zip
```

Uses installed SDK 36, build-tools 36.0.0, CMake 3.22.1, JDK 21, no Gradle or
Maven downloads. The APK is locally test-signed, debuggable, has no requested
permissions, and embeds all third-party notices plus source pins and driver hash.
The signing key stays in ignored `build-app/`; it is not a production signing key.

The APK also compiles the pinned PanProbe DXVK/VKD3D query gates as app-local
modules via `build-panprobe-checks.py`. Only their GPU-selection predicate is
adapted: vendor 0x13b5 + integrated G78 replaces the obsolete name-only "Mali"
match. No feature requirement is weakened. These modules query the isolated
**Android loader**, so loader-owned `VK_KHR_swapchain` is counted correctly.
This is not a Wine/DXVK/VKD3D gameplay test.
