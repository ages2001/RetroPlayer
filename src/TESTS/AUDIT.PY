import runpy
import struct
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
audit = runpy.run_path(str(ROOT/'SCRIPTS'/'AUDIT.PY'))


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def test_short_package_names(self):
        (self.root/'PLUGINS').mkdir()
        (self.root/'PLUGINS'/'AVCODEC.DLL').touch()
        (self.root/'AVS-APL.BIN').touch()
        audit['audit_names'](self.root)

    def test_long_name_rejected(self):
        (self.root/'LIBVLCCORE.DLL').touch()
        with self.assertRaises(ValueError): audit['audit_names'](self.root)

    def test_case_collision_rejected(self):
        (self.root/'RETRO.EXE').touch()
        (self.root/'retro.exe').touch()
        with self.assertRaises(ValueError): audit['audit_names'](self.root)

    def test_dos_device_name_rejected(self):
        (self.root/'CON.TXT').touch()
        with self.assertRaises(ValueError): audit['audit_names'](self.root)

    def test_symlink_rejected(self):
        (self.root/'RETRO.EXE').symlink_to(ROOT/'DIST'/'RETRO.EXE')
        with self.assertRaises(ValueError): audit['audit_names'](self.root)

    def test_real_executables_have_no_crt_dependency(self):
        for name in ('RETRO.EXE', 'PROBE.EXE'):
            info = audit['pe_info'](ROOT/'DIST'/name)
            self.assertEqual(info['subsystem_version'], (3, 10))
            self.assertEqual(info['machine'], 'i386')
            self.assertEqual(set(d.upper() for d in info['imports']) -
                             {'KERNEL32.DLL', 'USER32.DLL', 'COMDLG32.DLL'}, set())

    def test_modern_subsystem_rejected(self):
        data = bytearray((ROOT/'DIST'/'RETRO.EXE').read_bytes())
        pe = struct.unpack_from('<I', data, 0x3c)[0]
        struct.pack_into('<HH', data, pe+24+48, 6, 0)
        path = self.root/'NEW.EXE'; path.write_bytes(data)
        with self.assertRaises(ValueError): audit['pe_info'](path)

    def test_wrong_architecture_rejected(self):
        data = bytearray((ROOT/'DIST'/'RETRO.EXE').read_bytes())
        pe = struct.unpack_from('<I', data, 0x3c)[0]
        struct.pack_into('<H', data, pe+4, 0x8664)
        path = self.root/'X64.EXE'; path.write_bytes(data)
        with self.assertRaises(ValueError): audit['pe_info'](path)

    def test_invalid_image_rejected(self):
        path = self.root/'BAD.EXE'; path.write_bytes(b'not an executable')
        with self.assertRaises(ValueError): audit['pe_info'](path)


if __name__ == '__main__':
    unittest.main(verbosity=2)
