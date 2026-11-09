# Native Linux AArch64 ICD — limited offscreen build

This is a **glibc** PanVK library, not an Android/Bionic library. It targets
G78/JM kbase directly; access to `/dev/mali0` and the kernel's existing policy
are still required. Do not change SELinux or device permissions to make it run.

The archive does not bundle a libc, Vulkan loader, Zink, OpenGL or X server.
Use your existing compatible AArch64 glibc environment and its Vulkan loader.
Check `ldd lib/libvulkan_panfrost.so` inside that environment and verify
`sha256sum -c SHA256SUMS` before use. This build used a current Arch ARM sysroot;
an older glibc may need a rebuild against that distribution's sysroot.

Run an **offscreen/compute** application:

```sh
./with-panvk your-vulkan-compute-program
```

`VK_DRIVER_FILES` is local to that child process. Rollback is simply running the
application without this wrapper. Never copy this ICD into Android's vendor HAL.

## Presentation and Zink are not delivered by this package

The selected upstream checkout omits the Gallium/Zink implementation. Its
attempted `-Dgallium-drivers=zink` configuration fails at the missing
`src/gallium/drivers/zink/00-zink-defaults.conf`, before compilation. A separate
full Mesa/Zink build is needed. This ICD was built with `-Dplatforms=`; it does
not expose X11/Wayland presentation and cannot by itself accelerate an ordinary
windowed Firefox or Minecraft process. Existing VirGL remains the fallback.

Next integration work: build/test full Mesa Zink with compatible features, then
add/test X11 WSI and DMA-BUF import/export against the existing Termux:X11 path.
G78/JM support must be preserved; G710/CSF testing is insufficient. Do not force
OpenGL version overrides to conceal missing features. Minecraft 1.8.9 needs
legacy compatibility-profile/fixed-function behavior as well as shaders; a core
profile version string alone is not evidence. Firefox needs a functioning GL/EGL
or GLX context and window-system integration; WebRender is a separate test.
