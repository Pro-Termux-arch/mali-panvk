# SPDX-License-Identifier: MIT
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("mkabi", ROOT / "android/gralloc/mkabi.py")
mkabi = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mkabi)


class AbiTests(unittest.TestCase):
    def test_only_namespace_changes(self):
        original = "// license\n#define _LIBCPP_HAS_NO_THREADS\n#define _LIBCPP_ABI_NAMESPACE __ndk1\n"
        result = mkabi.platform_config(original)
        self.assertEqual(result, original.replace("__ndk1", "__1 /* platform ABI for gralloc only */"))

    def test_fail_closed_for_changed_or_duplicate_configuration(self):
        for text in ("", "#define _LIBCPP_ABI_NAMESPACE __1\n",
                     "#define _LIBCPP_ABI_NAMESPACE __ndk1\n" * 2):
            with self.assertRaises(ValueError):
                mkabi.platform_config(text)

    def test_installed_ndk_if_available(self):
        # A portable test need not have a particular NDK installed.
        import os
        ndk = os.environ.get("ANDROID_NDK_HOME")
        if not ndk:
            self.skipTest("ANDROID_NDK_HOME not provided")
        text = (Path(ndk) / "toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/c++/v1/__config_site").read_text()
        result = mkabi.platform_config(text)
        self.assertIn("_LIBCPP_ABI_NAMESPACE __1", result)
        self.assertNotIn("_LIBCPP_ABI_NAMESPACE __ndk1", result)


if __name__ == "__main__":
    unittest.main()
