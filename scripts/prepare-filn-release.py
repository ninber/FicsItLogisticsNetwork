#!/usr/bin/env python3
"""Assemble an allowlisted source handoff, never an installable SMM release."""
import hashlib
import json
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    native = ROOT / 'workspace/mods/FicsItLogisticsNetwork'
    lua = ROOT / 'workspace/experiments/FILNLua'
    files = {}

    def include(base, prefix, patterns):
        for pattern in patterns:
            for path in sorted(base.glob(pattern)):
                if path.is_file() and not path.is_symlink():
                    files[f'{prefix}/{path.relative_to(base).as_posix()}'] = path.read_bytes()

    include(native, 'native/FicsItLogisticsNetwork', [
        '*.uplugin', 'LICENSE.txt', 'README.md', 'Source/**/*.h',
        'Source/**/*.cpp', 'Source/**/*.cs', 'Config/*.ini', 'docs/*.md', 'tests/*.cpp'])
    include(lua, 'lua-experiment', [
        'Scripts/*.lua', 'DataForge/*.yml', 'FILNStations/*.yml',
        '*assets.json', 'kdataforge-asset.json', 'dashboard/**/*.html',
        'dashboard/**/*.css', 'dashboard/**/*.js', 'dashboard/icons/*.png',
        'dashboard/icons/manifest.json'])
    files['RELEASE.md'] = (ROOT / 'workspace/RELEASE.md').read_bytes()
    descriptor = json.loads((native / 'FicsItLogisticsNetwork.uplugin').read_text())
    report = {
        'kind': 'source-handoff', 'smm_ready': False,
        'native_version': descriptor['SemVersion'],
        'blockers': [
            'Lua dispatcher is a UE4SS experiment, not an SML runtime module.',
            'Native UHT/UBT compilation and Win64 Alpakit Release packaging have not passed.',
            'Native automatic dispatch/custom assets do not implement Lua feature parity.',
            'Runtime acceptance and dependency compatibility remain pending.',
        ],
        'local_css_engine_present': (ROOT / 'toolchains/UnrealEngineCSS/Engine/Build/BatchFiles/Linux/Build.sh').is_file(),
        'sha256': {name: hashlib.sha256(data).hexdigest() for name, data in sorted(files.items())},
    }
    output = ROOT / 'workspace/releases'
    output.mkdir(exist_ok=True)
    archive = output / 'FILN-source-handoff.zip'
    with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED) as bundle:
        for name, data in sorted(files.items()):
            info = zipfile.ZipInfo(name, date_time=(2026, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            bundle.writestr(info, data)
        bundle.writestr('release-report.json', json.dumps(report, indent=2) + '\n')
    (output / 'release-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'{archive}\nSource handoff only; SMM readiness: BLOCKED (see release-report.json).')


if __name__ == '__main__':
    main()
