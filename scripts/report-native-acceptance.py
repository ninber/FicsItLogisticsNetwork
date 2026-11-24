#!/usr/bin/env python3
"""Report unresolved runtime acceptance; never infer integration success from pure tests."""
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[1]
manifest = root / 'workspace/specs/014-native-completion/acceptance.json'
cases = json.loads(manifest.read_text())['cases']
failed = 0
for case in cases:
    evidence = case.get('evidence', [])
    verified = case['status'] == 'verified' and evidence and all((root / path).is_file() for path in evidence)
    print(('PASS ' if verified else 'FAIL ') + case['id'] + ': ' + case.get('reason', ''))
    failed += not verified
print(f'{failed}/{len(cases)} runtime/release acceptance gates unresolved; this reports evidence, not simulated gameplay.')
sys.exit(bool(failed))
