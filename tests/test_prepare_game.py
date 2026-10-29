"""Synthetic metadata/data only; no original game content."""
import importlib.util
import struct
import tempfile
import unittest
import zipfile
from pathlib import Path

spec = importlib.util.spec_from_file_location(
    'prepare_game', Path(__file__).resolve().parents[1] / 'tools/prepare_game.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class TransferTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.game = self.root / 'game'
        (self.game / 'gen').mkdir(parents=True)
        data = bytearray(64)
        struct.pack_into('>6I', data, 0, 0x58455832, 1, 64, 0, 0, 1)
        struct.pack_into('>2I', data, 24, 0x00040006, 32)
        struct.pack_into('>I', data, 44, 0x454108B1)
        (self.game / 'default.xex').write_bytes(data)
        (self.game / 'gen/main_xbox.hdr').write_bytes(b'synthetic header')
        (self.game / 'gen/part.ark').write_bytes(b'A' * 1100000)
        (self.game / 'gen/second.ark').write_bytes(b'B' * 100)

    def tearDown(self):
        self.temp.cleanup()

    def test_round_trip_and_no_overwrite(self):
        original = {p.relative_to(self.game).as_posix(): p.read_bytes()
                    for p in self.game.rglob('*') if p.is_file()}
        output = self.root / 'transfer'
        manifest = module.package(self.game, output, 1)
        self.assertEqual(manifest['file_count'], len(original))
        recovered = {}
        for record in manifest['archives']:
            with zipfile.ZipFile(output / record['name']) as archive:
                self.assertIsNone(archive.testzip())
                for name in archive.namelist():
                    prefix = 'Games/xbox360/Beatles-Rock-Band/'
                    self.assertTrue(name.startswith(prefix))
                    recovered[name[len(prefix):]] = archive.read(name)
        self.assertEqual(original, recovered)
        for name, contents in original.items():
            self.assertEqual((self.game / name).read_bytes(), contents)
        before = (output / 'transfer-manifest.json').read_bytes()
        with self.assertRaises(ValueError):
            module.package(self.game, output, 1)
        self.assertEqual(before, (output / 'transfer-manifest.json').read_bytes())

    def test_nested_output_and_invalid_title_rejected(self):
        with self.assertRaises(ValueError):
            module.package(self.game, self.game / 'output')
        data = bytearray((self.game / 'default.xex').read_bytes())
        struct.pack_into('>I', data, 44, 0x12345678)
        (self.game / 'default.xex').write_bytes(data)
        with self.assertRaises(ValueError):
            module.package(self.game, self.root / 'output')
        self.assertFalse((self.root / 'output').exists())

    def test_missing_data_rejected(self):
        (self.game / 'gen/main_xbox.hdr').unlink()
        with self.assertRaises(ValueError):
            module.package(self.game, self.root / 'output')


if __name__ == '__main__':
    unittest.main()
