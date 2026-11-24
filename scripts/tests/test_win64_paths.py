"""Short aliases must preserve existing files and resolve to the configured toolchain."""
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / 'win64-paths.sh'


class Win64PathsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.engine = self.root / 'engine'
        self.project = self.root / 'project/FactoryGame.uproject'
        self.engine.mkdir()
        (self.engine / 'Engine/Source').mkdir(parents=True)
        self.project.parent.mkdir()
        self.project.write_text('{}')
        key = hashlib.sha256(f'{self.engine}\n{self.project}'.encode()).hexdigest()[:10]
        self.alias = Path(f'/tmp/filn-win64-{os.getuid()}-{key}')
        self.assertFalse(self.alias.exists())
        self.env = dict(os.environ, UE_ROOT=str(self.engine), SATISFACTORY_PROJECT=str(self.project))

    def tearDown(self):
        if self.alias.is_symlink():
            self.alias.unlink()
        elif self.alias.exists():
            shutil.rmtree(self.alias)
        self.temp.cleanup()

    def run_paths(self):
        return subprocess.run(['bash', '-c', 'set -euo pipefail; source "$1"',
                               'filn-path-test', str(SCRIPT)], env=self.env,
                              capture_output=True, text=True, timeout=10)

    def test_short_paths_resolve_to_originals_and_can_be_reused(self):
        result = self.run_paths()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.alias / 'e').resolve(), self.engine)
        self.assertEqual((self.alias / 'p/FactoryGame.uproject').resolve(), self.project)
        inode = (self.alias / 'e').lstat().st_ino
        self.assertEqual(self.run_paths().returncode, 0)
        self.assertEqual((self.alias / 'e').lstat().st_ino, inode)

    def test_conflicting_alias_is_preserved(self):
        self.alias.mkdir(mode=0o700)
        existing = self.alias / 'e'
        existing.write_text('preserve')
        result = self.run_paths()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(existing.read_text(), 'preserve')
        self.assertFalse((self.alias / 'p').exists())

    def test_symlink_alias_directory_is_rejected(self):
        self.alias.symlink_to(self.root, target_is_directory=True)
        result = self.run_paths()
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.root / 'e').exists())

    def run_launcher(self, cwd):
        self.assertEqual(self.run_paths().returncode, 0)
        fake = self.root / 'wine-test'
        fake.write_text('#!/bin/bash\nprintf "%s\\n" "$PWD" "$@"\nexit 47\n')
        fake.chmod(0o755)
        env = dict(self.env, FILN_REAL_WINE_COMMAND=str(fake),
                   FILN_WIN64_ENGINE_SOURCE=str(self.alias / 'e/Engine/Source'))
        return subprocess.run([str(SCRIPT.with_name('win64-wine.sh')), 'one two', 'three'],
                              cwd=cwd, env=env, text=True, capture_output=True, timeout=10)

    def test_launcher_preserves_short_logical_cwd_arguments_and_exit_code(self):
        result = self.run_launcher(self.engine / 'Engine/Source')
        self.assertEqual(result.returncode, 47)
        self.assertEqual(result.stdout.splitlines(),
                         [str(self.alias / 'e/Engine/Source'), 'one two', 'three'])

    def test_launcher_does_not_change_unrelated_working_directory(self):
        result = self.run_launcher(self.root)
        self.assertEqual(result.returncode, 47)
        self.assertEqual(result.stdout.splitlines(), [str(self.root), 'one two', 'three'])


if __name__ == '__main__':
    unittest.main()
