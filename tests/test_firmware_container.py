"""Validate archive boundaries and reject installers before extraction."""
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('firmware_prepare', ROOT / 'tools/prepare_browser_firmware.py')
FW = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FW)


def pbp(archive):
    return b'\0PBP' + struct.pack('<I8I', 0x10000, *([40] * 8)) + archive


class FirmwareContainerTest(unittest.TestCase):
    def test_elf_is_not_a_firmware_archive(self):
        self.assertEqual(FW.inspect(b'\x7fELF' + b'\0' * 40)['kind'], 'executable')

    def test_sony_archive_identification(self):
        self.assertEqual(FW.inspect(pbp(b'PSAR' + b'\0' * 40))['archive'], 'Sony PSAR')

    def test_ark_archive_inventory_and_truncation(self):
        archive = struct.pack('<IIB', 1, 3, 4) + b'test' + b'abc'
        result = FW.inspect(pbp(archive))
        self.assertEqual(result['archive'], 'ARK installer bundle')
        self.assertEqual(result['entries'], [{'name': 'test', 'bytes': 3}])
        self.assertEqual(FW.inspect(pbp(archive[:-1]))['archive'], 'unknown')
        self.assertEqual(FW.inspect(pbp(archive + b'garbage'))['archive'], 'unknown')

    def test_invalid_section_bounds(self):
        for offsets in ([39] * 8, [40] * 7 + [999], [40, 41, 40, 40, 40, 40, 40, 40]):
            with self.subTest(offsets=offsets), self.assertRaises(ValueError):
                FW.inspect(b'\0PBP' + struct.pack('<I8I', 0x10000, *offsets))
        with self.assertRaises(ValueError):
            FW.inspect(b'\0PBP')

    def test_unknown_firmware_never_invokes_decryptor(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'unknown.PBP'
            source.write_bytes(pbp(b'PSAR' + b'\0' * 40))
            output = root / 'output'
            with self.assertRaisesRegex(ValueError, 'MD5'):
                FW.extract(source, output, root / 'nonexistent-decryptor')
            self.assertFalse(output.exists())
