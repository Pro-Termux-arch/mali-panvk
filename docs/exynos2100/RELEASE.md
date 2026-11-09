# Experimental Exynos 2100/G78 build

Hardware-tested, isolated userspace Vulkan on the existing kbase JM kernel.
**Not** a KernelSU/Magisk module; never flash or replace vendor/global drivers.

Contents:

* Android custom-loader ZIP, verified ARM64/API31 binary, MIT/upstream notices,
  full source/toolchain manifest and per-file checksums.
* Test-signed Android presentation APK: stock or PanVK selection in its own
  process, driver identity gate, DXVK/VKD3D query preflights and real rendering.
* Separate native glibc AArch64 ICD archive: **offscreen/compute only**, no X11/
  Wayland or Zink. Requires compatible Linux userspace (GLIBC >= 2.38).
* SHA256SUMS and ARTIFACTS.json identify exact artifacts and source revisions.

Verified: compute/readback, vertex/fragment pixels, Android presentation/resize/
swapchain cycling, Linux Vulkan loader/ICD, reversible stock/custom switching.
Fixed: kbase tracking-map leakage on load/unload (baseline 6 retained VMAs after
3 cycles; modified 0 after every one of 10 cycles on Android and native Linux).

Not claimed: Vulkan CTS conformance, Zink/Firefox/Minecraft, Wine/DXVK/VKD3D games,
physical external-display qualification, long-term stability or an FPS speedup.
DXVK 56/56 and VKD3D 24/24 hard-query checks passing is not a game test.

Upstream: JimVulkan/mali-panvk v1.6.0,
58f2c55854bd48b18df756edb11f187af9cd2fde. Mesa, JimVulkan, PanProbe and
libadrenotools/linkernsbypass authors retain their credits and licenses.
No proprietary extracted driver binaries, private keys or raw device logs ship.

See [RESULTS.md](RESULTS.md), [BUILD.md](BUILD.md), [DEPLOYMENT.md](DEPLOYMENT.md)
and [CREDITS.md](CREDITS.md) for exact scope, reproduction and rollback.
