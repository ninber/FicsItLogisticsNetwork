"""CLI-output classification only; these tests do NOT verify actual Wine or Vulkan."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / 'probe-dev-container.sh'
class DevProbeClassificationTests(unittest.TestCase):
    def probe(self, vulkan_output):
        with tempfile.TemporaryDirectory() as temp:
            for name, output in {'clang':'clang version 18', 'wine64':'wine-10', 'vulkaninfo':vulkan_output}.items():
                path=Path(temp)/name
                path.write_text('#!/usr/bin/python3\nprint('+repr(output)+')\n')
                path.chmod(0o755)
            env=os.environ.copy(); env['PATH']=temp+os.pathsep+env['PATH']
            return subprocess.run(['bash',str(SCRIPT)],env=env,capture_output=True,text=True)
    def test_software_only_is_rejected(self):
        result=self.probe('deviceType = PHYSICAL_DEVICE_TYPE_CPU\ndeviceName = llvmpipe')
        self.assertNotEqual(result.returncode,0)
    def test_loader_error_plus_other_discrete_gpu_is_rejected(self):
        result=self.probe('ERROR: NVIDIA ICD unavailable\ndeviceType = PHYSICAL_DEVICE_TYPE_DISCRETE_GPU\ndeviceName = AMD Radeon')
        self.assertNotEqual(result.returncode,0)
    def test_enumerated_nvidia_device_is_accepted(self):
        result=self.probe('deviceType = PHYSICAL_DEVICE_TYPE_DISCRETE_GPU\ndeviceName = NVIDIA GeForce RTX 3090')
        self.assertEqual(result.returncode,0,result.stderr)
if __name__=='__main__':
    unittest.main()
