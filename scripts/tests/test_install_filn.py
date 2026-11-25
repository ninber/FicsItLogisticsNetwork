"""Installer regression: changed old-dated headers must invalidate generated code.
No Unreal, compiler or game is executed by these tests.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import unittest

SCRIPTS = Path(__file__).resolve().parents[1]


class InstallTimestampTests(unittest.TestCase):
    def test_changed_headers_are_newer_than_generated_output_and_noop_is_stable(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            scripts = root / 'scripts'
            scripts.mkdir()
            shutil.copy2(SCRIPTS / 'install-filn.sh', scripts)
            workspace = root / 'workspace'
            project = workspace / 'StarterProject'
            source = workspace / 'mods/FicsItLogisticsNetwork'
            target = project / 'Mods/FicsItLogisticsNetwork'
            for folder in ('Source', 'Config', 'tests', 'docs'):
                (source / folder).mkdir(parents=True)
            for name in ('FicsItLogisticsNetwork.uplugin', 'README.md', 'LICENSE.txt', '.gitignore'):
                (source / name).write_text('original')
            project.mkdir(parents=True)
            (project / 'FactoryGame.uproject').write_text('{}')
            header = source / 'Source/Selector.h'
            header.write_text('old')
            def install():
                subprocess.run(['bash', str(scripts / 'install-filn.sh')], check=True, capture_output=True)
            install()
            installed = target / 'Source/Selector.h'
            generated = target / 'Intermediate/Selector.generated.h'
            generated.parent.mkdir()
            generated.write_text('old generated code')
            earlier = time.time() - 3600
            os.utime(generated, (earlier, earlier))
            os.utime(installed, (earlier - 10, earlier - 10))
            original = installed.stat().st_mtime_ns
            # No content change must not refresh build inputs.
            install()
            self.assertEqual(installed.stat().st_mtime_ns, original)
            header.write_text('new reflected declaration')
            os.utime(header, (earlier - 100, earlier - 100))
            install()
            self.assertEqual(installed.read_text(), header.read_text())
            self.assertGreater(installed.stat().st_mtime_ns, generated.stat().st_mtime_ns)
            stamp = installed.stat().st_mtime_ns
            install()
            self.assertEqual(installed.stat().st_mtime_ns, stamp)
            self.assertEqual(generated.read_text(), 'old generated code')
            self.assertEqual((target / 'README.md').read_text(), 'original')


if __name__ == '__main__':
    unittest.main()
