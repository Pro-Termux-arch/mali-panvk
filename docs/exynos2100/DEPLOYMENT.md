# Isolated deployment and rollback

## Safety boundary

The ZIP is an application-loader package, **not** a Magisk/KernelSU module.
Never install it through KernelSU, copy it into `/vendor`, replace a Vulkan HAL,
or modify `gpu_driver_9840`. Do not change SELinux, device permissions, clocks,
voltages, thermal policies or boot state. Experimental userspace can still
cause a GPU/kernel fault. Save work before testing; stop at instability.

Tested loader: the helper APK built from this branch, Android's system Vulkan
loader and libadrenotools custom-driver mode at the pins in BUILD.md.
Other apps/GameHub variants are not verified. A ZIP by itself cannot redirect
an arbitrary Android app. That app must explicitly implement custom loading.

## Verify before installing

From the repository root:

```sh
python3 tools/exynos2100/package.py --verify dist/exynos2100/exynos2100-panvk-497c27a3422a.zip
sha256sum -c dist/exynos2100/exynos2100-panvk-497c27a3422a.zip.sha256
```

The tested Android driver SHA-256 is
`3981f449c24584744ad8bfb750748d647bf6764112649ac8777659ef5e89e461`.
The ZIP SHA-256 is
`285b36ee894702c32b4aeaea0b272a9823e3e0d42eb9290765e48df86a335a38`.
Build-app.py verifies this ZIP then embeds its exact library; it never bundles
device-extracted proprietary libraries. Source/debug artifacts stay on the host.

## Install the test APK only

```sh
adb install build-app/exynos-panvk-probe.apk
```

If this test package is already installed with this local signing key, use
`adb install -r ...`. If its signing key differs, do not uninstall an unknown
existing app automatically; preserve any data and decide explicitly first.
This APK is debuggable and locally test-signed, not a production distribution.
It requests no network, shared-storage or root permission.

## Select stock or PanVK — app-local, fresh process

Stock (also the app's default when opened from the launcher):

```sh
adb shell am force-stop io.github.protermuxarch.exynosprobe
adb shell am start -W -n io.github.protermuxarch.exynosprobe/.MainActivity --es mode stock
```

Custom PanVK:

```sh
adb shell am force-stop io.github.protermuxarch.exynosprobe
adb shell am start -W -n io.github.protermuxarch.exynosprobe/.MainActivity --es mode panvk
```

The helper verifies the driver ID, Arm vendor, integrated GPU type and G78
identity before testing. A custom-loader failure or silent stock fallback is
an error, not a successful PanVK run. Do not change modes within one process;
namespace-loader state is process-lifetime. Keep the app visible while its
surface test runs; it takes about nine seconds at 60 Hz.

Automated validation (PID-filtered logcat, no root or log clearing):

```sh
python3 tools/exynos2100/test-app.py --mode panvk --out build-audit/app-panvk.log
python3 tools/exynos2100/test-app.py --mode stock --out build-audit/app-stock.log
```

Require `PASS selected=...`, `PASS swapchain_lifecycle`, and `exit:0`.
The DXVK/VKD3D query exits shown in the UI are **preflights**, not game tests.
The ROM blocks `run-as` privilege setup, so use the above logcat path. Only this
task's private test-log reads used root on the host; the app does not.

## Rollback

* Select stock using the command above; all other apps already retain their
  normal graphics path.
* To remove this project's app and its private caches/logs:
  `adb uninstall io.github.protermuxarch.exynosprobe`.
* ADB command-line probe files are confined to
  `/data/local/tmp/exynos2100-panvk/`. Removing only that directory is optional;
  nothing there starts at boot or changes other apps.
* Keep existing VirGL/Termux:X11/DroidSpaces launch routines for Linux apps.
  This project neither replaces their libraries nor changes their configuration.

Stock Mali GLES is intentionally unchanged. No ANGLE replacement is installed.
No global DeX/compositor settings or drivers were changed. The presentation
test used a 1920x1080 scrcpy virtual display, not a verified physical USB-C/HDMI
DeX/external-display matrix; do not infer external-display qualification.

## Native Linux package

See `tools/exynos2100/linux-README.md`. This separate glibc ICD is limited to
offscreen/compute: it is **not** a drop-in windowed Firefox/Minecraft solution.
The process-local `with-panvk` wrapper is reversible by omitting it; do not
export its environment globally or load the Android DSO into glibc.
