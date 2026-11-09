# Authorship, provenance and redistribution

* **Mesa/Panfrost/PanVK developers** authored the compiler, Vulkan runtime, device
  code and most graphics implementation. Individual source-file copyrights and
  licenses remain authoritative.
* **JimVulkan and the credited upstream contributors** authored/adapted this
  Android kbase port and its existing gralloc/build/package integration. The
  official GitHub fork retains that repository's commit history and upstream
  remote. It does not make Mesa/the port original work of Pro-Termux-arch.
* **Pro-Termux-arch device branch** adds the owned tracking-page cleanup, the
  installed-NDK ABI overlay generator, runtime staging/packaging validation,
  deterministic device ZIP/provenance records, bounded GPU regression probe,
  isolated loader helper, pinned test adaptation and Linux package integration.
  These additions are MIT, with notices in `tools/exynos2100/NOTICE.txt`.
* **zenithblue-oss/panprobe contributors** authored the reused Android swapchain
  lifecycle test and DXVK/VKD3D requirement gates (MIT). The application compiles
  them from pinned source `dea5b138b62050181a4f50dbd755a5ac1aaf05d3`, not an
  unattributed copy. JNI symbol names are remapped at compilation; the two query
  gates' obsolete Mali-name predicate is adapted in generated build copies only.
  The upstream tests' copyright/NOTICE and MIT license ship in APK assets.
* **Billy Laws/bylaws and libadrenotools/liblinkernsbypass contributors** authored
  the rootless custom-loader/namespace code (BSD-2-Clause; exact notices shipped).
  Pins are recorded in BUILD.md and APK provenance.json. Only custom loading is
  used, not their optional clock/GPU-memory APIs.
* **AOSP and LLVM** license their retained headers/configuration as described in
  `android/gralloc/NOTICE.md`. The generated NDK configuration keeps its LLVM
  Apache-2.0 WITH LLVM-exception notice, changing only the inline ABI namespace.

Preserved unchanged: root LICENSE, LICENSES/, THIRD_PARTY_NOTICES.md,
android/NOTICE.txt and individual upstream copyright/SPDX headers. The existing
gralloc provenance notice has an additive explanation of the new generator.
libdrm static linkage retains its MIT notices; zlib notices are retained too.

The device ZIP includes upstream NOTICE.txt plus FORK-NOTICE.txt. The APK includes
driver notices, device-tooling MIT text, PanProbe LICENSE/NOTICE, adrenotools and
linkernsbypass licenses and exact source pins. The Linux package includes upstream
and fork notices, without bundling glibc or C++ runtime binaries.

No driver patches were imported from the secondary kbase implementations.
They were reviewed as references only. No Samsung/Arm proprietary device library,
credential, serial/UUID, raw system log or signing private key is published.
Raw test logs and local keys are confined to ignored build directories.

This work is not endorsed by Samsung, Arm, Google or Mesa. Experimental API
advertisement and the existence of a fork do not establish conformance or remove
the upstream attribution obligations.
