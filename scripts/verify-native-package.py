#!/usr/bin/env python3
"""Fail closed when an Alpakit archive is missing native release evidence."""
import json
from pathlib import PurePosixPath
import sys
import zipfile

if len(sys.argv) != 2:
    sys.exit('Usage: python3 scripts/verify-native-package.py <Alpakit ZIP>')
try:
    with zipfile.ZipFile(sys.argv[1]) as archive:
        bad = archive.testzip()
        if bad:
            raise ValueError('CRC failure: ' + bad)
        names = archive.namelist()
        for name in names:
            p = PurePosixPath(name)
            if p.is_absolute() or '..' in p.parts or '\\' in name:
                raise ValueError('Unsafe archive path: ' + name)
            if any(part.lower() in {'source', 'intermediate', 'tests', '.git', 'node_modules'} for part in p.parts) or p.suffix.lower() in {'.lua', '.py', '.cpp', '.h'}:
                raise ValueError('Development/legacy dependency file: ' + name)
        descriptors = [name for name in names if name.endswith('/FicsItLogisticsNetwork.uplugin') or name == 'FicsItLogisticsNetwork.uplugin']
        if len(descriptors) != 1:
            raise ValueError('Expected exactly one FILN descriptor')
        meta = json.loads(archive.read(descriptors[0]))
        deps = {dep['Name']: dep for dep in meta.get('Plugins', []) if dep.get('Enabled', False)}
        if set(deps) != {'SML'} or not deps['SML'].get('SemVersion'):
            raise ValueError('Expected versioned SML-only dependency')
        if not meta.get('GameVersion') or not meta.get('SemVersion'):
            raise ValueError('Missing game/mod version metadata')
        binaries = [name for name in names if '/Binaries/Win64/' in '/' + name and 'FicsItLogisticsNetwork' in name and name.endswith('.dll')]
        if not binaries:
            raise ValueError('Missing FILN Win64 DLL')
        for name in binaries:
            data = archive.read(name)
            if len(data) < 64 or data[:2] != b'MZ':
                raise ValueError('Invalid PE DOS header: ' + name)
            offset = int.from_bytes(data[60:64], 'little')
            if data[offset:offset+6] != b'PE\0\0d\x86':
                raise ValueError('Expected x64 PE binary: ' + name)
        print('PASS archive integrity, metadata, dependency list, Win64 PE and file exclusions')
        print('Cooked asset completeness, SMM loading and runtime compatibility require independent acceptance.')
except (OSError, ValueError, KeyError, zipfile.BadZipFile) as exc:
    sys.exit('FAIL: ' + str(exc))
