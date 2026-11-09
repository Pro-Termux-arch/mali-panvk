#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Pro-Termux-arch
"""Derive the platform libc++ namespace overlay from the selected NDK.

Do not mix a newer __config_site with an older libc++: macro conventions changed
between NDK releases, including positive vs negative feature macros. Keep every
configuration choice from this toolchain, changing only __ndk1 to __1 for the
gralloc C++ translation unit that calls platform libui.
"""
from pathlib import Path
import re
import sys


def platform_config(text):
    pattern = r"(?m)^(#[ \t]*define[ \t]+_LIBCPP_ABI_NAMESPACE[ \t]+)__ndk1[ \t]*$"
    result, count = re.subn(pattern, r"\g<1>__1 /* platform ABI for gralloc only */", text)
    if count != 1:
        raise ValueError("expected exactly one NDK __ndk1 namespace definition")
    return result


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: mkabi.py <NDK __config_site> <output overlay directory>")
    source, out = map(Path, sys.argv[1:])
    try:
        text = platform_config(source.read_text())
    except (OSError, ValueError) as exc:
        sys.exit(f"error: {exc}")
    out.mkdir(parents=True, exist_ok=True)
    (out / "__config_site").write_text(text)


if __name__ == "__main__":
    main()
