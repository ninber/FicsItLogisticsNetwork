"""Execution routing tests; no engine, Wine, container or game is launched."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

SCRIPTS = Path(__file__).resolve().parents[1]


class HeadlessLocalTests(unittest.TestCase):
    def run_local(self, *args, compiler_failure=None):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            scripts = root / 'scripts'
            scripts.mkdir()
            for name in ('headless.sh', 'env.sh', 'verify-windows-sdk.sh'):
                shutil.copy2(SCRIPTS / name, scripts / name)
            binaries = root / 'bin'
            binaries.mkdir()
            for name in ('bash', 'dirname', 'mkdir', 'flock', 'touch', 'mktemp', 'cat', 'timeout'):
                (binaries / name).symlink_to(shutil.which(name))
            marker = root / 'container-invoked'
            for name in ('podman', 'distrobox'):
                path = binaries / name
                path.write_text(f'#!/bin/bash\ntouch "{marker}"\nexit 99\n')
                path.chmod(0o755)
            # Catch delegation even if the wrapper would fail before invoking tools.
            (scripts / 'dev-exec.sh').write_text(f'#!/bin/bash\ntouch "{marker}"\nexit 99\n')
            env = os.environ.copy()
            env['PATH'] = str(binaries)
            if compiler_failure is not None:
                wine = binaries / 'wine64'
                wine.write_text('#!/bin/bash\nexit 98\n')
                wine.chmod(0o755)
                compiler = root / 'msvc/bin/x64/cl'
                compiler.parent.mkdir(parents=True)
                compiler.write_text(
                    '#!/bin/bash\n'
                    '[[ "$WINE_MSVC_RAW_STDOUT" == 1 ]] || exit 97\n'
                    'echo "wineserver: bind: Operation not permitted" >&2\n'
                    f'exit {compiler_failure}\n')
                compiler.chmod(0o755)
                env['UE_WINE_MSVC'] = str(root / 'msvc')
            result = subprocess.run(['/bin/bash', str(scripts / 'headless.sh'), *args],
                                    env=env, text=True, capture_output=True, timeout=10)
            self.assertFalse(marker.exists(), result.stderr)
            return result

    def test_sdk_missing_local_wine_is_blocked_without_delegation(self):
        result = self.run_local('sdk')
        self.assertEqual(result.returncode, 69, result.stderr)
        self.assertIn('BLOCKED:', result.stderr)

    def test_package_missing_local_wine_is_blocked_without_delegation(self):
        result = self.run_local('package')
        self.assertEqual(result.returncode, 69, result.stderr)
        self.assertIn('BLOCKED:', result.stderr)

    def test_package_rejects_launch_overrides_before_toolchain_probe(self):
        result = self.run_local('package', '-launch')
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertIn('deployment/launch is deliberately excluded', result.stderr)

    def test_sdk_compiler_failure_returns_diagnostics_without_fifo_wait(self):
        result = self.run_local('sdk', compiler_failure=42)
        self.assertEqual(result.returncode, 42, result.stderr)
        self.assertIn('wineserver: bind: Operation not permitted', result.stderr)
        self.assertIn('SDK compiler check exited 42', result.stderr)

    def test_package_stops_at_failed_compiler_check(self):
        result = self.run_local('package', compiler_failure=43)
        self.assertEqual(result.returncode, 43, result.stderr)
        self.assertIn('SDK compiler check exited 43', result.stderr)


if __name__ == '__main__':
    unittest.main()
