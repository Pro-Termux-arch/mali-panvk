#!/bin/sh
# SPDX-License-Identifier: MIT
# Build a glibc AArch64 ICD, not an Android/Bionic library.
# Requires: CROSS_FILE, HOST_TOOLS_DIR, Meson/Ninja and an AArch64 glibc sysroot
# providing zlib/expat C/C++ runtime headers/libraries. No root or installation.
set -eu
cd "$(dirname "$0")/../.."
: "${CROSS_FILE:?set CROSS_FILE to a Meson AArch64 Linux cross file}"
: "${HOST_TOOLS_DIR:?set HOST_TOOLS_DIR to matching Mesa host compiler tools}"
BUILD="${LINUX_BUILD_DIR:-build-linux-g78}"
JOBS="${JOBS:-4}"
case "$JOBS" in ''|*[!0-9]*|0) echo 'invalid JOBS' >&2; exit 1 ;; esac
PATH="$HOST_TOOLS_DIR:$PATH"
export PATH
if [ ! -f "$BUILD/build.ninja" ]; then
   meson setup "$BUILD" --cross-file "$CROSS_FILE" \
      --prefix=/usr -Dlibdir=lib -Dbuildtype=release -Db_ndebug=true \
      -Dplatforms= -Dvulkan-drivers=panfrost -Dgallium-drivers= \
      -Dvulkan-layers= -Dtools= -Dvideo-codecs= \
      -Dmesa-clc=system -Dprecomp-compiler=system -Dllvm=disabled \
      -Dzstd=disabled -Dlmsensors=disabled -Dperfetto=false \
      -Dglx=disabled -Dgbm=disabled -Degl=disabled -Dgles1=disabled -Dgles2=disabled \
      -Dallow-fallback-for=libdrm --force-fallback-for=libdrm -Dlibdrm:default_library=static
fi
ninja -j "$JOBS" -C "$BUILD" src/panfrost/vulkan/libvulkan_panfrost.so \
   src/panfrost/vulkan/panfrost_icd.aarch64.json
meson install -C "$BUILD" --no-rebuild --skip-subprojects --tags runtime \
   --destdir "$(cd "$BUILD" && pwd)/staging"
OUT=dist/linux-aarch64
mkdir -p "$OUT/lib"
"${CROSS_STRIP:-llvm-strip}" -o "$OUT/lib/libvulkan_panfrost.so" \
   "$BUILD/staging/usr/lib/libvulkan_panfrost.so"
cp tools/exynos2100/linux-with-panvk "$OUT/with-panvk"
chmod 755 "$OUT/with-panvk"
cp android/NOTICE.txt "$OUT/NOTICE.txt"
cp tools/exynos2100/NOTICE.txt "$OUT/FORK-NOTICE.txt"
cp tools/exynos2100/linux-README.md "$OUT/README.md"
cat > "$OUT/exynos2100.json" <<'EOF'
{"file_format_version":"1.0.0","ICD":{"library_path":"./lib/libvulkan_panfrost.so","api_version":"1.4.0"}}
EOF
git rev-parse HEAD > "$OUT/SOURCE_REVISION"
(cd "$OUT" && sha256sum lib/libvulkan_panfrost.so exynos2100.json with-panvk NOTICE.txt FORK-NOTICE.txt README.md SOURCE_REVISION > SHA256SUMS)
echo "$OUT (offscreen/compute only; no Linux window-system integration claimed)"
