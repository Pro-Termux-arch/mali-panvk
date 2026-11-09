# Engineering log

## 2026-11-09

* Audited tools before installing: gh authenticated, adb connected, NDK r27d/r28c
  installed. LLVM/Clang 23 installed. Meson/Ninja/Mako/libclc/LLVM-SPIRV available
  in an existing user-local extracted Arch package cache, with matching LLVM 23.
* Reused existing source and host-tool caches. Created official GitHub fork via
  `gh repo fork JimVulkan/mali-panvk --clone=false`; cloned with a local object
  reference, preserving upstream commit history. Verified origin and upstream.
* Read upstream build.sh, package.py, license notices and kbase handshake code.
  Baseline will use the unmodified driver before focused engineering patches.
* Raw device output stays in ignored build directories, not in public history.

## 2026-11-10 (device/host local date rollover)

* Stock r38p0 reports Vulkan 1.1.213; module metadata says r38p1. Verified with
  direct physical-device properties, not Android's 1.3 package-manager flag.
* Unmodified build failed with installed NDK r28c: upstream's newer libc++
  __config_site uses incompatible positive feature macros. Generated the ABI
  overlay from this NDK, changing only the namespace. Unit tests passed.
* Baseline driver rendered and computed correctly, but leaked 2 /dev/mali0 VMAs
  per load/unload. After recording 6 VMAs after 3 cycles, retained/unmapped the
  owned tracking page on teardown and setup failure. Modified Android and Linux
  drivers passed 10 cycles each with zero retained Mali VMAs.
* Meson build output carried $ORIGIN/../../android_stub RUNPATH. Packaging gate
  rejected it; runtime staging now removes it before stripping/packaging.
* Official libadrenotools custom-loader mode successfully presented on G78,
  including resize and 10 swapchain cycles. Stock/custom switching verified
  in separate processes. No clock/mapping/turbo hooks used.
* The ROM blocks run-as setup; used PID-filtered logcat for automated app tests.
  Root was used only for read-only device metadata/private test-log inspection.
* JM UAPI query returned 11.35; requesting 11.31 returns 11.31, requesting 11.99
  negotiates to 11.35. Upstream's comment describing a validate-only handshake
  is not universal; no handshake behavior change was needed here.
* Zink preflight found unsupported smooth/stippled line features, and selected
  upstream lacks Gallium/Zink source. Windowed Linux integration remains open.
* Direct ICD requirement checking falsely lacked loader-owned VK_KHR_swapchain.
  Routed pinned PanProbe query modules through the isolated Android loader:
  DXVK hard 56/56 and VKD3D hard 24/24. Not game/application execution evidence.
* Real output PPMs from stock, baseline, modified Android and native Linux were
  byte-identical. Only the tiny synthetic image and sanitized summaries are
  published; raw device, display, kernel and package logs remain ignored.
