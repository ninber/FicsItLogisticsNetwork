#!/usr/bin/env python3
"""Validate configured Wwise paths and target libraries without GUI or generation."""
import json
import os
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

root = Path(os.environ['SATISFACTORY_PROJECT']).resolve().parent
config = (root / 'Config/DefaultGame.ini').read_text()
errors = []
def check(condition, message):
    print(('OK      ' if condition else 'FAIL    ') + message)
    if not condition:
        errors.append(message)

def configured(key, base):
    match = re.search(r'^' + key + r'=\((?:FilePath|Path)="([^"]+)"\)', config, re.M)
    check(bool(match), key + ' configured')
    return (base / match[1]).resolve() if match else base / '__missing__'

project = configured('WwiseProjectPath', root)
banks = configured('RootOutputPath', root / 'Content')
check(project.is_file(), str(project))
if project.is_file():
    try:
        ET.parse(project)
        check(True, 'Wwise project XML parses')
    except ET.ParseError as exc:
        check(False, 'Wwise project XML: ' + str(exc))
check(banks.is_dir(), str(banks))
check((banks / 'ProjectInfo.json').is_file(), 'GeneratedSoundBanks/ProjectInfo.json')
for platform in ['Windows']:
    bank = banks / platform / 'Init.bnk'
    check(bank.is_file() and bank.stat().st_size > 8 and bank.read_bytes()[:4] == b'BKHD', str(bank))
    for name in ['PlatformInfo.json', 'PluginInfo.json', 'Init.json']:
        path = banks / platform / name
        try:
            json.loads(path.read_text())
            check(True, str(path))
        except (OSError, ValueError) as exc:
            check(False, str(path) + ': ' + str(exc))
third = root / 'Plugins/WwiseSoundEngine/ThirdParty'
for path in ['include/AK/AkWwiseSDKVersion.h', 'Linux_x64/Profile/lib/libAkSoundEngine.a', 'Linux_x64/Release/lib/libAkSoundEngine.a', 'x64_vc170/Profile/lib/AkSoundEngine.lib', 'x64_vc170/Release/lib/AkSoundEngine.lib']:
    check((third / path).is_file(), str(third / path))
for plugin in ['Wwise', 'WwiseSoundEngine']:
    path = root / 'Plugins' / plugin / (plugin + '.uplugin')
    try:
        meta = json.loads(path.read_text())
        print(plugin, meta.get('VersionName', 'version unspecified'))
    except (OSError, ValueError) as exc:
        check(False, str(exc))
sys.exit(bool(errors))
