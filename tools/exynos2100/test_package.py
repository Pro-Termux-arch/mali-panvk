# SPDX-License-Identifier: MIT
import importlib.util
import io
from pathlib import Path
import struct
import unittest
import zipfile

spec = importlib.util.spec_from_file_location("device_package", Path(__file__).with_name("package.py"))
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


def minimal_elf(machine=183, name=b".text"):
    data = bytearray(256)
    data[:7] = b"\x7fELF\x02\x01\x01"
    struct.pack_into("<HH", data, 16, 3, machine)
    struct.pack_into("<Q", data, 40, 64)
    struct.pack_into("<6H", data, 52, 64, 0, 0, 64, 2, 1)
    names = b"\0.shstrtab\0" + name + b"\0"
    struct.pack_into("<IIQQQQIIQQ", data, 128, 1, 3, 0, 0, 192, len(names), 0, 0, 1, 0)
    data[192:192+len(names)] = names
    return data


class PackageTests(unittest.TestCase):
    def test_aarch64(self):
        package.validate_elf(minimal_elf())

    def test_reject_x86(self):
        with self.assertRaises(ValueError): package.validate_elf(minimal_elf(machine=62))

    def test_reject_truncation(self):
        for n in (0, 4, 63, 64, 100, 190):
            with self.subTest(n=n), self.assertRaises(ValueError):
                package.validate_elf(minimal_elf()[:n])

    def test_reject_out_of_range_string_table(self):
        data = minimal_elf()
        struct.pack_into("<Q", data, 128+24, 999999)
        with self.assertRaises(ValueError): package.validate_elf(data)

    def test_reject_debug_sections(self):
        data = minimal_elf(name=b".debug_info")
        struct.pack_into("<II", data, 64, 11, 1)
        with self.assertRaises(ValueError): package.validate_elf(data)

    def test_deterministic_and_fixed_permissions(self):
        a = package.archive_bytes({"b": b"two", "a": b"one"}, 1700000000)
        b = package.archive_bytes({"a": b"one", "b": b"two"}, 1700000000)
        self.assertEqual(a, b)
        with zipfile.ZipFile(io.BytesIO(a)) as z:
            self.assertEqual(z.namelist(), ["a", "b"])
            self.assertEqual(z.getinfo("a").external_attr >> 16, 0o100644)


if __name__ == "__main__":
    unittest.main()
