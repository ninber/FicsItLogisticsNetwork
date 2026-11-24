#!/usr/bin/env python3
"""Stage pinned official UE4SS binaries; optionally install to an explicit game directory."""
import argparse
import hashlib
import json
import pathlib
import shutil
import subprocess
import sys
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
MOD = ROOT / 'workspace/experiments/FILNLua'


def unpack(archive, destination):
    with zipfile.ZipFile(archive) as z:
        for entry in z.infolist():
            name = pathlib.PurePosixPath(entry.filename)
            if name.is_absolute() or '..' in name.parts or '\\' in entry.filename or (entry.external_attr >> 16) & 0o170000 == 0o120000:
                raise RuntimeError(f'Unsafe ZIP entry: {entry.filename}')
        z.extractall(destination)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir', type=pathlib.Path, help='Install to this Satisfactory directory (game must be closed)')
    parser.add_argument('--offline', action='store_true', help='Use already downloaded, checksum-matching archives only')
    args = parser.parse_args()
    manifest = json.loads((MOD / 'ue4ss-assets.json').read_text())
    archives = ROOT / 'downloads/ue4ss'
    archives.mkdir(parents=True, exist_ok=True)
    for asset in manifest['assets']:
        file = archives / asset['name']
        if not file.exists():
            if args.offline:
                raise RuntimeError(f'Missing archive: {file}')
            partial = file.with_suffix('.partial')
            print(f'Downloading {asset["name"]} ...', flush=True)
            subprocess.run(['curl', '-fL', '--connect-timeout', '15', '--retry', '2', '-o', str(partial), asset['url']], check=True)
            partial.replace(file)
        if file.stat().st_size != asset['size']:
            raise RuntimeError(f'Archive size mismatch: {file}; remove incomplete file and retry')
        expected = asset['digest'].removeprefix('sha256:')
        with file.open('rb') as f:
            actual = hashlib.file_digest(f, 'sha256').hexdigest()
        if actual != expected:
            raise RuntimeError(f'Archive checksum mismatch: {file}')
    stage = ROOT / 'toolchains/FILNLua-UE4SS'
    if stage.exists():
        raise RuntimeError(f'Staging directory already exists; inspect before replacing: {stage}')
    stage.mkdir(parents=True)
    runtime, configs = stage / 'runtime', stage / 'configs'
    binary = next(a for a in manifest['assets'] if a['name'].startswith('UE4SS_'))
    custom = next(a for a in manifest['assets'] if a['name'] == 'zCustomGameConfigs.zip')
    unpack(archives / binary['name'], runtime)
    unpack(archives / custom['name'], configs)
    # Accommodate old flat layout without guessing the game's executable location.
    if (runtime / 'UE4SS.dll').is_file() and not (runtime / 'ue4ss').exists():
        (runtime / 'ue4ss').mkdir()
        for entry in list(runtime.iterdir()):
            if entry.name.lower() not in ('ue4ss', 'dwmapi.dll'):
                shutil.move(str(entry), str(runtime / 'ue4ss' / entry.name))
    if not (runtime / 'dwmapi.dll').is_file() or not (runtime / 'ue4ss/UE4SS.dll').is_file():
        raise RuntimeError(f'Unexpected archive layout; inspect {runtime}')
    matches = [p for p in configs.rglob('Satisfactory') if p.is_dir()]
    if len(matches) != 1:
        raise RuntimeError('Expected one Satisfactory custom configuration directory')
    ue = runtime / 'ue4ss'
    shutil.copytree(matches[0], ue, dirs_exist_ok=True)
    ini = ue / 'UE4SS-settings.ini'
    text = ini.read_text(encoding='utf-8-sig')
    # Custom Satisfactory settings/signatures are supplied by the same verified release.
    if 'MinorVersion = 6' not in text:
        raise RuntimeError('Satisfactory custom configuration does not target UE 5.6; inspect before proceeding')
    text = text.replace('GuiConsoleEnabled = 0', 'GuiConsoleEnabled = 1')
    text = text.replace('RenderMode = ExternalThread', 'RenderMode = EngineTick')
    ini.write_text(text, encoding='utf-8')
    mods = ue / 'Mods'
    mods.mkdir(exist_ok=True)
    shutil.copytree(MOD / 'Scripts', mods / 'FILNLua/Scripts')
    # Enable only this experiment and the bundled engine-console helper, if supplied.
    enabled = 'FILNLua : 1\n'
    if (mods / 'ConsoleEnablerMod').is_dir():
        enabled += 'ConsoleEnablerMod : 1\n'
    (mods / 'mods.txt').write_text(enabled)
    shutil.copy2(MOD / 'ue4ss-assets.json', stage / 'verified-assets.json')
    print(f'Verified runtime staged at {runtime}', flush=True)
    if args.game_dir:
        game = args.game_dir.expanduser().resolve()
        candidates = list(game.glob('Engine/Binaries/Win64/FactoryGame*-Win64-Shipping.exe'))
        if not candidates:
            candidates = list(game.glob('FactoryGame/Binaries/Win64/FactoryGame*-Win64-Shipping.exe'))
        if len(candidates) != 1:
            raise RuntimeError(f'Expected one actual FactoryGame shipping executable, found {len(candidates)}')
        target = candidates[0].parent
        for name in ('dwmapi.dll', 'ue4ss', 'UE4SS.dll', 'xinput1_3.dll'):
            if (target / name).exists():
                raise RuntimeError(f'Existing loader detected: {target / name}; refusing to overwrite')
        shutil.copytree(ue, target / 'ue4ss')
        shutil.copy2(runtime / 'dwmapi.dll', target / 'dwmapi.dll')
        print(f'Installed to {target}')
        print('Steam launch option: WINEDLLOVERRIDES="dwmapi=n,b" %command%')
    else:
        print('Game files unchanged. Copy runtime/dwmapi.dll and runtime/ue4ss beside the actual shipping executable when ready.')


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, OSError, subprocess.CalledProcessError, zipfile.BadZipFile) as exc:
        print(f'FILN Lua setup stopped: {exc}', file=sys.stderr)
        sys.exit(1)
