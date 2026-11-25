"""Source boundary regression only; not evidence of shipping runtime safety."""
from pathlib import Path
import re
import unittest

SOURCE = Path(__file__).resolve().parents[2] / 'workspace/mods/FicsItLogisticsNetwork/Source/FicsItLogisticsNetwork'


class NativeCapacityCallTests(unittest.TestCase):
    def test_no_direct_nullable_slot_size_calls(self):
        calls = []
        for file in SOURCE.rglob('*'):
            if file.suffix in ('.h', '.cpp'):
                calls.extend((file.name, m.group(1)) for m in re.finditer(r'->GetSlotSizeForItem\s*\(([^;]*)\)', file.read_text()))
        self.assertEqual(calls, [('FILNInventoryCapacity.h', 'Slot,Resource,&Item')])

    def test_every_capacity_consumer_uses_shared_adapter(self):
        for file in ('LogisticsDispatch.cpp', 'LogisticsFuel.cpp', 'LogisticsTelemetry.cpp'):
            self.assertIn('filn::inventory::TryCapacity(', (SOURCE/'Private'/file).read_text())


if __name__ == '__main__':
    unittest.main()
