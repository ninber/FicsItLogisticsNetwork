#!/usr/bin/env python3
"""Fail closed when an Alpakit archive is missing native release evidence."""
import json
import re
from pathlib import Path, PurePosixPath
import sys
import zipfile

if len(sys.argv) not in (2, 4) or (len(sys.argv) == 4 and sys.argv[2] != '--expected-descriptor'):
    sys.exit('Usage: python3 scripts/verify-native-package.py <Alpakit ZIP> [--expected-descriptor <source .uplugin>]')
try:
    with zipfile.ZipFile(sys.argv[1]) as archive:
        bad = archive.testzip()
        if bad:
            raise ValueError('CRC failure: ' + bad)
        names = archive.namelist()
        if len({name.casefold() for name in names}) != len(names):
            raise ValueError('Duplicate or case-colliding archive paths')
        for name in names:
            p = PurePosixPath(name)
            if p.is_absolute() or '..' in p.parts or '\\' in name or ':' in name:
                raise ValueError('Unsafe archive path: ' + name)
            if any(part.lower() in {'cache', 'icons'} for part in p.parts) and p.suffix.lower() in {'.png','.jpg','.jpeg','.webp'}:
                raise ValueError('Runtime-only icon in release: ' + name)
            if any(part.lower() in {'source', 'intermediate', 'tests', '.git', 'node_modules'} for part in p.parts) or p.suffix.lower() in {'.lua', '.py', '.cpp', '.h'}:
                raise ValueError('Development/legacy dependency file: ' + name)
        descriptors = [name for name in names if name.endswith('/FicsItLogisticsNetwork.uplugin') or name == 'FicsItLogisticsNetwork.uplugin']
        if len(descriptors) != 1:
            raise ValueError('Expected exactly one FILN descriptor')
        meta = json.loads(archive.read(descriptors[0]))
        if len(sys.argv) == 4:
            expected = json.loads(Path(sys.argv[3]).read_text(encoding='utf-8'))
            for key in ('FileVersion', 'Version', 'VersionName', 'SemVersion', 'GameVersion', 'Plugins'):
                if key not in expected or meta.get(key) != expected[key]:
                    raise ValueError(f'Archive descriptor mismatch for {key}: expected {expected.get(key)!r}, got {meta.get(key)!r}')
            print('PASS archive descriptor matches current source version and dependency metadata')
        deps = meta.get('Plugins', [])
        if (not isinstance(deps, list) or len(deps) != 1 or not isinstance(deps[0], dict)
                or deps[0].get('Name') != 'SML' or deps[0].get('Enabled') is not True
                or deps[0].get('Optional', False) or not deps[0].get('SemVersion')):
            raise ValueError('Expected required versioned SML-only dependency; no optional third-party dependencies')
        if not meta.get('GameVersion') or not meta.get('SemVersion'):
            raise ValueError('Missing game/mod version metadata')
        # Match the published SMR schema, not the broader SML range grammar.
        # https://github.com/satisfactorymodding/smr-api/blob/main/static/uplugin-json-schema.json
        if type(meta.get('FileVersion')) is not int or meta['FileVersion'] != 3 or type(meta.get('Version')) is not int:
            raise ValueError('Invalid SMR FileVersion/Version metadata')
        if not isinstance(meta.get('GameVersion'), str):
            raise ValueError('Invalid SMR GameVersion metadata')
        if not isinstance(meta.get('SemVersion'), str) or not re.fullmatch(r'^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-((?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?(?:\+([0-9a-zA-Z-]+(?:\.[0-9a-zA-Z-]+)*))?$', meta['SemVersion']):
            raise ValueError('Invalid SMR mod SemVersion')
        if meta['Version'] != int(meta['SemVersion'].split('.', 1)[0]):
            raise ValueError('SemVer major version should match Version')
        if not isinstance(deps[0]['SemVersion'], str) or not re.fullmatch(r'^(<=|<|>|>=|\^)?(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-((?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?(?:\+([0-9a-zA-Z-]+(?:\.[0-9a-zA-Z-]+)*))?$', deps[0]['SemVersion']):
            raise ValueError('Invalid SMR dependency SemVersion: use one comparison operator or exact version')
        binaries = [name for name in names if '/Binaries/Win64/' in '/' + name and 'FicsItLogisticsNetwork' in name and name.endswith('.dll')]
        if not binaries:
            raise ValueError('Missing FILN Win64 DLL')
        for name in binaries:
            data = archive.read(name)
            if len(data) < 64 or data[:2] != b'MZ':
                raise ValueError('Invalid PE DOS header: ' + name)
            offset = int.from_bytes(data[60:64], 'little')
            if offset < 64 or offset + 24 > len(data) or data[offset:offset+6] != b'PE\0\0d\x86':
                raise ValueError('Expected x64 PE binary: ' + name)
            sections = int.from_bytes(data[offset+6:offset+8], 'little')
            optional_size = int.from_bytes(data[offset+20:offset+22], 'little')
            flags = int.from_bytes(data[offset+22:offset+24], 'little')
            optional = offset + 24
            table = optional + optional_size
            if (not 1 <= sections <= 96 or optional_size < 112 or
                    table + sections * 40 > len(data) or
                    data[optional:optional+2] != b'\x0b\x02' or flags & 0x2002 != 0x2002):
                raise ValueError('Invalid x64 PE DLL structure: ' + name)
            for index in range(sections):
                section = table + index * 40
                size = int.from_bytes(data[section+16:section+20], 'little')
                start = int.from_bytes(data[section+20:section+24], 'little')
                if size and (start < table + sections * 40 or start + size > len(data)):
                    raise ValueError('Truncated/invalid PE section: ' + name)
        cooked = [name for name in names if PurePosixPath(name).suffix.lower() in {'.pak', '.utoc'} and 'ficsitlogisticsnetwork' in name.casefold()]
        if not cooked:
            raise ValueError('Missing cooked FILN content container (dedicated depot and selector assets required)')
        print('PASS archive integrity, metadata, dependency list, Win64 PE, cooked container presence and file exclusions')
        print('Cooked asset completeness, SMM loading and runtime compatibility require independent acceptance.')
except (OSError, ValueError, KeyError, zipfile.BadZipFile) as exc:
    sys.exit('FAIL: ' + str(exc))
