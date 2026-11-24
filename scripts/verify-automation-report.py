#!/usr/bin/env python3
"""Editor exit zero is not enough: require actual nonempty passing test evidence."""
import json
import sys
from pathlib import Path
try:
    report = json.loads(Path(sys.argv[1]).read_text(encoding='utf-8-sig'))
    required = {'FILN.NativeInventory.Solid', 'FILN.NativeInventory.Fluid'}
    results = {test['fullTestPath']: test['state'] for test in report['tests']}
    if not required.issubset(results) or any(results[name] != 'Success' for name in required):
        sys.exit('FAIL: both required native inventory cases must have actual Success results')
    failed = int(report['failed'])
    passed = int(report['succeeded'])
    pending = int(report.get('notRun', 0)) + int(report.get('inProcess', 0))
    if failed or pending or passed < 2:
        sys.exit(f'FAIL: native automation succeeded={passed} failed={failed} incomplete={pending}')
    print(f'PASS: {passed} native inventory integration tests; other runtime gates remain independent')
except (OSError, ValueError, KeyError, IndexError, TypeError) as exc:
    sys.exit('FAIL: missing/invalid native automation evidence: ' + str(exc))
