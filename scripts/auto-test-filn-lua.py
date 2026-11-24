#!/usr/bin/env python3
"""Unattended FILN snapshot diagnostics and explicitly gated runtime proofs."""
import argparse
import datetime as dt
import fcntl
import hashlib
import json
import os
import pathlib
import re
import shutil
import signal
import subprocess
import sys
import time
import uuid

ROOT = pathlib.Path(__file__).resolve().parent.parent
MAP = '/Game/FactoryGame/Map/GameLevel01/Persistent_Level.Persistent_Level'
EXE = 'factorygamesteam-win64-shipping.exe'
GAME_EXECUTABLES = {EXE, 'factorygamesteam.exe'}


class Blocked(RuntimeError):
    pass


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def newest_save(root, owner=None):
    folders = [p for p in root.iterdir() if p.is_dir() and (p.name == 'common' or (p.name.isdigit() and (not owner or p.name == owner)))]
    saves = [p for folder in folders for p in folder.glob('*.sav') if not p.name.startswith('FILN_AutoTest_') and p.stat().st_size > 0]
    if not saves:
        raise Blocked(f'No gameplay saves found under {root}')
    return max(saves, key=lambda p: (p.stat().st_mtime_ns, str(p)))


def newest_autosave(root):
    candidates=[p for p in root.glob('*/*.sav') if p.parent.name.isdigit() and re.fullmatch(r'.+_autosave_[0-9]+\.sav',p.name,re.IGNORECASE) and p.stat().st_size>0]
    if not candidates: raise Blocked('No real gameplay autosave found')
    return max(candidates,key=lambda p:(p.stat().st_mtime_ns,str(p)))


def select_save(root, owner=None, requested=None):
    if requested is None:
        return newest_save(root, owner)
    requested = pathlib.Path(requested).expanduser()
    source = (requested if requested.is_absolute() else root / requested).resolve()
    if not source.is_relative_to(root.resolve()) or source.parent == root.resolve():
        raise Blocked('Explicit save must be inside a gameplay owner/common directory')
    if source.suffix.lower() != '.sav' or source.name.startswith('FILN_AutoTest_') or not source.is_file() or source.stat().st_size == 0:
        raise Blocked('Explicit gameplay save is missing, empty or an automation snapshot: ' + str(source))
    return source


def autoload_args(save_name):
    if not re.fullmatch(r'FILN_AutoTest_[a-f0-9]+', save_name):
        raise ValueError('Autotest save alias must be a generated safe identifier')
    return ['-nosplash', '-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:GameDefaultMap=' + MAP +
            ',[/Script/EngineSettings.GameMapsSettings]:LocalMapOptions=?skipOnboarding?listen?loadgame=' + save_name]


def game_pids():
    """Select Satisfactory's game and its bootstrap executable, never Steam/Proton."""
    result = []
    for proc in pathlib.Path('/proc').iterdir():
        if not proc.name.isdigit():
            continue
        try:
            comm = (proc / 'comm').read_text().strip().lower()
            args = (proc / 'cmdline').read_bytes().decode(errors='replace').split('\0')
            if comm.startswith('factorygame') and any(a.replace('\\', '/').rsplit('/', 1)[-1].lower() in GAME_EXECUTABLES for a in args):
                result.append(int(proc.name))
        except (OSError, ProcessLookupError):
            pass
    return result


def close_wine_prefix(prefix):
    """After game exit, close only this app's leftover Wine server/helpers."""
    if prefix is None:
        return
    prefix = prefix.resolve()
    for proc in pathlib.Path('/proc').iterdir():
        if not proc.name.isdigit():
            continue
        try:
            if (proc / 'comm').read_text().strip() != 'wineserver':
                continue
            environment = dict(entry.split('=', 1) for entry in (proc / 'environ').read_bytes().decode(errors='replace').split('\0') if '=' in entry)
            if environment.get('SteamAppId') != '526870' or pathlib.Path(environment.get('WINEPREFIX', '/missing')).resolve() != prefix:
                continue
            server = (proc / 'exe').resolve()
        except OSError:
            continue
        env = dict(os.environ, WINEPREFIX=str(prefix))
        subprocess.run([str(server), '-k'], env=env, timeout=15, check=True)
        subprocess.run([str(server), '-w'], env=env, timeout=15, check=True)


def stop_game(timeout=30, prefix=None):
    pids = game_pids()
    for pid in pids:
        try:
            os.kill(pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
    deadline = time.monotonic() + timeout
    while game_pids():
        if time.monotonic() >= deadline:
            raise RuntimeError('Game did not exit after SIGTERM; refusing forced termination or live script/config replacement')
        time.sleep(.25)
    close_wine_prefix(prefix)
    if pids or prefix is not None:
        # Allow Steam to observe app shutdown before asking it to launch the app again.
        time.sleep(2)


def writable(path):
    existing = path
    while not existing.exists():
        existing = existing.parent
    if os.statvfs(existing).f_flag & os.ST_RDONLY:
        raise Blocked(f'Read-only filesystem: {existing}')
    # Check real effective write access, then a reversible probe before stopping the game.
    if not os.access(existing, os.W_OK):
        raise Blocked(f'No write access: {existing}')
    probe = existing / ('.filn-write-probe-' + uuid.uuid4().hex)
    try:
        probe.write_bytes(b'')
        probe.unlink()
    except OSError as exc:
        raise Blocked(f'Write preflight failed for {existing}: {exc}') from exc


class LogReader:
    def __init__(self, path):
        self.path = path
        self.offset = 0
        self.pending = ''
        self.identity = None
        self.history = []

    def lines(self):
        if not self.path.exists():
            return []
        stat = self.path.stat()
        identity = (stat.st_dev, stat.st_ino)
        if identity != self.identity or stat.st_size < self.offset:
            self.offset, self.pending = 0, ''
            self.identity = identity
        with self.path.open('rb') as stream:
            stream.seek(self.offset)
            data = stream.read(1024 * 1024)
            self.offset = stream.tell()
        self.pending += data.decode('utf-8', errors='replace')
        lines = self.pending.split('\n')
        self.pending = lines.pop()
        self.history.extend(lines)
        return lines


def await_session(reader, token, timeout):
    deadline = time.monotonic() + timeout
    heartbeat = 0
    while time.monotonic() < deadline:
        for line in reader.lines():
            if f'AUTOTEST_SESSION {token}' in line:
                return
        if time.monotonic() > heartbeat:
            print('Waiting for this run\'s UE4SS/FILNLua startup...', flush=True)
            heartbeat = time.monotonic() + 10
        time.sleep(.25)
    raise RuntimeError('Timed out waiting for unique FILNLua startup token; inspect launch/UE4SS logs')


def rpc(inbox, reader, request_id, command, timeout=15):
    with inbox.open('a', encoding='utf-8') as stream:
        stream.write(f'@{request_id} {command}\n')
    marker = re.compile(r'AUTOTEST_RESULT ' + re.escape(request_id) + r' (PASS|FAIL) (\w+) ?(.*)')
    lines = []
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        for line in reader.lines():
            if '[FILN-Lua]' in line:
                lines.append(line)
            match = marker.search(line)
            if match:
                return {'command': command, 'status': match[1], 'detail': match[3].strip(), 'lines': lines}
        time.sleep(.1)
    raise RuntimeError(f'Timeout for {command}; queue acknowledgement is not a passing result')


def run_depot_journey(args, report, scripts, reader, run_token, fixture):
    home = fixture['vehicle']['route'][0]
    stops = [s['guid'] for s in fixture['rest_stops'] if s['guid'] != home]
    if len(stops) != 2:
        raise RuntimeError('Depot journey requires exactly two test stops')
    arguments = ' '.join([home] + stops)
    cycles = getattr(args, 'depot_cycles', 2)
    for cycle in range(1, cycles + 1):
        prefix = run_token + '-cycle-' + str(cycle)
        for command in ('autotest_depot_probe', 'autotest_depot_prepare', 'autotest_depot_begin'):
            result = rpc(scripts / 'commands.txt', reader, prefix + '-' + command, command + ' ' + arguments, args.command_timeout)
            report['commands'].append(result)
            if result['status'] != 'PASS':
                raise RuntimeError('Depot setup failed: ' + result['detail'])
        deadline = time.monotonic() + args.journey_timeout
        cycle_lines = []
        attempt = 0
        while time.monotonic() < deadline:
            attempt += 1
            result = rpc(scripts / 'commands.txt', reader, prefix + '-status-' + str(attempt), 'autotest_depot_status ' + home, args.command_timeout)
            report['commands'].append(result)
            cycle_lines.extend(result['lines'])
            if result['status'] != 'PASS':
                raise RuntimeError('Depot journey failed: ' + result['detail'])
            state = re.search(r'DEPOT_STATE state=(\w+)', '\n'.join(result['lines']))
            if state is None:
                raise RuntimeError('Missing depot lifecycle status')
            print('Depot cycle ' + str(cycle) + ': ' + state[1], flush=True)
            if state[1] == 'Idle':
                if not any('DEPOT_CYCLE_PASS ' in line for line in cycle_lines):
                    raise RuntimeError('Idle status without actual cycle/hold evidence')
                break
            time.sleep(2)
        else:
            result = rpc(scripts / 'commands.txt', reader, prefix + '-cancel', 'autotest_depot_cancel ' + home, args.command_timeout)
            report['commands'].append(result)
            raise RuntimeError('Depot journey timed out; cancellation requested')
        report['depot_journey'] = {'status': 'PASS' if cycle == cycles else 'INCOMPLETE', 'cycles': cycle,
            'home': home, 'scope': 'two stops, home return, restored one-stop route and 12-second hold per cycle'}

    for command, evidence in (('autotest_depot_guard', 'DEPOT_GUARD_PASS'),
                              ('autotest_depot_break_road', 'DEPOT_ROAD_FAULT'),
                              ('autotest_depot_blocked', 'DEPOT_BLOCKED_PASS')):
        result = rpc(scripts / 'commands.txt', reader, run_token + '-' + command, command + ' ' + arguments, args.command_timeout)
        report['commands'].append(result)
        if result['status'] != 'PASS' or not any(evidence in line for line in result['lines']):
            raise RuntimeError('Depot guard/fault proof failed: ' + result['detail'])
        if command == 'autotest_depot_break_road':
            time.sleep(2)  # Native graph topology rebuild after EndPlay.
    report['depot_journey']['failed_assignment_held'] = True
    report['depot_journey']['disconnected_road_held'] = True


def automatic_assignment(lines, home):
    """Use the started job, not the earlier read-only selection preview."""
    selection = re.compile(r'CARGO_SELECTED home=(\w+) provider=(\w+) requester=(\w+) item=(\S+) supply=(\d+) requested_stock=(\d+) capacity=(\d+) road_cm=(\d+)')
    started = re.compile(r'AUTO_DISPATCH_STARTED home=(\w+) provider=(\w+) requester=(\w+) truckload=(\d+)')
    candidates = {}
    actual = None
    for line in lines:
        match = selection.search(line)
        if match and match[1] == home:
            candidates[(match[2], match[3])] = {'provider': match[2], 'requester': match[3], 'item': match[4],
                'supply': int(match[5]), 'requester_stock': int(match[6]), 'capacity': int(match[7]), 'road_cm': int(match[8])}
        match = started.search(line)
        if match and match[1] == home:
            actual = candidates.get((match[2], match[3]))
            if actual is None:
                raise RuntimeError('Started automatic job lacks matching cargo selection evidence')
            actual = dict(actual, truckload=int(match[4]), automatic_assignment=True)
    if actual is None:
        raise RuntimeError('Missing actual automatic dispatch receipt')
    return actual


def run_cargo_journey(args, report, scripts, reader, run_token, fixture):
    home = fixture['vehicle']['route'][0]
    selection = rpc(scripts / 'commands.txt', reader, run_token + '-cargo-select', 'autotest_cargo_select ' + home, max(args.command_timeout, 60))
    report['commands'].append(selection)
    match = re.search(r'CARGO_SELECTED home=(\w+) provider=(\w+) requester=(\w+) item=(\S+) supply=(\d+) requested_stock=(\d+) capacity=(\d+) road_cm=(\d+)', '\n'.join(selection['lines']))
    if selection['status'] != 'PASS' or match is None:
        raise RuntimeError('Real cargo job selection failed: ' + selection['detail'])
    job = {'provider': match[2], 'requester': match[3], 'item': match[4], 'supply': int(match[5]),
           'requester_stock': int(match[6]), 'capacity': int(match[7]), 'road_cm': int(match[8])}
    report['cargo_job'] = job
    arguments = ' '.join([home, job['provider'], job['requester'], job['item']])
    for command in (('autotest_depot_prepare',) if getattr(args, 'automatic_delivery', False) else ('autotest_depot_prepare', 'autotest_depot_begin')):
        result = rpc(scripts / 'commands.txt', reader, run_token + '-cargo-' + command, command + ' ' + arguments, args.command_timeout)
        report['commands'].append(result)
        if result['status'] != 'PASS':
            raise RuntimeError('Cargo dispatch failed: ' + result['detail'])
    if getattr(args, 'automatic_delivery', False):
        result=rpc(scripts / 'commands.txt', reader, run_token + '-auto-start', 'auto_start', args.command_timeout)
        report['commands'].append(result)
        if result['status']!='PASS': raise RuntimeError('Automatic dispatch start failed: '+result['detail'])
        deadline=time.monotonic()+90
        while time.monotonic()<deadline:
            result=rpc(scripts / 'commands.txt', reader, run_token+'-auto-wait-'+str(time.time_ns()),'auto_status',args.command_timeout)
            report['commands'].append(result)
            if any('AUTO_JOB home='+home+' state=Trip' in line for line in result['lines']):break
            time.sleep(2)
        else: raise RuntimeError('Periodic scheduler did not dispatch the depot truck')
        # Prove one automatically scheduled trip; prevent a second test assignment.
        result=rpc(scripts / 'commands.txt',reader,run_token+'-auto-stop','auto_stop',args.command_timeout)
        report['commands'].append(result)
        if result['status']!='PASS': raise RuntimeError('Automatic dispatch pause failed')
        report['selection_preview'] = dict(job)
        job.clear()
        job.update(automatic_assignment(reader.history, home))
    if getattr(args, 'player_optout', False):
        for command in ('autotest_driver_optout','autotest_driver_optout_check'):
            result=rpc(scripts / 'commands.txt',reader,run_token+'-'+command,command+' '+home,args.command_timeout)
            report['commands'].append(result)
            if result['status']!='PASS':raise RuntimeError('Native autopilot optout failed: '+result['detail'])
            time.sleep(2)
        job.update(status='OPTOUT_PASS',scope='native autopilot disable and detached ownership; no forced player possession or completed delivery claimed')
        return
    deadline = time.monotonic() + args.journey_timeout
    lines = [];attempt = 0
    while time.monotonic() < deadline:
        attempt += 1
        result = rpc(scripts / 'commands.txt', reader, run_token + '-cargo-status-' + str(attempt), 'autotest_depot_status ' + home, args.command_timeout)
        report['commands'].append(result);lines.extend(result['lines'])
        if result['status'] != 'PASS':
            raise RuntimeError('Cargo journey failed: ' + result['detail'])
        state = re.search(r'DEPOT_STATE state=(\w+)', '\n'.join(result['lines']))
        if state is None:
            raise RuntimeError('Missing cargo lifecycle status')
        print('Cargo delivery: ' + state[1], flush=True)
        if state[1] in ('Recovery', 'Suspended'):
            raise RuntimeError('Cargo journey stopped: ' + '\n'.join(result['lines']))
        if state[1] == 'Idle':
            proof = re.search(r'DEPOT_CYCLE_PASS [^\n]* loaded=(\d+) unloaded=(\d+)', '\n'.join(lines))
            if not proof or int(proof[1])<=0 or int(proof[1])!=int(proof[2]):
                raise RuntimeError('Cargo completion lacks actual load/unload evidence')
            job.update(status='PASS', loaded=int(proof[1]), unloaded=int(proof[2]), returned=True, held_seconds=12)
            break
        time.sleep(2)
    else:
        rpc(scripts / 'commands.txt', reader, run_token + '-cargo-cancel', 'autotest_depot_cancel ' + home, args.command_timeout)
        raise RuntimeError('Cargo delivery timed out; cancellation requested')


def fleet_choices(lines):
    candidates = {}
    choices = []
    for line in lines:
        match = re.search(r'FLEET_CANDIDATE round=(\d+) home=(\w+) total_cm=([\d.]+)', line)
        if match:
            candidates.setdefault(match[1], {})[match[2]] = float(match[3])
        match = re.search(r'FLEET_CHOICE round=(\d+) home=(\w+) total_cm=([\d.]+) compared=(\d+)', line)
        if match:
            scores = candidates.get(match[1], {})
            if len(scores) != int(match[4]) or match[2] not in scores or abs(float(match[3])-scores[match[2]])>.002 or float(match[3]) > min(scores.values()) + .002:
                raise RuntimeError('Fleet choice is not the minimum logged complete-trip distance')
            choices.append({'round': int(match[1]), 'home': match[2], 'total_cm': float(match[3]), 'compared': len(scores)})
    return choices


def soak_cycles(lines, homes):
    """Count unique completed native cycles, never status samples or partial transfers."""
    cycles={}
    for line in lines:
        match=re.search(r'DEPOT_CYCLE_PASS home=(\w+) cycle=(\d+).*loaded=(\d+) unloaded=(\d+)',line)
        if match and match[1] in homes:
            if int(match[3])<=0 or match[3]!=match[4]:
                raise RuntimeError('Soak cargo mismatch')
            cycles[match[1]+':'+match[2]]={'home':match[1],'cycle':int(match[2]),'items':int(match[3])}
    return cycles


def run_fleet_journey(args, report, scripts, reader, run_token):
    size = getattr(args, 'expected_fleet_size', 4)
    result = rpc(scripts / 'commands.txt', reader, run_token+'-fleet-audit', 'autotest_fleet_audit '+str(size)+(' soak' if getattr(args,'soak_seconds',0) else ''), args.command_timeout)
    report['commands'].append(result)
    if result['status'] != 'PASS' or not any('FLEET_AUDIT_PASS enabled='+str(size) in line for line in result['lines']):
        raise RuntimeError('Fleet setup audit failed: '+result['detail'])
    homes = re.findall(r'FLEET_VEHICLE home=(\w+) id=V\d+', '\n'.join(result['lines']))
    if len(set(homes)) != size:
        raise RuntimeError('Fleet audit lacks distinct vehicle/home evidence')
    trace=rpc(scripts / 'commands.txt',reader,run_token+'-fuel-trace','autotest_fleet_fuel_trace',args.command_timeout)
    report['commands'].append(trace)
    report['fuel_trace']={'status':trace['status'],'scope':'read-only dedicated fuel inventories and native smart-splitter rules'}
    if getattr(args, 'verify_refueling', False):
        result=rpc(scripts/'commands.txt',reader,run_token+'-refuel-fixture','autotest_fleet_refuel_fixture',args.command_timeout)
        report['commands'].append(result)
        if result['status']!='PASS': raise RuntimeError('Refuel fixture setup failed: '+result['detail'])
        report['refuel_fixture']={'scope':'snapshot only: existing manually supplied coal moved from truck fuel slots to own depot fuel slots; one coal remains per truck; no creation or cargo writes'}
    if getattr(args, 'fix_fuel_branches', False):
        fixture=json.loads((getattr(args,'fixture_file',None) or ROOT/'workspace/experiments/FILNLua/test-fixture.json').read_text())
        plan=fixture['fleet']['fuel_splitters']
        if len(plan)!=4 or {p['home'] for p in plan}!=set(homes): raise RuntimeError('Fuel plan differs from native active homes')
        words=[]
        for p in plan:
            if not re.fullmatch(r'[a-f0-9]{32}',p['home']) or not re.fullmatch(r'Build_ConveyorAttachmentSplitterSmart_C_[0-9]+',p['actor_suffix']): raise RuntimeError('Invalid explicit fuel plan')
            words.extend([p['home'],p['actor_suffix']])
        result=rpc(scripts/'commands.txt',reader,run_token+'-fix-fuel','autotest_fleet_fix_fuel_rules '+' '.join(words),args.command_timeout)
        report['commands'].append(result)
        if result['status']!='PASS' or not any('FUEL_BRANCH_FIX_PASS' in line for line in result['lines']): raise RuntimeError('Fuel branch setup failed: '+result['detail'])
        report['fuel_branch_fix']={'scope':'snapshot-only four disabled fuel branch rules enabled; bypass rules unchanged; no inventory writes'}
        deadline=time.monotonic()+180
        while True:
            result=rpc(scripts/'commands.txt',reader,run_token+'-depot-fuel-'+str(time.time_ns()),'autotest_fleet_depot_fuel_ready'+(' 100' if getattr(args,'verify_refueling',False) else ''),args.command_timeout)
            report['commands'].append(result)
            if result['status']=='PASS' and any('DEPOT_FUEL_READY count=4 cargo_empty=true' in line for line in result['lines']): break
            if time.monotonic()>=deadline: raise RuntimeError('Fuel belts did not fill all four refueling inventories')
            time.sleep(2)
        report['fuel_branch_fix']['native_fuel_flow']='PASS: all four refueling stores received items; all cargo stores stayed empty'
    if getattr(args, 'starter_fuel', False):
        result = rpc(scripts / 'commands.txt', reader, run_token+'-fleet-fuel-seed', 'autotest_fleet_seed_fuel', args.command_timeout)
        report['commands'].append(result)
        if result['status']!='PASS': raise RuntimeError('Fleet fixture fuel preparation failed: '+result['detail'])
        report['starter_fuel']={'scope':'disposable snapshot only; up to 100 coal added to each empty active truck fuel inventory','original_save_unchanged':True}
    deadline=time.monotonic()+10
    while True:
        result=rpc(scripts / 'commands.txt',reader,run_token+'-fleet-fuel-ready-'+str(time.time_ns()),'autotest_fleet_fuel_ready',args.command_timeout)
        report['commands'].append(result)
        if result['status']=='PASS' and any('FLEET_FUEL_READY all=true' in line for line in result['lines']): break
        if time.monotonic()>=deadline: raise RuntimeError('Fleet has vehicles without starter fuel: '+result['detail'])
        time.sleep(.5)
    report['fleet'] = {'homes': homes, 'scope': 'powered fleet; ordinary power-plant vehicle remains active; disabled homes preserved'}
    if getattr(args,'refuel_only',False):
        deadline=time.monotonic()+60
        while True:
            result=rpc(scripts/'commands.txt',reader,run_token+'-stationary-ready-'+str(time.time_ns()),'autotest_fleet_stationary_refuel_ready',args.command_timeout)
            report['commands'].append(result)
            if result['status']=='PASS' and any('STATIONARY_REFUEL_PASS homes='+str(size)+' dispatch_paused=true routes_unchanged=true ordinary_preserved=true' in line for line in result['lines']): break
            if time.monotonic()>=deadline: raise RuntimeError('Stationary refueling while dispatch paused did not complete')
            time.sleep(1)
        report['stationary_refueling']={'status':'PASS','vehicles':size,'dispatch_paused':True,'routes_unchanged':True,'ordinary_preserved':True}
        report['fleet']['status']='NOT_RUN'
        report['scope']='stationary native refueling only; no cargo dispatch in this run; original save preserved'
        return

    result = rpc(scripts / 'commands.txt', reader, run_token+'-fleet-start', 'auto_start', args.command_timeout)
    report['commands'].append(result)
    if result['status'] != 'PASS': raise RuntimeError('Fleet start failed: '+result['detail'])
    if getattr(args,'soak_seconds',0):
        started=time.monotonic(); stop_at=started+args.soak_seconds; drained=False
        report['soak']={'scheduling_seconds':args.soak_seconds,'status':'RUNNING'}
        while time.monotonic()<stop_at+args.journey_timeout:
            if not drained and time.monotonic()>=stop_at:
                result=rpc(scripts/'commands.txt',reader,run_token+'-soak-stop','auto_stop',args.command_timeout)
                report['commands'].append(result)
                if result['status']!='PASS': raise RuntimeError('Soak pause failed')
                drained=True
            result=rpc(scripts/'commands.txt',reader,run_token+'-soak-'+str(time.time_ns()),'autotest_fleet_status',args.command_timeout)
            report['commands'].append(result)
            if result['status']!='PASS': raise RuntimeError('Soak native invariant failed: '+result['detail'])
            states=re.findall(r'FLEET_JOB home=\w+ state=(\w+)', '\n'.join(result['lines']))
            cycles=soak_cycles(reader.history, homes)
            report['soak'].update(elapsed_seconds=round(time.monotonic()-started,1),cycles=list(cycles.values()),items=sum(c['items'] for c in cycles.values()))
            report['fleet']['choices']=fleet_choices(reader.history)
            print('Soak '+str(round(time.monotonic()-started))+'s cycles='+str(len(cycles))+' states='+','.join(states),flush=True)
            if drained and len(states)==size and all(state in ('Idle','Waiting') for state in states):
                counts=[sum(c['home']==h for c in cycles.values()) for h in homes]
                if not counts or max(counts)<2: raise RuntimeError('Soak lacked repeated vehicle-cycle coverage')
                report['soak']['status']='PASS';report['fleet'].update(status='PASS',ordinary_preserved=True,disabled_excluded=True,reservations_unique=True)
                return
            time.sleep(5)
        raise RuntimeError('Soak deliveries did not drain before timeout')
    deadline = time.monotonic()+args.journey_timeout
    prior = None
    while time.monotonic()<deadline:
        result = rpc(scripts / 'commands.txt', reader, run_token+'-fleet-status-'+str(time.time_ns()), 'autotest_fleet_status', args.command_timeout)
        report['commands'].append(result)
        if result['status'] != 'PASS': raise RuntimeError('Fleet native validation failed: '+result['detail'])
        jobs = re.findall(r'FLEET_JOB home=(\w+) state=(\w+) loaded=(\d+) unloaded=(\d+)', '\n'.join(result['lines']))
        states = {home: state for home, state, _, _ in jobs}
        if states != prior:
            print('Fleet: '+', '.join(h[-8:]+':'+states.get(h,'Unknown') for h in homes), flush=True)
            prior = states
        report['fleet']['choices'] = fleet_choices(reader.history)
        if any('FLEET_STATUS_PASS completed='+str(size)+' ordinary_preserved=true disabled_excluded=true reservations_unique=true' in line for line in result['lines']):
            proofs = {}
            for line in reader.history:
                match = re.search(r'DEPOT_CYCLE_PASS home=(\w+) .*loaded=(\d+) unloaded=(\d+)', line)
                if match and match[1] in homes and int(match[2])>0 and match[2]==match[3]:
                    proofs[match[1]] = {'loaded':int(match[2]),'unloaded':int(match[3]),'held_seconds':12}
            if len(proofs)!=size or not report['fleet']['choices']:
                raise RuntimeError('Fleet completion lacks per-vehicle cargo/hold or ranking proof')
            if getattr(args,'verify_refueling',False):
                fuel_proofs={}
                for line in reader.history:
                    match=re.search(r'DEPOT_NATIVE_FUEL_RECEIVED home=(\w+) before=(\d+) after=(\d+)',line)
                    if match and match[1] in homes and int(match[3])>int(match[2]): fuel_proofs[match[1]]={'before':int(match[2]),'after':int(match[3])}
                if len(fuel_proofs)!=size: raise RuntimeError('Missing native refueling transfer evidence for fleet')
                report['native_refueling']={'status':'PASS','homes':fuel_proofs}
            report['fleet'].update(status='PASS', cycles=proofs, ordinary_preserved=True, disabled_excluded=True, reservations_unique=True)
            result=rpc(scripts / 'commands.txt', reader, run_token+'-fleet-stop', 'auto_stop', args.command_timeout)
            report['commands'].append(result)
            if result['status']!='PASS': raise RuntimeError('Fleet test pause failed')
            if getattr(args,'checkpoint_fleet',False):
                result=rpc(scripts/'commands.txt',reader,run_token+'-fleet-checkpoint','save_city',args.command_timeout)
                report['commands'].append(result)
                match=re.search(r'FILN_CITY_SAVE_REQUESTED name=(\w+)', '\n'.join(result['lines']))
                if result['status']!='PASS' or not match: raise RuntimeError('Fleet checkpoint request failed')
                root=args.save_root.expanduser().resolve();deadline=time.monotonic()+60;previous=None
                while time.monotonic()<deadline:
                    files=list(root.glob('*/'+match[1]+'.sav'))
                    if len(files)==1 and files[0].stat().st_size>0:
                        signature=(files[0].stat().st_size,files[0].stat().st_mtime_ns)
                        if previous==signature:
                            report['fleet_checkpoint']={'save_file':str(files[0]),'sha256':digest(files[0]),'scope':'new configured city copy; original source preserved'};break
                        previous=signature
                    time.sleep(.5)
                else: raise RuntimeError('Fleet checkpoint file not completed')
            return
        time.sleep(2)
    raise RuntimeError('Fleet acceptance timed out; inspect pending homes and active demand')


def run(args, report, output):
    if getattr(args, 'refuel_only', False) and (not getattr(args, 'verify_refueling', False) or getattr(args,'checkpoint_fleet',False)):
        raise Blocked('Refuel-only mode requires native refueling verification and cannot checkpoint cargo proof')
    if getattr(args, 'checkpoint_fleet', False) and not getattr(args, 'fleet_delivery', False):
        raise Blocked('Fleet checkpoint requires completed fleet verification')
    if getattr(args, 'verify_refueling', False) and not getattr(args, 'fix_fuel_branches', False):
        raise Blocked('Refueling verification requires the fuel-branch fleet fixture')
    if getattr(args, 'fix_fuel_branches', False) and not getattr(args, 'fleet_delivery', False):
        raise Blocked('Fuel-branch fixture requires fleet-delivery')
    if getattr(args, 'starter_fuel', False) and not getattr(args, 'fleet_delivery', False):
        raise Blocked('Starter fuel is only allowed for the disposable fleet test')
    if getattr(args, 'fleet_delivery', False) and (not getattr(args, 'automatic_delivery', False) or getattr(args, 'isolate_traffic', False)):
        raise Blocked('Fleet mode requires automatic-delivery and preserves ordinary traffic; do not use isolate-traffic')
    if getattr(args, 'isolate_traffic', False) and not getattr(args, 'automatic_delivery', False):
        raise Blocked('Traffic isolation requires automatic-delivery on a disposable snapshot')
    game = args.game_dir.expanduser().resolve()
    scripts = game / 'Engine/Binaries/Win64/ue4ss/Mods/FILNLua/Scripts'
    if not scripts.is_dir():
        scripts = game / 'FactoryGame/Binaries/Win64/ue4ss/Mods/FILNLua/Scripts'
    if not (scripts / 'main.lua').is_file():
        raise Blocked(f'Installed FILNLua missing: {scripts}')
    ue_log = scripts.parents[2] / 'UE4SS.log'
    saved = args.save_root.expanduser().resolve()
    prefix = next((parent for parent in saved.parents if parent.name == 'pfx'), None)
    manifest = game.parent.parent / 'appmanifest_526870.acf'
    owner = None
    if manifest.is_file():
        match = re.search(r'"LastOwner"\s+"(\d+)"', manifest.read_text())
        owner = match[1] if match else None
    latest = select_save(saved, owner, getattr(args, 'save_file', None))
    if getattr(args,'fleet_delivery',False):
        fixture=json.loads((getattr(args,'fixture_file',None)or ROOT/'workspace/experiments/FILNLua/test-fixture.json').read_text())
        if digest(latest)!=fixture['sha256']:
            raise Blocked('Fleet test requires the exact pinned fixture before game shutdown or edits')
    expected = getattr(args, 'expected_save_sha256', None)
    if expected and digest(latest) != expected:
        raise Blocked('Pinned fixture save changed; refusing silent substitution or an unverified fixture')
    report.update(source_save=str(latest), source_sha256=digest(latest), source_mtime_ns=latest.stat().st_mtime_ns)
    report['selected_game'] = str(game)
    report['phase'] = 'preflight'
    # These checks precede ALL process signals or installed-file modifications.
    for path in (scripts, scripts.parent, ue_log.parent, saved / 'common'):
        writable(path)
    for name in ('main.lua', 'Core.lua', 'Config.lua'):
        if not os.access(scripts / name, os.W_OK):
            raise Blocked(f'Installed file is not writable: {scripts / name}')
    steam = shutil.which('steam')
    if not steam:
        raise Blocked('Steam launcher not on PATH')
    if not os.environ.get('DISPLAY') and not os.environ.get('WAYLAND_DISPLAY'):
        raise Blocked('No graphical desktop session available to launch Steam')
    print(f'Latest gameplay save: {latest}', flush=True)
    report['phase'] = 'host-tests'
    with (output / 'host-tests.log').open('w') as stream:
        subprocess.run(['bash', str(ROOT / 'scripts/test-filn-lua.sh')], stdout=stream, stderr=subprocess.STDOUT, check=True)
    report['host_tests'] = 'PASS'
    report['phase'] = 'save-backup'
    # Keep a complete backup of the current save's directory plus common saves.
    for folder in {latest.parent, saved / 'common'}:
        if folder.is_dir():
            dest = output / 'save-backup' / folder.name
            dest.mkdir(parents=True)
            for file in folder.glob('*.sav'):
                shutil.copy2(file, dest / file.name)
    print('Stopping any existing Satisfactory shipping process...', flush=True)
    report['phase'] = 'stop-game'
    stop_game(prefix=prefix)
    config = scripts / 'Config.lua'
    config_before = config.read_bytes()
    token_file = scripts / 'AutoTestRun.lua'
    token_before = token_file.read_bytes() if token_file.exists() else None
    run_token = uuid.uuid4().hex
    alias = 'FILN_AutoTest_' + run_token
    test_save = saved / 'common' / (alias + '.sav')
    game_log = saved.parent / 'Logs/FactoryGame.log'
    # Preserve old logs and remove them only after the game has exited, so stale runs
    # cannot satisfy the latest-save identity checks.
    for original, name in ((ue_log, 'ue4ss-before.log'), (game_log, 'game-before.log')):
        if original.exists():
            shutil.copy2(original, output / name)
            original.unlink()
    installed = False
    reader = None
    try:
        report['phase'] = 'update-mod'
        subprocess.run(['bash', str(ROOT / 'scripts/update-filn-lua.sh'), str(game)], check=True)
        for name in ('main.lua', 'Core.lua', 'Bridge.lua', 'RestStop.lua', 'RestStopBuilding.lua', 'DepotRuntime.lua', 'CargoAudit.lua', 'RoadGraph.lua', 'CargoDispatch.lua', 'StockPolicy.lua', 'StationRetrofit.lua', 'ProgrammableStation.lua', 'ProgrammableUnlock.lua', 'HubResearch.lua', 'AutoDispatch.lua', 'DispatchJournal.lua', 'RoadCache.lua', 'MigrationVerify.lua', 'TrafficIsolation.lua', 'TestFuel.lua', 'FuelSupplyRules.lua', 'RefuelFixture.lua', 'BlueprintLibrary.lua', 'RequestStatus.lua', 'RecoveryFSM.lua', 'DepotFuelService.lua', 'Json.lua', 'Telemetry.lua', 'Dashboard.lua', 'OrphanRoutes.lua', 'OrphanTest.lua'):
            canonical = ROOT / 'workspace/experiments/FILNLua/Scripts' / name
            if digest(scripts / name) != digest(canonical):
                raise RuntimeError(f'Installed source mismatch: {name}')
        # Automated discovery is read-only even if the operator enabled manual route writes.
        test_config=(ROOT / 'workspace/experiments/FILNLua/Scripts/Config.lua').read_text()
        if getattr(args, 'route_bridge', False):
            test_config=test_config.replace('AutoTestAllowRouteBridge = false', 'AutoTestAllowRouteBridge = true')
        if getattr(args, 'depot_journey', False) or getattr(args, 'cargo_journey', False) or getattr(args, 'station_retrofit', False) or getattr(args, 'automatic_delivery', False):
            test_config=test_config.replace('AutoTestAllowDepotJourney = false', 'AutoTestAllowDepotJourney = true')
        if getattr(args,'blueprint_previews',False):
            test_config=test_config.replace('return {','return {\n    AutoTestAllowBlueprintPreview = true,')
        if getattr(args, 'automatic_delivery', False):
            test_config=test_config.replace('AllowAutoDispatch = false', 'AllowAutoDispatch = true').replace('return {', 'return {\n    AutoTestAllowAutoDispatch = true,')
        if getattr(args, 'verify_refueling', False):
            test_config=test_config.replace('return {', 'return {\n    AutoTestAllowRefuelFixture = true,')
        if getattr(args, 'fix_fuel_branches', False):
            test_config=test_config.replace('return {', 'return {\n    AutoTestAllowFuelRules = true,')
        if getattr(args, 'starter_fuel', False):
            test_config=test_config.replace('return {', 'return {\n    AutoTestAllowStarterFuel = true,')
        config.write_text(test_config)
        token_file.write_text('return "' + run_token + '"\n')
        test_save.parent.mkdir(exist_ok=True)
        shutil.copy2(latest, test_save)
        if digest(test_save) != report['source_sha256']:
            raise RuntimeError('Latest-save snapshot changed during preparation')
        if getattr(args,'blueprint_previews',False):
            organized=output/'organized-snapshot.sav'
            subprocess.run(['node',str(ROOT/'scripts/save-tools/organize-blueprint-categories.cjs'),str(test_save),str(organized),str(ROOT/'workspace/blueprints/FILN/manifest.json')],check=True,timeout=180)
            shutil.copy2(organized,test_save)
            report['blueprint_category_fixture']={'scope':'category records changed only in autosave-derived disposable snapshot; original gameplay autosave unchanged'}
        installed = True
        launch = [steam, '-applaunch', '526870', *autoload_args(alias)]
        report.update(test_save_alias=alias, launch=launch, phase='launch')
        print('Launching Steam and auto-loading the latest-save snapshot...', flush=True)
        env = dict(os.environ)
        if 'dwmapi=' not in env.get('WINEDLLOVERRIDES', ''):
            env['WINEDLLOVERRIDES'] = 'dwmapi=n,b;' + env.get('WINEDLLOVERRIDES', '')
        with (output / 'steam-launch.log').open('w') as stream:
            subprocess.run(launch, env=env, stdout=stream, stderr=subprocess.STDOUT, timeout=30, check=True)
        report['phase'] = 'loader-readiness'
        reader = LogReader(ue_log)
        await_session(reader, run_token, args.start_timeout)
        # UE4SS loads before the game's initial map/save finishes loading. The first
        # queued game-thread callback may legitimately wait for that entire startup.
        result = rpc(scripts / 'commands.txt', reader, run_token + '-ping', 'ping', args.start_timeout)
        report['commands'].append(result)
        if result['status'] != 'PASS':
            raise RuntimeError('Lua ping failed: ' + result['detail'])
        report['phase'] = 'save-readiness'
        deadline = time.monotonic() + args.start_timeout
        attempt = 0
        # Startup map options can fall back to a new world. Require the actual load-map
        # log for our snapshot, then a real authoritative registry and expected vehicles.
        load_pattern = re.compile(r'LogLoad: LoadMap: [^\r\n]*\?loadgame=' + alias + r'(?=[?\s]|$)')
        scan = None
        while time.monotonic() < deadline:
            text = game_log.read_text(errors='replace') if game_log.exists() else ''
            if not load_pattern.search(text):
                time.sleep(1)
                continue
            attempt += 1
            scan = rpc(scripts / 'commands.txt', reader, run_token + '-scan-' + str(attempt), 'scan', args.command_timeout)
            report['commands'].append(scan)
            if scan['status'] == 'PASS':
                break
            if 'expected one authoritative vehicle subsystem' not in scan['detail'] or 'found 0' not in scan['detail']:
                raise RuntimeError('Scan failed: ' + scan['detail'])
            time.sleep(1)
        if not scan or scan['status'] != 'PASS':
            raise RuntimeError('Latest-save world did not become ready before timeout')
        vehicles = sorted(set(re.findall(r'\[FILN-Lua\] (V\d+) ', '\n'.join(scan['lines']))), key=lambda v: int(v[1:]))
        report['vehicle_count'] = len(vehicles)
        report['station_count'] = len(set(re.findall(r'\[FILN-Lua\] (S\d+) ', '\n'.join(scan['lines']))))
        if len(vehicles) < args.min_vehicles:
            raise RuntimeError(f'Only {len(vehicles)} vehicles; expected at least {args.min_vehicles}. Wrong/empty fallback save cannot pass.')
        report['phase'] = 'snapshot-preparation'
        prepared = rpc(scripts / 'commands.txt', reader, run_token + '-prepare', 'autotest_prepare', args.command_timeout)
        report['commands'].append(prepared)
        if prepared['status'] != 'PASS' or not any('AUTOTEST_PREPARED autosave=false' in line for line in prepared['lines']):
            raise RuntimeError('Snapshot autosave preparation failed: ' + prepared['detail'])
        report['autosave_disabled'] = True
        if getattr(args, 'filn_station', False):
            result=rpc(scripts / 'commands.txt', reader, run_token+'-filn-station','autotest_filn_station',args.command_timeout)
            report['commands'].append(result)
            if result['status']!='PASS' or not any('FILN_STATION_HOLOGRAM PASS actual_buildgun=true class=programmable' in line for line in result['lines']):
                raise RuntimeError('FILN station Build Gun/hologram proof failed: '+result['detail'])
            report['filn_station']='actual Build Gun and programmable hologram verified; construction/widget rendering not tested'
        if getattr(args, 'hub_research', False):
            result = rpc(scripts / 'commands.txt', reader, run_token + '-hub-research', 'hub_research', args.command_timeout)
            report['commands'].append(result)
            if result['status'] != 'PASS':
                raise RuntimeError('HUB inspection/repair failed: ' + result['detail'])
            report['hub_research'] = result['lines']
        if getattr(args, 'programmable_stations', False):
            result = rpc(scripts / 'commands.txt', reader, run_token + '-programmable', 'programmable_unlock', args.command_timeout)
            report['commands'].append(result)
            if result['status'] != 'PASS' or not any('PROGRAMMABLE_BUILDGUN actual_listed=true output=1' in line for line in result['lines']):
                raise RuntimeError('Programmable station unlock failed: ' + result['detail'])
            report['programmable_stations'] = 'schematic purchased, recipe/building available, Build Gun list and descriptor category verified; visual UI/placement not tested'
        if getattr(args, 'rest_stop_building', False):
            report['phase'] = 'rest-stop-building'
            result = rpc(scripts / 'commands.txt', reader, run_token + '-rest-stop-building', 'autotest_rest_stop_building', args.command_timeout)
            report['commands'].append(result)
            if result['status'] != 'PASS' or not any('REST_STOP_BUILDING registered=true unlocked=true available=true' in line for line in result['lines']):
                raise RuntimeError('Rest-stop building registration failed: ' + result['detail'])
            report['rest_stop_building'] = 'registered/unlocked/available; placement and hold NOT tested'
        if getattr(args, 'rest_stop_hologram', False):
            report['phase'] = 'rest-stop-hologram'
            result = rpc(scripts / 'commands.txt', reader, run_token + '-rest-stop-hologram', 'autotest_rest_stop_hologram', args.command_timeout)
            report['commands'].append(result)
            if result['status'] != 'PASS' or not any('REST_STOP_HOLOGRAM PASS created=true build_class=true descriptor=true' in line for line in result['lines']):
                raise RuntimeError('Rest-stop hologram creation failed: ' + result['detail'])
            report['rest_stop_hologram'] = 'native build-state hologram created; construction and roads NOT tested'
        report['phase'] = 'station-audit'
        audit = rpc(scripts / 'commands.txt', reader, run_token + '-stations', 'stations', args.command_timeout)
        report['commands'].append(audit)
        summary = re.search(r'HOME_FIXTURES eligible=(\d+) stations=(\d+)', '\n'.join(audit['lines']))
        if audit['status'] != 'PASS' or summary is None:
            raise RuntimeError('Station metadata audit failed: ' + audit['detail'])
        report['eligible_home_count'] = int(summary[1])
        report['home_audit_unavailable'] = sum('home audit unavailable:' in line for line in audit['lines'])
        if int(summary[2]) != report['station_count'] or report['home_audit_unavailable']:
            raise RuntimeError('Incomplete station eligibility audit; inspect API errors in station rows')
        if getattr(args, 'isolate_traffic', False):
            fixture = json.loads((getattr(args, 'fixture_file', None) or ROOT / 'workspace/experiments/FILNLua/test-fixture.json').read_text())
            if digest(latest) != fixture['sha256']:
                raise RuntimeError('Traffic isolation requires the exact pinned fixture')
            home = fixture['vehicle']['route'][0]
            result = rpc(scripts / 'commands.txt', reader, run_token + '-isolate-traffic', 'autotest_isolate_traffic ' + home, args.command_timeout)
            report['commands'].append(result)
            if result['status'] != 'PASS' or not any('TRAFFIC_ISOLATION_PASS' in line and 'routes_unchanged=true snapshot_only=true' in line for line in result['lines']):
                raise RuntimeError('Traffic isolation failed: ' + result['detail'])
            report['traffic_isolation'] = {'status': 'PASS', 'scope': 'snapshot only: ordinary autopilots paused; no actors deleted or routes changed'}
        if getattr(args, 'cargo_audit', False):
            report['phase'] = 'cargo-audit'
            result = rpc(scripts / 'commands.txt', reader, run_token + '-cargo-audit', 'cargo_stations', args.command_timeout)
            report['commands'].append(result)
            if result['status'] != 'PASS' or not any('CARGO_AUDIT stations=' in line and 'errors=0' in line for line in result['lines']):
                raise RuntimeError('Cargo stock audit failed: ' + result['detail'])
            report['cargo_audit'] = {'status': 'PASS', 'scope': 'native station stocks/filters/capacity; no inferred empty requester resource'}
        if getattr(args, 'station_retrofit', False):
            report['phase'] = 'station-retrofit'
            result = rpc(scripts / 'commands.txt', reader, run_token + '-station-retrofit', 'autotest_station_retrofit', args.command_timeout)
            report['commands'].append(result)
            if result['status'] != 'PASS' or not any('STATION_RETROFIT_PASS filters=true insertion_check=true restored=true' in line for line in result['lines']):
                raise RuntimeError('Lua station retrofit failed: ' + result['detail'])
            report['station_retrofit'] = 'same actor/node/name/mode; slot filters and insertion checks; rolled back; docking/save reload unverified'
        report['phase'] = 'vehicle-inspection'
        for number, vehicle in enumerate(vehicles):
            result = rpc(scripts / 'commands.txt', reader, run_token + '-inspect-' + str(number), 'inspect ' + vehicle, args.command_timeout)
            report['commands'].append(result)
            if result['status'] != 'PASS':
                raise RuntimeError('Inspect failed: ' + result['detail'])
        if getattr(args, 'route_bridge', False) and not getattr(args,'orphan_home',None):
            report['phase'] = 'route-bridge'
            bridge = rpc(scripts / 'commands.txt', reader, run_token + '-route-bridge', 'autotest_route_bridge', args.command_timeout)
            report['commands'].append(bridge)
            proof = re.search(r'BRIDGE_PROOF (V\d+) PASS changed_route=true restored_route=true restored_autopilot=true', '\n'.join(bridge['lines']))
            if bridge['status'] != 'PASS' or proof is None:
                raise RuntimeError('Route bridge proof failed: ' + bridge['detail'])
            report['route_bridge'] = {'status': 'PASS', 'vehicle': proof[1], 'scope': 'route write/readback/restore; actual delivery NOT tested'}
        if getattr(args, 'depot_journey', False):
            report['phase'] = 'depot-journey'
            fixture = json.loads((getattr(args, 'fixture_file', None) or ROOT / 'workspace/experiments/FILNLua/test-fixture.json').read_text())
            if digest(latest) != fixture['sha256']:
                raise RuntimeError('Depot journey requires the exact pinned three-rest-stop fixture')
            run_depot_journey(args, report, scripts, reader, run_token, fixture)
        if getattr(args,'blueprint_previews',False):
            report['phase']='blueprint-previews'
            manifest=json.loads((ROOT/'workspace/blueprints/FILN/manifest.json').read_text())
            deadline=time.monotonic()+120
            while True:
                result=rpc(scripts/'commands.txt',reader,run_token+'-blueprint-discovery-'+str(time.time_ns()),'blueprints_verify',args.command_timeout)
                report['commands'].append(result)
                if result['status']=='PASS':break
                if time.monotonic()>=deadline:raise RuntimeError('Blueprint discovery/category membership did not become ready: '+result['detail'])
                time.sleep(10)
            proofs=[]
            for index,entry in enumerate(sorted(manifest['entries'],key=lambda e:e['name']),1):
                result=rpc(scripts/'commands.txt',reader,run_token+'-blueprint-'+str(index),'autotest_blueprints_preview '+str(index),120)
                report['commands'].append(result)
                if result['status']!='PASS' or not any('BLUEPRINT_PREVIEW_PASS index='+str(index)+' name='+entry['name'] in line for line in result['lines']):
                    raise RuntimeError('Native blueprint preview failed: '+entry['name']+' '+result['detail'])
                proofs.append(entry['name']);print('Blueprint preview PASS: '+entry['name'],flush=True)
                time.sleep(.5)
            report['blueprint_previews']={'status':'PASS','copies':proofs,'constructed':False,'scope':'native hologram actor/visual load only; no placed construction'}
        elif getattr(args,'recovery_fsm',False):
            report['phase']='recovery-fsm'
            result=rpc(scripts/'commands.txt',reader,run_token+'-recovery-start','autotest_recovery_fleet',args.command_timeout)
            report['commands'].append(result)
            if result['status']!='PASS': raise RuntimeError('Recovery enrollment failed: '+result['detail'])
            deadline=time.monotonic()+args.journey_timeout
            while time.monotonic()<deadline:
                result=rpc(scripts/'commands.txt',reader,run_token+'-recovery-status-'+str(time.time_ns()),'autotest_recovery_status',args.command_timeout)
                report['commands'].append(result)
                if result['status']!='PASS': raise RuntimeError('Recovery inspection failed: '+result['detail'])
                print('\n'.join(line for line in result['lines'] if 'RECOVERY_NATIVE ' in line),flush=True)
                if any('RECOVERY_FLEET_STATUS ready=4 ordinary_preserved=true' in line for line in result['lines']):
                    report['recovery_fsm']={'status':'PASS','ready':4,'scope':'real stranded trucks returned home and fully refueled; ordinary/disabled vehicles preserved'}
                    break
                time.sleep(5)
            else: raise RuntimeError('Recovery fleet did not return/refuel before deadline; inspect DEPOT_FSM retry reasons')
        elif getattr(args, 'fleet_delivery', False):
            report['phase'] = 'fleet-journey'
            run_fleet_journey(args, report, scripts, reader, run_token)
        elif getattr(args, 'cargo_journey', False) or getattr(args, 'automatic_delivery', False):
            report['phase'] = 'cargo-journey'
            fixture = json.loads((getattr(args, 'fixture_file', None) or ROOT / 'workspace/experiments/FILNLua/test-fixture.json').read_text())
            if digest(latest) != fixture['sha256']:
                raise RuntimeError('Cargo journey requires the exact pinned fixture')
            run_cargo_journey(args, report, scripts, reader, run_token, fixture)
        if getattr(args,'orphan_home',None):
            report['phase']='orphan-recovery'
            result=rpc(scripts/'commands.txt',reader,run_token+'-orphan-start','autotest_orphan_adopt '+args.orphan_home,args.command_timeout)
            report['commands'].append(result)
            if result['status']!='PASS': raise RuntimeError('Orphan adoption failed: '+result['detail'])
            deadline=time.monotonic()+args.journey_timeout
            while time.monotonic()<deadline:
                result=rpc(scripts/'commands.txt',reader,run_token+'-orphan-'+str(time.time_ns()),'autotest_orphan_state',args.command_timeout)
                report['commands'].append(result)
                if result['status']!='PASS': raise RuntimeError('Orphan observer/preservation failed: '+result['detail'])
                print('\n'.join(l for l in result['lines'] if 'ORPHAN_TEST_' in l),flush=True)
                if any('ORPHAN_TEST_RETURNED ' in l for l in result['lines']):
                    report['orphan_recovery']={'status':'PASS','home':args.orphan_home,'scope':'native orphan adoption, remaining-cargo delivery, home-only return; non-test vehicle routes/autopilot preserved'}
                    break
                time.sleep(5)
            else: raise RuntimeError('Orphan recovery did not return empty before deadline; observer evidence retained')
        report['status'] = 'PASS'
        report['phase'] = 'complete'
        if getattr(args,'refuel_only',False):
            print('PASS: stationary native refueling with cargo dispatch paused; no cargo cycle in this run.',flush=True)
        else:
            print(f'PASS: latest-save launch, ping, {len(vehicles)} vehicles, {report["station_count"]} stations and all vehicle inspections.', flush=True)
    finally:
        # Leave no automated test game running, and do not save on exit.
        if installed:
            stop_game(prefix=prefix)
        # Do not replace config/remove the snapshot while an unresponsive game remains.
        if not game_pids():
            config.write_bytes(config_before)
            if token_before is None:
                token_file.unlink(missing_ok=True)
            else:
                token_file.write_bytes(token_before)
            test_save.unlink(missing_ok=True)
        if reader is not None:
            (output / 'live-events.log').write_text('\n'.join(reader.history) + '\n')
        for original, name in ((ue_log, 'ue4ss.log'), (game_log, 'game.log')):
            if original.exists():
                shutil.copy2(original, output / name)
        if latest.exists():
            report['source_unchanged'] = digest(latest) == report['source_sha256']
            if not report['source_unchanged']:
                raise RuntimeError('Original source save changed during test; backup retained for investigation')


def main():
    home = pathlib.Path.home()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixture-file', type=pathlib.Path, help='Explicit alternate fixture metadata for an isolated migration proof')
    parser.add_argument('--game-dir', type=pathlib.Path, default=home / '.local/share/Steam/steamapps/common/Satisfactory')
    parser.add_argument('--save-root', type=pathlib.Path, default=home / '.local/share/Steam/steamapps/compatdata/526870/pfx/drive_c/users/steamuser/AppData/Local/FactoryGame/Saved/SaveGames')
    fixture_path = ROOT / 'workspace/experiments/FILNLua/test-fixture.json'
    fixture = json.loads(fixture_path.read_text()) if fixture_path.is_file() else {}
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument('--save-file', type=pathlib.Path,
                           help='Exact gameplay save; defaults to the pinned user-built fixture when available')
    selection.add_argument('--latest-autosave', action='store_true', help='Use newest real numbered gameplay autosave; exclude manual/calculator/test saves')
    selection.add_argument('--latest-save', action='store_true', help='Explicitly opt out of the pinned fixture and select newest gameplay save')
    parser.add_argument('--start-timeout', type=float, default=1800)
    parser.add_argument('--command-timeout', type=float, default=15)
    parser.add_argument('--min-vehicles', type=int, default=1)
    parser.add_argument('--orphan-home',help='Exact orphan home GUID for observed recovery on the disposable snapshot; requires route-bridge')
    parser.add_argument('--route-bridge', action='store_true', help='Enable reversible route-write proof on the autosave-disabled test snapshot')
    parser.add_argument('--filn-station', action='store_true', help='Verify branded FILN station actual Build Gun listing and native hologram')
    parser.add_argument('--hub-research', action='store_true', help='Inspect and clear a stale purchased milestone on a disposable snapshot')
    parser.add_argument('--programmable-stations', action='store_true', help='Unlock and verify installed Smarter Truck Stations build recipe on a snapshot')
    parser.add_argument('--rest-stop-building', action='store_true', help='Verify generated rest-stop classes, unlock and build availability on the snapshot')
    parser.add_argument('--rest-stop-hologram', action='store_true', help='Exercise native placement hologram creation on the disposable snapshot')
    parser.add_argument('--depot-journey', action='store_true', help='Test dispatcher trip and home-only idle restoration using the pinned fixture')
    parser.add_argument('--soak-seconds', type=float, default=0, help='Repeated production dispatch window; requires fleet-delivery and automatic-delivery')
    parser.add_argument('--journey-timeout', type=float, default=360)
    parser.add_argument('--depot-cycles', type=int, choices=(1, 2), default=2)
    parser.add_argument('--cargo-audit', action='store_true', help='Inspect actual provider/requester item stocks and capacities')
    parser.add_argument('--player-optout', action='store_true', help='Verify native autopilot-off detaches without touching player possession; requires automatic-delivery')
    parser.add_argument('--blueprint-previews', action='store_true', help='Load all FILN copies as native build-gun holograms on a disposable snapshot; never construct')
    parser.add_argument('--recovery-fsm', action='store_true', help='Recover four stranded home-only trucks on a disposable snapshot; preserve ordinary vehicles')
    parser.add_argument('--refuel-only', action='store_true', help='Only verify stationary native refueling with cargo dispatch paused; no delivery-cycle claim')
    parser.add_argument('--checkpoint-fleet', action='store_true', help='After successful fleet proof, write a new configured city checkpoint without overwriting the source')
    parser.add_argument('--verify-refueling', action='store_true', help='Conserve supplied fuel while making snapshot trucks low on fuel, then require observed native home refueling')
    parser.add_argument('--fix-fuel-branches', action='store_true', help='Enable four explicitly pinned disabled fuel branches only in the fleet snapshot and verify dedicated fuel inventory flow')
    parser.add_argument('--starter-fuel', action='store_true', help='Add 100 starter coal only to empty active truck fuel inventories in the disposable fleet snapshot')
    parser.add_argument('--fleet-delivery', action='store_true', help='Validate powered fleet cycles and shortest complete-trip choices while preserving ordinary traffic; requires automatic-delivery')
    parser.add_argument('--expected-fleet-size', type=int, default=4)
    parser.add_argument('--isolate-traffic', action='store_true', help='Pause ordinary vehicle autopilots only on the disposable automatic-delivery snapshot; no deletions or route edits')
    parser.add_argument('--automatic-delivery', action='store_true', help='Test periodic automatic scheduling, full cargo delivery and depot hold on pinned snapshot')
    parser.add_argument('--cargo-journey', action='store_true', help='Select a real stocked LOAD provider and low-stock UNLOAD requester, then test delivery and home return')
    parser.add_argument('--station-retrofit', action='store_true', help='Test native per-slot station filters without replacing the actor, then roll back')
    args = parser.parse_args()
    if args.orphan_home and (not args.route_bridge or not re.fullmatch('[0-9a-f]{32}',args.orphan_home)):
        parser.error('orphan-home requires route-bridge and an exact 32-character lowercase GUID')
    if args.blueprint_previews:
        parser.error('Automated UI previews are disabled by user preference; ask for manual visual validation')
    if args.blueprint_previews and (args.automatic_delivery or args.fleet_delivery or args.recovery_fsm or args.soak_seconds):
        parser.error('blueprint previews require isolated diagnostics without dispatch tests')
    if args.recovery_fsm and (args.automatic_delivery or args.fleet_delivery or args.soak_seconds):
        parser.error('recovery fixture must run without cargo scheduling')
    if args.soak_seconds and (args.soak_seconds<0 or not args.fleet_delivery or not args.automatic_delivery or args.refuel_only or args.starter_fuel or args.verify_refueling or args.fix_fuel_branches):
        parser.error('soak requires fleet automatic delivery without fixture mutations')
    if args.latest_autosave:
        args.save_file=newest_autosave(args.save_root.expanduser().resolve())
    if args.latest_save:
        args.save_file = None
    if args.fixture_file:
        fixture=json.loads(args.fixture_file.read_text())
    if not args.latest_save and args.save_file is None and fixture:
        args.save_file=pathlib.Path(fixture['save_file'])
    args.expected_save_sha256 = fixture.get('sha256') if fixture and args.save_file and args.save_file.expanduser().resolve() == pathlib.Path(fixture['save_file']).resolve() else None
    stamp = dt.datetime.now(dt.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    output = ROOT / 'workspace/test-results' / (stamp + '-' + uuid.uuid4().hex[:8])
    output.mkdir(parents=True)
    report = {'status': 'RUNNING', 'phase': 'init', 'commands': [], 'started_utc': stamp,
              'scope': 'latest-save snapshot diagnostics' + (' and reversible route bridge' if args.route_bridge else '') + (' and depot trip/return/hold with snapshot-only road fault' if args.depot_journey else '') + (' and real cargo delivery/home hold' if args.cargo_journey else '') + '; isolated snapshot, original save preserved'}
    lock_path = ROOT / 'workspace/test-results/.runner.lock'
    with lock_path.open('w') as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            run(args, report, output)
        except KeyboardInterrupt:
            report.update(status='INTERRUPTED', error='Interrupted; snapshot cleanup executed')
        except Blocked as exc:
            report.update(status='BLOCKED', error=str(exc))
        except Exception as exc:
            report.update(status='FAIL', error=f'{type(exc).__name__}: {exc}')
        finally:
            (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
            print(f'{report["status"]}: {report.get("error", "complete")}', flush=True)
            print(f'Report: {output / "report.json"}', flush=True)
    return 0 if report['status'] == 'PASS' else 2 if report['status'] == 'BLOCKED' else 1


if __name__ == '__main__':
    sys.exit(main())
