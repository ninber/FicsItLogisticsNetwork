#!/usr/bin/env python3
"""Install FILN's station content extension; requires installed KDataForge and Smarter Truck Stations."""
import argparse,importlib.util,pathlib,shutil,uuid
ROOT=pathlib.Path(__file__).resolve().parent.parent
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--game-dir',type=pathlib.Path,default=pathlib.Path.home()/'.local/share/Steam/steamapps/common/Satisfactory');args=p.parse_args()
game=args.game_dir.expanduser().resolve()
spec=importlib.util.spec_from_file_location('runner',ROOT/'scripts/auto-test-filn-lua.py');runner=importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)
if runner.game_pids():p.error('Close the game before installing station content.')
assert (game/'FactoryGame/Mods/KDataForge/KDataForge.uplugin').is_file(),'KDataForge is required'
assert (game/'FactoryGame/Mods/GameFeatures/TruckStationSettings/TruckStationSettings.uplugin').is_file(),'Smarter Truck Stations is required'
target=game/'FactoryGame/DataForge/FILNStations';target.parent.mkdir(parents=True,exist_ok=True)
if target.exists():
 backup=game/'FactoryGame/Saved/FILNBackups'/('stations-'+uuid.uuid4().hex);backup.parent.mkdir(parents=True,exist_ok=True);shutil.copytree(target,backup)
shutil.copytree(ROOT/'workspace/experiments/FILNLua/FILNStations',target,dirs_exist_ok=True)
print('Installed FILN Logistics Station content:',target)
