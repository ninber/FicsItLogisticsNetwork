#!/usr/bin/env python3
"""Report unresolved runtime acceptance; never infer integration success from pure tests."""
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[1]
manifest = root / 'workspace/specs/014-native-completion/acceptance.json'
cases = json.loads(manifest.read_text())['cases']
failed = 0
excluded = 0
for case in cases:
    if case['status'] in ('deferred', 'out-of-scope'):
        print('SCOPE EXCLUDED ' + case['id'] + ': ' + case.get('reason', ''))
        excluded += 1
        continue
    evidence = case.get('evidence', [])
    verified = case['status'] == 'verified' and evidence and all((root / path).is_file() for path in evidence)
    print(('PASS ' if verified else 'FAIL ') + case['id'] + ': ' + case.get('reason', ''))
    failed += not verified
print(f'{failed}/{len(cases)-excluded} in-scope runtime/release acceptance gates unresolved ({excluded} excluded, not passed); this reports evidence, not simulated gameplay.')
sys.exit(bool(failed))
