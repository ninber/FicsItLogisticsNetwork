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
    def test_path_traversal_is_rejected(self):
        self.assertNotEqual(self.probe({'../outside':'bad'}).returncode, 0)
    def test_fake_dll_is_rejected(self):
        meta = {'SemVersion':'0.1.0','GameVersion':'>=520140','Plugins':[{'Name':'SML','Enabled':True,'SemVersion':'>=3.12.1'}]}
        result = self.probe({'FicsItLogisticsNetwork.uplugin':json.dumps(meta),'Binaries/Win64/FactoryGame-FicsItLogisticsNetwork-Win64-Shipping.dll':b'not a PE'})
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Invalid PE', result.stderr)
if __name__ == '__main__':
    unittest.main()
