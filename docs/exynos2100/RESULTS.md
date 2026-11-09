# Exynos 2100 results — 2026-11-09/10

**Outcome: a built, hardware-tested, isolated Android driver and presentation
helper, plus a tested offscreen/compute glibc AArch64 ICD. Not yet a complete
Firefox/Minecraft/Zink/Wine solution.**

## Source and artifact identity

Fork: https://github.com/Pro-Termux-arch/mali-panvk
Branch: `device/exynos2100-g78`.

| Component | Exact compilation source |
|---|---|
| Chosen upstream v1.6.0 | `58f2c55854bd48b18df756edb11f187af9cd2fde` |
| Baseline (build-only NDK compatibility patch, no driver-core change) | `2dcc5f8920` (full hash in git history) |
| Tested/packaged Android driver | `497c27a3422a` (full hash in ZIP build-manifest.json) |
| Tested Linux glibc driver | `ab86f464134a8b04dabd463ec5d66f81cf19ffd9` |

The Android driver's later packaging/integration revision is recorded separately
from its compilation revision. Documentation changes after compilation do not
relabel the old binary as a new build. Use release SHA256SUMS for the APK/archive.

* Android: `dist/exynos2100/exynos2100-panvk-497c27a3422a.zip`
* Android library SHA-256: `3981f449c24584744ad8bfb750748d647bf6764112649ac8777659ef5e89e461`
* ZIP SHA-256: `285b36ee894702c32b4aeaea0b272a9823e3e0d42eb9290765e48df86a335a38`
* Presentation helper: `build-app/exynos-panvk-probe.apk` (also copied into release artifacts)
* Linux: `dist/linux-aarch64/`, separate ELF/glibc library and process-local ICD wrapper.
* Linux library SHA-256: `13ae47abe459b3228aa0b676c503f364473810eeafe4c2bee142302a3f55b620`
* Baseline ZIP: `dist/baseline/mali-panvk-27.0.0-devel-2dcc5f8.zip`
  (engineering comparison only; carries the upstream metadata convention).

## Live device audit

Kernel GPU properties: Mali-G78, **14 cores**, r0p1, product 0x9202.
Vulkan GPU ID: 0x92020010, Arm vendor 0x13b5, integrated GPU. SoC: Exynos 2100.
Kernel: `5.4.302-Floppy-v1.1.2-KN-release`; Android 16.
The ROM reports **SM-S947B/m2s**, not the user's historical SM-G996B identity;
GPU/kernel evidence, not that model string, determined compatibility.

`/dev/mali0` is kbase, with existing system:system 0666 permissions. No DRM node
was observed. No permissions or security settings were changed; SELinux remained
**Enforcing**. JM VERSION_CHECK returned **11.35** for a 0.0 query. Requesting
11.31 returned 11.31; requesting 11.99 returned 11.35. The current backend's
11.31 request works without a UAPI/kernel modification.

Installed gpu_driver_9840 metadata identifies r38p1. Actual stock queries return
`Mali-G78`, driver info beginning `v1.r38p0-01eac0`, physical Vulkan **1.1.213**.
Android advertises a Vulkan 1.3 package capability; that is not the physical API.
This task did not replace or extract/publish that module's proprietary binaries.

Android's SurfaceFlinger uses a Vulkan Ganesh RenderEngine. Existing DroidSpaces
configuration enables GPU/X11/VirGL, and existing session scripts select
`GALLIUM_DRIVER=virpipe` with a stock-Mali Android VirGL server. No active VirGL
or Linux desktop process was observed during the initial audit; the existing
container was stopped. It was not started/replaced just to run the new tests.

## Stock vs baseline vs modified

| Test | Installed stock Mali | Original PanVK core + NDK build fix | Modified PanVK |
|---|---|---|---|
| Physical identity | Mali-G78 | PanVK-G78 MC14 | PanVK-G78 MC14 |
| Physical API reported | 1.1.213 | 1.4.363 | 1.4.363 |
| Compile with installed r28c | Proprietary, not built | Pass after isolated build fix | Pass |
| Compute SSBO/readback | 256/256 correct | 256/256 correct | 256/256 correct on all 10 cycles |
| Vertex + fragment image | Correct | Correct | Correct on all 10 cycles |
| In-process load/teardown | 3 cycles, no retained Mali VMA | **6 retained VMAs after 3 cycles** | **0 after each of 10 cycles** |
| Android surface lifecycle | Pass through stock loader | Not tested | Pass through isolated custom loader |
| Linux/glibc equivalent | Not applicable to Bionic blob | Cached upstream ICD: compute/raster pass, 4 leaked VMAs after 2 cycles | Fresh build: 10 loader-backed cycles, 0 retained VMAs |
| FPS improvement | No claim | Not measured | No claim |

Vulkan versions above are **reported capabilities**, not CTS/conformance results.
No Vulkan feature/API-version override was used.

### Actual command execution and output

The small probe allocates/maps real GPU buffers, compiles SPIR-V into compute
and vertex/fragment pipelines, dispatches 256 arithmetic outputs, submits a
triangle draw, copies an optimal-tiled render target back to a host buffer and
waits on explicit fences with five-second timeouts. It verifies all 4096 RGBA
pixels (1152 red, 2944 blue), saves PPM, destroys resources and unloads the driver.
Identity checks reject software, non-Arm and non-G78 devices.

The stock, baseline, modified Android and modified native Linux PPMs were
**byte-identical**, SHA-256:
`36c01cc8520f3a4049319430cda0a11e030e49f8d2ef176ef80655830710b6d0`.

Actual GPU output, converted losslessly from that readback (not an illustration):

![GPU-rendered triangle](evidence/triangle.png)

The tracking-map regression test inspects `/proc/self/maps` after each complete
teardown. The fix retains each object's own tracking mmap pointer and unmaps it
after queue retirement, including setup failure paths. Success teardown was
hardware tested; error cleanup was source-reviewed, not fault-injected.

### Android surface presentation and selection

The exact ZIP library was verified/extracted into a test APK with
libadrenotools custom mode. The helper refuses a silent stock fallback by checking
`VK_DRIVER_ID_MESA_PANVK` before rendering. Stock and custom selection both passed
in fresh processes. The user also visually confirmed the red triangle on blue.

Each recorded lifecycle run presents 300 frames, resizes and presents 120 frames,
then does ten create/present-10/destroy swapchain cycles: **520 frames per run**.
It passed initial and repeated custom launches, and the stock/custom A/B run.
Typical recorded frame rates were about 60–61 FPS, **display/FIFO limited**;
these are not throughput benchmarks or a demonstrated performance improvement.

Display audit identified a **1920x1080 scrcpy virtual display**. This is successful
Android surface presentation, not qualification of physical HDMI/USB-C DeX.

### Native Linux path

A fresh, separate **glibc AArch64** driver was built with Clang/LLVM 23 and the
existing AArch64 sysroot. Tested through the native Linux Vulkan loader and its
ICD JSON in the **ADB shell SELinux context**, using a private test runtime,
while retaining Android's kernel/SELinux policy. This was not a run inside the
stopped DroidSpaces container or Termux's app security domain. Ten compute, raster and
unload cycles passed. It references GLIBC symbols through **2.38**, GLIBCXX through
3.4 and CXXABI through 1.3. Rebuild for an older target distribution if necessary.
The package does not redistribute the test runtime's libc or C++ libraries.

This build has **no X11/Wayland WSI**. Native windowed Linux applications remain
blocked on presentation integration and a compatible full Mesa/Zink userspace.
Android/Bionic and Linux/glibc DSOs are separate artifacts, never interchanged.

## Zink, ported apps and Direct3D

* **Zink/OpenGL: not passed.** Current GL 2.1 documented preflight found
  `smoothLines`, `stippledRectangularLines`, `stippledBresenhamLines` and
  `stippledSmoothLines` false. Direct ICD enumeration also lacks
  `VK_KHR_swapchain_mutable_format`; WSI/loader ownership needs separate handling.
  The selected checkout omits Gallium/Zink; configuration fails at missing
  `src/gallium/drivers/zink/00-zink-defaults.conf`. No version override disguises it.
* **Firefox/Minecraft Java 1.8.9: not run on this stack.** These need tested
  desktop GL contexts, correct legacy compatibility behavior (Minecraft) and
  Linux/Android launcher integration. The test helper is not a ported PC app.
  Existing VirGL remains the reliable integration fallback rather than being
  removed to force an incomplete native path.
* **DXVK preflight: 56/56 hard checks, 22/38 soft checks available.** This is the
  pinned PanProbe DXVK 3.1.1 query gate through the verified Android loader.
  Direct-ICD checking initially reported 55/56 because `VK_KHR_swapchain` belongs
  to Android's loader; routing through that loader correctly resolves the gap.
* **VKD3D preflight: 24/24 hard checks, 58/76 soft checks available.** The pinned
  checker predicts FL **11_0**, not FL12. It is not a successful D3D12 run.
* **Wine/DXVK/VKD3D applications: not executed.** Their query results establish
  candidate compatibility, not gameplay, correctness or complete feature levels.
* **Native Mali GLES unchanged.** No ANGLE replacement was needed/installed.

## Stability, safety and remaining tests

* Runtime staging removes the build-only Android stub RUNPATH; the ZIP gate
  verifies ELF64/AArch64 DSO, stripped sections, dependencies, exported ICD entry
  point, metadata, paths and per-file SHA-256. No stubs/proprietary blobs shipped.
* Two packaging runs of identical inputs produced identical ZIP bytes. A second
  independent clean toolchain build was **not** performed; bit-identical compiler
  reproducibility is not established by the ZIP test.
* Host tests include malformed/truncated/wrong-architecture ELF rejection,
  ABI-header transformation, deterministic archive metadata, the real packaged
  artifact, deliberate binary corruption and unsafe metadata-path rejection.
* Missing-library negative test failed cleanly without software/stock fallback.
  `VK_ERROR_DEVICE_LOST` recovery, Android background/surface-loss behavior,
  memory pressure, AHardwareBuffer interoperability beyond this presentation,
  cache corruption, validation layers and CTS remain untested.
* Thermal status was 0 at sampled start/end (AP about 32.5–34 C). No sustained
  thermal or battery benchmark was performed. GPU time-in-state counters advanced
  during tests, but also include system composition: not exclusive attribution.
* Sampled post-test kernel ring buffer had zero Mali lines matching
  fault/error/reset/hang/timeout. This is limited telemetry, not proof of absence
  of kernel faults or long-term stability.
* No kernel/boot/vendor flash, module overwrite, SELinux change, permission
  widening, clock/voltage/thermal change, persistent boot hook or global driver
  replacement was performed. App uninstall/process-local selection is reversible.

## Useful next engineering work

1. Upstream the small tracking-page lifetime fix, with the VMA regression case.
2. Build a full Mesa/Zink tree, verify its real feature requirements/emulation,
   then test compatibility-profile GL and Minecraft 1.8.9 without overrides.
3. Implement/test native Linux X11 WSI and DMA-BUF against existing Termux:X11;
   keep a software-copy presentation option separate from software rendering.
4. Run a real small DXVK application through the already verified Android loader,
   then a representative application, before any gameplay support claim.
5. Add validation layers, deterministic cache and device-loss tests, then several
   controlled sustained workload trials. Measure rather than infer speedups.
6. Review loader namespace fragility and test Android lifecycle/display removal;
   keep the helper experimental until those failure paths are qualified.

Full provenance/licenses: [CREDITS.md](CREDITS.md). Build/install procedures:
[BUILD.md](BUILD.md), [DEPLOYMENT.md](DEPLOYMENT.md).
