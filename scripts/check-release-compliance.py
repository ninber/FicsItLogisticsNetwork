#!/usr/bin/env python3
"""Source-tree guard; not an asset-rights or shipping-game certification."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
tracked = subprocess.check_output(['git','ls-files','-z'], cwd=ROOT).decode().split('\0')
bad = []
for name in filter(None, tracked):
    p = Path(name)
    parts = [part.lower() for part in p.parts]
    if ('icons' in parts and p.suffix.lower() in {'.png','.jpg','.jpeg','.webp'}) or ('cache' in parts and 'icons' in parts):
        bad.append(name)
for name in ('workspace/PUBLICATION.md','workspace/experiments/FILNLua/dashboard/icons/unknown.svg'):
    if not (ROOT/name).is_file():
        bad.append('Missing '+name)
if bad:
    sys.exit('FAIL source compliance:\n'+'\n'.join(bad))
print('PASS tracked icon/cache exclusions and disclosure presence; shipping and rights review still required.')
