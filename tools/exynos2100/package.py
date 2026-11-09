#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Pro-Termux-arch
"""Validated, deterministic custom-loader ZIPs. Never installs a system driver."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import time
import zipfile

ROOT = Path(__file__).resolve().parents[2]
UPSTREAM = "58f2c55854bd48b18df756edb11f187af9cd2fde"
LIBRARY = "libvulkan_panfrost.so"
ALLOWED_DEPS = {"libc.so", "libdl.so", "libm.so", "liblog.so", "libnativewindow.so", "libhardware.so",
                "libc++.so", "libutils.so", "libcutils.so", "libsync.so",
                "android.hardware.graphics.mapper@4.0.so"}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def git(*args):
    return subprocess.check_output(["git", "-C", str(ROOT), *args], text=True).strip()


def validate_elf(data):
    """Fail closed on truncation, wrong ABI, malformed tables or unstripped ELF."""
    if len(data) < 64 or data[:7] != b"\x7fELF\x02\x01\x01":
        raise ValueError("expected ELF64 little endian")
    if struct.unpack_from("<HH", data, 16) != (3, 183):
        raise ValueError("expected ET_DYN AArch64, not a host binary")
    shoff = struct.unpack_from("<Q", data, 40)[0]
    ehsize, _, _, shsize, count, strindex = struct.unpack_from("<6H", data, 52)
    if ehsize != 64 or shsize != 64 or not count or strindex >= count or shoff < 64 or shoff + count*64 > len(data):
        raise ValueError("invalid or missing ELF section table")
    sections = [struct.unpack_from("<IIQQQQIIQQ", data, shoff+i*64) for i in range(count)]
    strings = sections[strindex]
    if strings[1] != 3 or strings[4] + strings[5] > len(data):
        raise ValueError("invalid section name table")
    names = data[strings[4]:strings[4]+strings[5]]
    for section in sections:
        if section[0] >= len(names) or b"\0" not in names[section[0]:]:
            raise ValueError("invalid section name")
        name = names[section[0]:].split(b"\0", 1)[0]
        if section[1] != 8 and section[4] + section[5] > len(data):
            raise ValueError("section outside file")
        if name == b".symtab" or name.startswith((b".debug", b".zdebug")):
            raise ValueError("driver must be stripped")


def inspect_binary(path):
    validate_elf(path.read_bytes())
    dynamic = subprocess.check_output(["readelf", "-dW", str(path)], text=True)
    needed = sorted(re.findall(r"\(NEEDED\).*\[([^]]+)\]", dynamic))
    if not {"libc.so", "libm.so"}.issubset(needed) or set(needed) - ALLOWED_DEPS:
        raise ValueError(f"unexpected Android dependencies: {needed}")
    if re.search(r"\((?:RPATH|RUNPATH)\)", dynamic):
        raise ValueError("driver must not carry a build-host library search path")
    symbols = subprocess.check_output(["readelf", "--dyn-syms", "-W", str(path)], text=True)
    exports = [line for line in symbols.splitlines() if " UND " not in line]
    if not any(re.search(r"\bvk_icdGetInstanceProcAddr$", line) for line in exports):
        raise ValueError("missing ICD entry point export")
    if "INTERP" in subprocess.check_output(["readelf", "-lW", str(path)], text=True):
        raise ValueError("expected driver DSO, not a PIE executable")
    return needed


def archive_bytes(files, epoch):
    stamp = time.gmtime(max(315532800, min(epoch, 4354819198)))[:6]
    out = io.BytesIO()
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name, contents in sorted(files.items()):
            info = zipfile.ZipInfo(name, stamp)
            info.create_system = 3
            info.external_attr = 0o100644 << 16
            z.writestr(info, contents, compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)
    return out.getvalue()


def verify(path):
    with zipfile.ZipFile(path) as z:
        expected = {"meta.json", LIBRARY, "NOTICE.txt", "FORK-NOTICE.txt", "build-manifest.json", "SHA256SUMS"}
        if set(z.namelist()) != expected or len(z.infolist()) != len(expected):
            raise ValueError("unexpected or duplicate package entries")
        if any(i.file_size > 100*1024*1024 for i in z.infolist()):
            raise ValueError("oversized package member")
        files = {name: z.read(name) for name in expected}
    meta = json.loads(files["meta.json"])
    if meta["libraryName"] != LIBRARY or meta["minApi"] != 31 or meta["schemaVersion"] != 1:
        raise ValueError("invalid loader metadata")
    if any(c in meta["name"] for c in "/\\"):
        raise ValueError("unsafe loader directory name")
    checks = dict(line.split("  ", 1)[::-1] for line in files["SHA256SUMS"].decode().splitlines())
    if set(checks) != expected - {"SHA256SUMS"}:
        raise ValueError("incomplete checksums")
    for name, digest in checks.items():
        if sha(files[name]) != digest:
            raise ValueError(f"checksum mismatch: {name}")
    manifest = json.loads(files["build-manifest.json"])
    if manifest["library_sha256"] != sha(files[LIBRARY]):
        raise ValueError("manifest library hash mismatch")
    validate_elf(files[LIBRARY])
    # Re-inspect the bytes in the archive, not an unrelated input path.
    with tempfile.TemporaryDirectory(prefix="panvk-verify-", dir=os.environ.get("TMPDIR")) as tmp:
        lib = Path(tmp) / LIBRARY
        lib.write_bytes(files[LIBRARY])
        if inspect_binary(lib) != manifest["needed"]:
            raise ValueError("manifest dependencies mismatch")
    return manifest


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("library", type=Path, nargs="?")
    p.add_argument("--ndk", type=Path)
    p.add_argument("--source-revision", help="full revision used to compile the supplied binary")
    p.add_argument("--out", type=Path, default=ROOT / "dist/exynos2100")
    p.add_argument("--verify", type=Path)
    a = p.parse_args()
    if a.verify:
        print(json.dumps(verify(a.verify), indent=2)); return
    if not a.library or not a.ndk or not a.source_revision:
        p.error("library, --ndk and --source-revision are required")
    revision = git("rev-parse", a.source_revision + "^{commit}")
    if revision != a.source_revision:
        p.error("supply the full source commit hash")
    subprocess.run(["git", "-C", str(ROOT), "merge-base", "--is-ancestor", UPSTREAM, revision], check=True)
    needed = inspect_binary(a.library)
    ndk_props = dict(re.findall(r"(?m)^(\S+)\s*=\s*(.+)$", (a.ndk / "source.properties").read_text()))
    version = (ROOT / "VERSION").read_text().strip()
    manifest = {"schema": 1, "source_revision": revision, "packaging_revision": git("rev-parse", "HEAD"),
                "packaging_dirty": bool(git("status", "--porcelain", "--untracked-files=no")),
                "upstream_url": "https://github.com/JimVulkan/mali-panvk", "upstream_revision": UPSTREAM,
                "fork_url": "https://github.com/Pro-Termux-arch/mali-panvk",
                "ndk_revision": ndk_props["Pkg.Revision"], "target": "aarch64-linux-android31",
                "mesa_version": version, "library_sha256": sha(a.library.read_bytes()), "needed": needed,
                "hardware_validation": "See docs/exynos2100/RESULTS.md; package metadata is not a test result."}
    meta = {"schemaVersion": 1, "name": f"Exynos2100 G78 PanVK {revision[:12]}",
            "description": "Experimental isolated G78 JM kbase driver; not a KernelSU or system update.",
            "author": "JimVulkan, Mesa authors; integration by Pro-Termux-arch",
            "packageVersion": git("rev-list", "--count", revision), "vendor": "Mesa",
            "driverVersion": f"Mesa {version} (experimental)", "minApi": 31, "libraryName": LIBRARY}
    files = {"meta.json": (json.dumps(meta, indent=2) + "\n").encode(), LIBRARY: a.library.read_bytes(),
             "NOTICE.txt": (ROOT / "android/NOTICE.txt").read_bytes(),
             "FORK-NOTICE.txt": (ROOT / "tools/exynos2100/NOTICE.txt").read_bytes(),
             "build-manifest.json": (json.dumps(manifest, sort_keys=True, indent=2) + "\n").encode()}
    files["SHA256SUMS"] = "".join(f"{sha(data)}  {name}\n" for name, data in sorted(files.items())).encode()
    epoch = int(os.environ.get("SOURCE_DATE_EPOCH", git("show", "-s", "--format=%ct", revision)))
    a.out.mkdir(parents=True, exist_ok=True)
    output = a.out / f"exynos2100-panvk-{revision[:12]}.zip"
    output.write_bytes(archive_bytes(files, epoch))
    verify(output)
    digest = sha(output.read_bytes())
    output.with_suffix(".zip.sha256").write_text(f"{digest}  {output.name}\n")
    print(f"{output}\nSHA256 {digest}")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError, zipfile.BadZipFile) as exc:
        raise SystemExit(f"error: {exc}")
