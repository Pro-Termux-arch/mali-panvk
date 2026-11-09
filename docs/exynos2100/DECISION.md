# ADR 001: isolated G78/JM stack

Date: 2026-11-09. Status: selected; hardware validation gates deployment.

Base: JimVulkan/mali-panvk v1.6.0,
`58f2c55854bd48b18df756edb11f187af9cd2fde` (remote tag verified).
Mesa version: 27.0.0-devel. GitHub fork:
https://github.com/Pro-Termux-arch/mali-panvk, branch `device/exynos2100-g78`.

## Evidence and choice

* Live kernel: 5.4.302-Floppy-v1.1.2-KN-release, Exynos 2100.
  Kernel gpuinfo: Mali-G78, 14 cores, r0p1, product 0x9202.
  ROM model/device strings are SM-S947B/m2s: do not use these as GPU identity.
* `/dev/mali0` exists, no DRM node observed. SELinux is Enforcing.
  Existing 0666 node permissions are observed, not changed by this project.
* Installed module gpu_driver_9840 identifies r38p1, extracted from Exynos 1080.
  Android advertises Vulkan 1.3, but `cmd gpu vkjson` physical properties report
  1.1.213. Treat actual queried capabilities as authoritative, not package flags.
* JimVulkan includes v9 JM submission, Android gralloc and packaging; its README
  reports S21/Exynos 2100 testing. These are upstream claims, not our test results.
* zenithblue-oss/panvk-kbase-android at
  cf70e02b918e1864cbcd746a7fefa57b96c673ce offers pinned patch profiles,
  v9 G78 Pixel 6a reports, PanProbe and PanPlay. Useful independent test/reference
  implementation; its much larger checkout and broad patch stack are unnecessary
  for the first baseline. Do not import its patches without license review.
* whaltermc/panvk and funnymdzz/mesa document Pixel 7/G710 CSF-centric submission,
  synchronous waits and incomplete DMA-BUF/WSI. CSF results do not establish G78
  JM compatibility. Neither is preferable as the initial Android base.

## Architecture and safety boundary

Android: per-process custom Vulkan -> PanVK -> kbase JM -> Mali-G78.
Keep the installed Mali GLES/Vulkan stack for Android, DeX and fallback.
Desktop GL: evaluate Zink over a separate **glibc AArch64** PanVK ICD; never
dlopen an Android/Bionic driver from an ordinary glibc process.
Preserve existing VirGL integration; do not change its environment globally.

Review found upstream issue reports of DX11 freezes, gralloc failures on other
GPUs and even device reboot on G615. Experimental userspace may fault the kernel.
Start with bounded offscreen tests, then separately test application presentation.
Do not advertise conformance, Vulkan 1.4 correctness, DXVK or Zink success from
extension enumeration alone. No performance optimization before correctness.

## Provenance

Upstream history, LICENSE, LICENSES, THIRD_PARTY_NOTICES.md, AOSP header notices
and android/NOTICE.txt are retained. Mesa/PanVK and the kbase port are upstream
work. Device tooling, build fixes and test results added on this branch are fork
work, identified by commits. No proprietary device binaries will be committed.
