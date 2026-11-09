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
