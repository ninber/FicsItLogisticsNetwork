#!/usr/bin/env python3
"""Checkpoint, copy-convert, and verify the user-authorized static city station migration."""
import argparse,datetime,hashlib,importlib.util,json,os,pathlib,re,shutil,subprocess,time
ROOT=pathlib.Path(__file__).resolve().parent.parent
spec=importlib.util.spec_from_file_location('filn_runner',ROOT/'scripts/auto-test-filn-lua.py')
runner=importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir',type=pathlib.Path,default=pathlib.Path.home()/'.local/share/Steam/steamapps/common/Satisfactory')
    parser.add_argument('--save-file',type=pathlib.Path,help='Exact current city save to migrate when the game is closed')
    parser.add_argument('--parser-module',type=pathlib.Path,default=ROOT/'scripts/save-tools/node_modules/@etothepii/satisfactory-file-parser')
    args=parser.parse_args();game=args.game_dir.expanduser().resolve();module=args.parser_module.expanduser().resolve()
    if not (module/'package.json').is_file():parser.error('Install the pinned parser with npm ci --prefix scripts/save-tools')
    assert json.loads((module/'package.json').read_text())['version']=='4.1.1','Parser version must match the lockfile'
    scripts=game/'Engine/Binaries/Win64/ue4ss/Mods/FILNLua/Scripts'
    assert not (scripts/'migration-pending.tsv').exists(),'A previous migration still awaits native verification'
    running=bool(runner.game_pids())
    assert running or args.save_file,'Load the city or provide its exact current save file'
    assert not(running and args.save_file),'Omit --save-file when the game is running so current progress can be checkpointed'
    save_root=game.parent.parent/'compatdata/526870/pfx/drive_c/users/steamuser/AppData/Local/FactoryGame/Saved/SaveGames'
    directory=ROOT/'workspace/test-results'/('static-city-apply-'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ'))
    directory.mkdir()
    if args.save_file:
        source=runner.select_save(save_root,requested=args.save_file)
    else:
        reader=runner.LogReader(scripts.parents[2]/'UE4SS.log');reader.offset=reader.path.stat().st_size
        def rpc(command,timeout=30):
            value=runner.rpc(scripts/'commands.txt',reader,'city-migration-'+str(time.time_ns()),command,timeout)
            if value['status']!='PASS':raise RuntimeError(command+': '+value['detail'])
            return value
        rpc('auto_stop')
        deadline=time.monotonic()+900
        while True:
            try:checkpoint=rpc('save_city');break
            except RuntimeError as exc:
                if 'wait for dispatcher trips' not in str(exc) or time.monotonic()>=deadline:raise
                print('Waiting for the current delivery/hold to complete; player controls remain untouched.',flush=True)
                time.sleep(5)
        name=re.search(r'FILN_CITY_SAVE_REQUESTED name=(\w+)','\n'.join(checkpoint['lines'])).group(1)
        source=None
        for _ in range(30):
            matches=list(save_root.glob('*/'+name+'.sav'))
            if matches and matches[0].stat().st_size>100000:source=matches[0];break
            time.sleep(1)
        assert source,'Native checkpoint did not appear'
    shutil.copy2(source,directory/source.name)
    manifest=directory/'manifest.json';filename='City_FILN_Migrated_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S')+'.sav';output=directory/filename
    env=dict(os.environ,FILN_SAVE_PARSER=str(module))
    with (directory/'conversion.log').open('w')as log:
        subprocess.run(['node',str(ROOT/'scripts/save-tools/plan-city.cjs'),str(source),str(manifest)],env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
        subprocess.run(['node',str(ROOT/'scripts/save-tools/replace-stations.cjs'),str(source),str(output),str(manifest),'--write'],env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
    plan=json.loads(manifest.read_text());assert hashlib.sha256(source.read_bytes()).hexdigest()==plan['sha256']
    pending=directory/'migration-pending.tsv'
    subprocess.run(['node',str(ROOT/'scripts/save-tools/verification-plan.cjs'),str(output),str(output)+'.report.json',str(pending)],env=env,check=True)
    target=source.parent/filename;assert not target.exists()
    state={'source_file':str(source),'source_sha256':plan['sha256'],'save_file':str(target),'selected':len(plan['stations']),'skipped':len(plan['skipped']),'verification_pending':True}
    (directory/'state.json').write_text(json.dumps(state,indent=2))
    runner.stop_game(prefix=game.parent.parent/'compatdata/526870/pfx')
    subprocess.run(['bash',str(ROOT/'scripts/update-filn-lua.sh'),str(game)],check=True)
    verified=scripts/'migration-pending.tsv.verified'
    if verified.exists():shutil.move(str(verified),str(directory/'previous-verification.tsv'))
    shutil.copy2(pending,scripts/'migration-pending.tsv');shutil.copy2(output,target)
    log_path=scripts.parents[2]/'UE4SS.log'
    if log_path.exists():shutil.move(str(log_path),str(directory/'ue4ss-before.log'))
    option='-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:GameDefaultMap='+runner.MAP+',[/Script/EngineSettings.GameMapsSettings]:LocalMapOptions=?skipOnboarding?listen?loadgame='+target.stem
    subprocess.run(['steam','-applaunch','526870','-nosplash',option],check=True,timeout=30)
    print('Loaded',target.name,'converted',state['selected'],'left unchanged',state['skipped'],flush=True)
    reader=runner.LogReader(scripts.parents[2]/'UE4SS.log');lines=[];deadline=time.monotonic()+1800
    while time.monotonic()<deadline:
        new=reader.lines();lines.extend(new)
        for line in new:
            if 'MIGRATION_'in line:print(line,flush=True)
            if 'MIGRATION_BLOCKED'in line:
                state['blocked_reason']=line;(directory/'state.json').write_text(json.dumps(state,indent=2));raise RuntimeError('Migration verification blocked; new automatic assignments remain paused. Original checkpoint is preserved.')
        if verified.exists() and verified.read_bytes()==pending.read_bytes() and any('MIGRATION_COMPLETE stations='+str(state['selected'])+' 'in line for line in lines):
            state.update(verification_pending=False,verified=True,sha256=hashlib.sha256(target.read_bytes()).hexdigest());(directory/'state.json').write_text(json.dumps(state,indent=2));(directory/'verification.log').write_text('\n'.join(lines));print('PASS: all migrated station identities, filters, modes, inventory references and road edges verified. Game left running.',flush=True);return
        time.sleep(.5)
    raise RuntimeError('Native verification timed out; automatic assignments remain paused and original checkpoint is preserved')
if __name__=='__main__':main()
