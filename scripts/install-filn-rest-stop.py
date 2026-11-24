#!/usr/bin/env python3
"""Install the pinned rest-stop dependency, content pack and Lua files persistently."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import urllib.request
import uuid
import zipfile

ROOT=Path(__file__).resolve().parent.parent
SOURCE=ROOT/'workspace/experiments/FILNLua'

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir',type=Path,default=Path.home()/'.local/share/Steam/steamapps/common/Satisfactory')
    args=parser.parse_args();game=args.game_dir.expanduser().resolve()
    mods=game/'FactoryGame/Mods';plugin=mods/'KDataForge'
    assert (mods/'SML/SML.uplugin').is_file(),'SML installation missing'
    metadata=json.loads((SOURCE/'kdataforge-asset.json').read_text())
    archive=ROOT/'downloads'/f'KDataForge-{metadata["version"]}-Windows.zip'
    archive.parent.mkdir(exist_ok=True)
    if not archive.is_file():
        request=urllib.request.Request(metadata['url'],headers={'User-Agent':metadata['user_agent'],'Accept':'application/octet-stream'})
        with urllib.request.urlopen(request,timeout=60) as response,archive.with_suffix('.part').open('wb') as out:
            shutil.copyfileobj(response,out)
        archive.with_suffix('.part').replace(archive)
    with archive.open('rb') as stream: actual=hashlib.file_digest(stream,'sha256').hexdigest()
    assert actual==metadata['sha256'] and archive.stat().st_size==metadata['size'],'dependency archive hash/size mismatch'
    if plugin.exists():
        installed=json.loads((plugin/'KDataForge.uplugin').read_text())
        assert installed['VersionName']==metadata['version'],'different KDataForge version installed; refusing overwrite'
    spec=importlib.util.spec_from_file_location('filn_runner',ROOT/'scripts/auto-test-filn-lua.py')
    runner=importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)
    # Refuse to replace files in a live game. The runner only stops the selected game's processes.
    runner.stop_game(prefix=game.parent.parent/'compatdata/526870/pfx')
    if not plugin.exists():
        with tempfile.TemporaryDirectory(prefix='FILN-KDataForge-',dir=mods) as td:
            staging=Path(td)
            with zipfile.ZipFile(archive) as z:
                assert all(not Path(n).is_absolute() and '..' not in Path(n).parts for n in z.namelist()),'unsafe archive path'
                z.extractall(staging)
            staging.rename(plugin)
    pack=game/'FactoryGame/DataForge/FILNRestStop'
    pack.parent.mkdir(exist_ok=True)
    if pack.exists():
        backup=game/'FactoryGame/Saved/FILNBackups'/('rest-stop-'+uuid.uuid4().hex)
        backup.parent.mkdir(parents=True,exist_ok=True);shutil.copytree(pack,backup)
        print('Pack backup:',backup)
    shutil.copytree(SOURCE/'DataForge',pack,dirs_exist_ok=True)
    subprocess.run(['bash',str(ROOT/'scripts/update-filn-lua.sh'),str(game)],check=True)
    print('Rest-stop dependency and content remain installed:',pack)
    print('Normal host sessions automatically unlock FILN Rest Stop in Vehicles. Placement/road connection still need validation.')
    return 0

if __name__=='__main__':
    raise SystemExit(main())
