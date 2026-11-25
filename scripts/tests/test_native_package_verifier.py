"""Archive verifier must reject a source-only handoff and invalid native binaries."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
import zipfile

SCRIPT = Path(__file__).resolve().parents[1] / 'verify-native-package.py'
class NativePackageVerifierTests(unittest.TestCase):
    def probe(self, files):
        with tempfile.TemporaryDirectory() as temp:
            archive = Path(temp) / 'test.zip'
            with zipfile.ZipFile(archive, 'w') as output:
                for name, data in files.items():
                    output.writestr(name, data)
            return subprocess.run(['python3', str(SCRIPT), str(archive)], capture_output=True, text=True)
    def test_source_handoff_is_not_release(self):
        result = self.probe({'Source/module.cpp': '// source'})
        self.assertNotEqual(result.returncode, 0)
    def test_descriptor_without_native_binary_is_not_release(self):
        meta = {'SemVersion':'0.1.0', 'GameVersion':'>=520140', 'Plugins':[{'Name':'SML','Enabled':True,'SemVersion':'>=3.12.1 <3.13.0'}]}
        result = self.probe({'FicsItLogisticsNetwork.uplugin':json.dumps(meta)})
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Missing FILN Win64 DLL', result.stderr)
    def test_third_party_dependency_rejected_even_optional_or_disabled(self):
        for enabled, optional in ((True, False), (True, True), (False, False)):
            with self.subTest(enabled=enabled, optional=optional):
                meta = {'SemVersion':'0.1.0','GameVersion':'>=520140','Plugins':[{'Name':'SML','Enabled':True,'SemVersion':'>=3.12.1'},{'Name':'TruckStationSettings','Enabled':enabled,'Optional':optional,'SemVersion':'>=1.1.0'}]}
                result = self.probe({'FicsItLogisticsNetwork.uplugin':json.dumps(meta)})
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('SML-only dependency', result.stderr)
    def test_path_traversal_is_rejected(self):
        self.assertNotEqual(self.probe({'../outside':'bad'}).returncode, 0)
    def test_fake_dll_is_rejected(self):
        meta = {'SemVersion':'0.1.0','GameVersion':'>=520140','Plugins':[{'Name':'SML','Enabled':True,'SemVersion':'>=3.12.1'}]}
        result = self.probe({'FicsItLogisticsNetwork.uplugin':json.dumps(meta),'Binaries/Win64/FactoryGame-FicsItLogisticsNetwork-Win64-Shipping.dll':b'not a PE'})
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Invalid PE', result.stderr)
    def test_signature_only_dll_is_rejected(self):
        meta = {'SemVersion':'0.1.0','GameVersion':'>=520140','Plugins':[{'Name':'SML','Enabled':True,'SemVersion':'>=3.12.1'}]}
        data = bytearray(128)
        data[:2] = b'MZ'
        data[60:64] = (64).to_bytes(4, 'little')
        data[64:70] = b'PE\0\0d\x86'
        result = self.probe({'FicsItLogisticsNetwork.uplugin':json.dumps(meta),'Binaries/Win64/FactoryGame-FicsItLogisticsNetwork-Win64-Shipping.dll':data})
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Invalid x64 PE DLL structure', result.stderr)
    def test_valid_pe_without_cooked_content_is_rejected(self):
        meta = {'SemVersion':'0.1.0','GameVersion':'>=520140','Plugins':[{'Name':'SML','Enabled':True,'SemVersion':'>=3.12.1'}]}
        data = bytearray(512)
        data[:2] = b'MZ'
        data[60:64] = (64).to_bytes(4, 'little')
        data[64:70] = b'PE\0\0d\x86'
        data[70:72] = (1).to_bytes(2, 'little')
        data[84:86] = (112).to_bytes(2, 'little')
        data[86:88] = (0x2002).to_bytes(2, 'little')
        data[88:90] = (0x20b).to_bytes(2, 'little')
        result = self.probe({'FicsItLogisticsNetwork.uplugin':json.dumps(meta),'Binaries/Win64/FactoryGame-FicsItLogisticsNetwork-Win64-Shipping.dll':data})
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Missing cooked FILN content container',result.stderr)
    def test_case_colliding_paths_are_rejected(self):
        result = self.probe({'Config/test.ini':'a','config/Test.ini':'b'})
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('case-colliding', result.stderr)
    def test_windows_drive_path_is_rejected(self):
        result = self.probe({'C:/outside':'bad'})
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Unsafe archive path', result.stderr)
if __name__ == '__main__':
    unittest.main()
