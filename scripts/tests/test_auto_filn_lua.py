import argparse
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import threading
import time
import unittest
from unittest import mock

SPEC = importlib.util.spec_from_file_location('auto_filn', Path(__file__).parents[1] / 'auto-test-filn-lua.py')
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


class RunnerTests(unittest.TestCase):
    def test_soak_counts_distinct_cycles_and_excludes_unmanaged_homes(self):
        first='DEPOT_CYCLE_PASS home=abc cycle=1 visited=2 loaded=4800 unloaded=4800'
        second=first.replace('cycle=1','cycle=2')
        cycles=runner.soak_cycles([first,first,second,first.replace('home=abc','home=other')],['abc'])
        self.assertEqual(len(cycles),2)
        self.assertEqual(sum(c['items'] for c in cycles.values()),9600)

    def test_soak_rejects_zero_or_partial_cargo_completion(self):
        for loaded,unloaded in [(0,0),(4800,4700)]:
            with self.assertRaises(RuntimeError):
                runner.soak_cycles([f'DEPOT_CYCLE_PASS home=abc cycle=1 loaded={loaded} unloaded={unloaded}'],['abc'])

    def test_latest_autosave_excludes_manual_and_calculator_saves(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);(root/'123').mkdir()
            auto=root/'123/City_autosave_2.sav';auto.write_bytes(b'coal');os.utime(auto,(1,1))
            for name in ['City_manual.sav','City_autosave_2_CALCULATOR.sav']:
                (root/'123'/name).write_bytes(b'newer but not actual autosave')
            self.assertEqual(runner.newest_autosave(root),auto)

    def test_latest_excludes_server_manager_and_automation_snapshot(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            (root / '123').mkdir()
            (root / 'common').mkdir()
            old = root / '123/old.sav'; old.write_bytes(b'a')
            newest = root / '123/new.sav'; newest.write_bytes(b'b')
            os.utime(old, (1, 1)); os.utime(newest, (2, 2))
            (root / 'ServerManager_V2.sav').write_bytes(b'newer metadata')
            (root / 'common/FILN_AutoTest_bad.sav').write_bytes(b'test')
            self.assertEqual(runner.newest_save(root), newest)

    def test_explicit_fixture_wins_over_newer_autosave(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td); (root / '123').mkdir()
            manual = root / '123/fixture.sav'; manual.write_bytes(b'fixture')
            newer = root / '123/newer.sav'; newer.write_bytes(b'wrong world')
            os.utime(manual, (1, 1)); os.utime(newer, (2, 2))
            self.assertEqual(runner.select_save(root, requested=manual), manual)
            with self.assertRaises(runner.Blocked):
                runner.select_save(root, requested=root / '123/missing.sav')
            snapshot = root / '123/FILN_AutoTest_bad.sav'; snapshot.write_bytes(b'snapshot')
            with self.assertRaises(runner.Blocked):
                runner.select_save(root, requested=snapshot)

    def test_changed_pinned_fixture_blocks_before_game_stop(self):
        with tempfile.TemporaryDirectory() as td:
            _, _, source, output, args, report = self.fixture(td)
            args.save_file = source; args.expected_save_sha256 = 'wrong hash'
            with mock.patch.object(runner, 'stop_game') as stop:
                with self.assertRaises(runner.Blocked):
                    runner.run(args, report, output)
                stop.assert_not_called()

    def test_depot_cycle_requires_actual_hold_evidence_and_supports_redispatch(self):
        fixture = {'vehicle': {'route': ['home']}, 'rest_stops': [{'guid': 'home'}, {'guid': 'a'}, {'guid': 'b'}]}
        args = argparse.Namespace(command_timeout=1, journey_timeout=5, depot_cycles=2)
        report = {'commands': []}
        def response(inbox, reader, request, command, timeout):
            lines = ['DEPOT_STATE state=Idle', 'DEPOT_CYCLE_PASS home=home'] if command.startswith('autotest_depot_status') else []
            markers = {'autotest_depot_guard': 'DEPOT_GUARD_PASS', 'autotest_depot_break_road': 'DEPOT_ROAD_FAULT', 'autotest_depot_blocked': 'DEPOT_BLOCKED_PASS'}
            verb = command.split()[0]
            if verb in markers: lines = [markers[verb]]
            return {'command': command, 'status': 'PASS', 'detail': '', 'lines': lines}
        with mock.patch.object(runner, 'rpc', side_effect=response) as rpc, mock.patch.object(runner.time, 'sleep'):
            runner.run_depot_journey(args, report, Path('/tmp'), None, 'token', fixture)
        self.assertEqual(report['depot_journey']['cycles'], 2)
        self.assertEqual(rpc.call_count, 11)
        self.assertEqual(report['depot_journey']['status'], 'PASS')
        report = {'commands': []}
        def no_proof(*params):
            return {'status': 'PASS', 'detail': '', 'lines': ['DEPOT_STATE state=Idle']}
        with mock.patch.object(runner, 'rpc', side_effect=no_proof):
            with self.assertRaisesRegex(RuntimeError, 'without actual cycle'):
                runner.run_depot_journey(args, report, Path('/tmp'), None, 'token', fixture)

    def test_depot_recovery_cannot_pass(self):
        fixture = {'vehicle': {'route': ['home']}, 'rest_stops': [{'guid': 'home'}, {'guid': 'a'}, {'guid': 'b'}]}
        args = argparse.Namespace(command_timeout=1, journey_timeout=5, depot_cycles=1)
        def response(inbox, reader, request, command, timeout):
            failed = command.startswith('autotest_depot_status')
            return {'status': 'FAIL' if failed else 'PASS', 'detail': 'active error', 'lines': ['DEPOT_STATE state=Recovery']}
        with mock.patch.object(runner, 'rpc', side_effect=response):
            with self.assertRaisesRegex(RuntimeError, 'Depot journey failed'):
                runner.run_depot_journey(args, {'commands': []}, Path('/tmp'), None, 'token', fixture)

    def test_cargo_proof_requires_actual_nonzero_equal_load_and_unload(self):
        fixture = {'vehicle': {'route': ['home']}}
        args = argparse.Namespace(command_timeout=1, journey_timeout=5)
        def trial(proof):
            def response(inbox, reader, request, command, timeout):
                if command.startswith('autotest_cargo_select'):
                    lines = ['CARGO_SELECTED home=home provider=provider requester=requester item=/Game/Item supply=4800 requested_stock=0 capacity=4800 road_cm=24500']
                elif command.startswith('autotest_depot_status'):
                    lines = ['DEPOT_STATE state=Idle', proof]
                else: lines = []
                return {'status': 'PASS', 'detail': '', 'lines': lines}
            report = {'commands': []}
            with mock.patch.object(runner, 'rpc', side_effect=response):
                runner.run_cargo_journey(args, report, Path('/tmp'), None, 'token', fixture)
            return report
        report = trial('DEPOT_CYCLE_PASS home=home loaded=100 unloaded=100')
        self.assertEqual(report['cargo_job']['status'], 'PASS')
        self.assertEqual(report['cargo_job']['loaded'], 100)
        for proof in ('', 'DEPOT_CYCLE_PASS home=home loaded=0 unloaded=0', 'DEPOT_CYCLE_PASS home=home loaded=100 unloaded=50'):
            with self.assertRaisesRegex(RuntimeError, 'lacks actual load/unload'):
                trial(proof)

    def test_traffic_isolation_requires_automatic_snapshot_before_actions(self):
        args = argparse.Namespace(isolate_traffic=True, automatic_delivery=False)
        with mock.patch.object(runner, 'stop_game') as stop:
            with self.assertRaisesRegex(runner.Blocked, 'requires automatic-delivery'):
                runner.run(args, {}, Path('/tmp'))
            stop.assert_not_called()

    def test_cargo_suspended_state_fails_without_waiting_for_timeout(self):
        args = argparse.Namespace(command_timeout=1, journey_timeout=5)
        def response(inbox, reader, request, command, timeout):
            if command.startswith('autotest_cargo_select'):
                lines = ['CARGO_SELECTED home=home provider=p requester=r item=/Game/Item supply=4800 requested_stock=0 capacity=4800 road_cm=1']
            elif command.startswith('autotest_depot_status'):
                lines = ['DEPOT_STATE state=Suspended error=residual-cargo']
            else: lines = []
            return {'status': 'PASS', 'detail': '', 'lines': lines}
        with mock.patch.object(runner, 'rpc', side_effect=response), mock.patch.object(runner.time, 'sleep') as sleep:
            with self.assertRaisesRegex(RuntimeError, 'Cargo journey stopped'):
                runner.run_cargo_journey(args, {'commands': []}, Path('/tmp'), None, 'token', {'vehicle': {'route': ['home']}})
            sleep.assert_not_called()

    def test_automatic_report_uses_started_job_instead_of_preview(self):
        lines = [
            'CARGO_SELECTED home=h provider=p requester=preview item=/Game/Old supply=100 requested_stock=0 capacity=100 road_cm=100',
            'CARGO_SELECTED home=h provider=p requester=actual item=/Game/Item supply=4800 requested_stock=0 capacity=4800 road_cm=200',
            'AUTO_DISPATCH_STARTED home=h provider=p requester=actual truckload=4800',
        ]
        job = runner.automatic_assignment(lines, 'h')
        self.assertEqual(job['requester'], 'actual')
        self.assertEqual(job['item'], '/Game/Item')
        self.assertEqual(job['truckload'], 4800)
        with self.assertRaisesRegex(RuntimeError, 'Missing actual'):
            runner.automatic_assignment(lines[:2], 'h')
        with self.assertRaisesRegex(RuntimeError, 'lacks matching'):
            runner.automatic_assignment(lines[2:], 'h')

    def test_fleet_ranking_requires_true_minimum_and_consistent_receipt(self):
        candidates = ['FLEET_CANDIDATE round=1 home=a total_cm=100.000', 'FLEET_CANDIDATE round=1 home=b total_cm=20.000']
        self.assertEqual(runner.fleet_choices(candidates+['FLEET_CHOICE round=1 home=b total_cm=20.000 compared=2'])[0]['home'], 'b')
        for choice in ('home=a total_cm=100.000 compared=2', 'home=a total_cm=1.000 compared=2', 'home=b total_cm=20.000 compared=3'):
            with self.assertRaisesRegex(RuntimeError, 'minimum'):
                runner.fleet_choices(candidates+['FLEET_CHOICE round=1 '+choice])

    def test_fleet_mode_rejects_traffic_isolation_before_stopping_game(self):
        args = argparse.Namespace(fleet_delivery=True, automatic_delivery=True, isolate_traffic=True)
        with mock.patch.object(runner, 'stop_game') as stop:
            with self.assertRaisesRegex(runner.Blocked, 'preserves ordinary traffic'):
                runner.run(args, {}, Path('/tmp'))
            stop.assert_not_called()

    def test_fleet_completion_requires_each_vehicles_cargo_proof(self):
        args = argparse.Namespace(expected_fleet_size=2, command_timeout=1, journey_timeout=5)
        def response(inbox, reader, request, command, timeout):
            if command.startswith('autotest_fleet_audit'):
                lines=['FLEET_VEHICLE home=a id=V1', 'FLEET_VEHICLE home=b id=V2', 'FLEET_AUDIT_PASS enabled=2']
            elif command=='autotest_fleet_fuel_ready':
                lines=['FLEET_FUEL_READY all=true']
            elif command=='autotest_fleet_status':
                lines=['FLEET_JOB home=a state=Idle loaded=100 unloaded=100', 'FLEET_JOB home=b state=Idle loaded=100 unloaded=100', 'FLEET_STATUS_PASS completed=2 ordinary_preserved=true disabled_excluded=true reservations_unique=true']
            else:lines=[]
            return {'status':'PASS','detail':'','lines':lines}
        history=['FLEET_CANDIDATE round=1 home=a total_cm=10.000', 'FLEET_CHOICE round=1 home=a total_cm=10.000 compared=1', 'DEPOT_CYCLE_PASS home=a loaded=100 unloaded=100', 'DEPOT_CYCLE_PASS home=b loaded=100 unloaded=100']
        reader=argparse.Namespace(history=history)
        with mock.patch.object(runner, 'rpc', side_effect=response):
            report={'commands':[]};runner.run_fleet_journey(args,report,Path('/tmp'),reader,'test')
            self.assertEqual(report['fleet']['status'],'PASS')
            reader.history=history[:-1]
            with self.assertRaisesRegex(RuntimeError,'per-vehicle cargo'):
                runner.run_fleet_journey(args,{'commands':[]},Path('/tmp'),reader,'test')

    def test_starter_fuel_is_only_allowed_on_fleet_snapshot(self):
        with mock.patch.object(runner, 'stop_game') as stop:
            with self.assertRaisesRegex(runner.Blocked, 'disposable fleet test'):
                runner.run(argparse.Namespace(starter_fuel=True, fleet_delivery=False),{},Path('/tmp'))
            stop.assert_not_called()

    def test_refuel_and_fuel_branch_fixtures_require_explicit_fleet_mode(self):
        for args in (argparse.Namespace(verify_refueling=True, fix_fuel_branches=False), argparse.Namespace(fix_fuel_branches=True, fleet_delivery=False)):
            with mock.patch.object(runner, 'stop_game') as stop:
                with self.assertRaises(runner.Blocked):runner.run(args,{},Path('/tmp'))
                stop.assert_not_called()

    def test_refuel_only_proof_never_starts_cargo_dispatch(self):
        homes=[str(i)*32 for i in range(1,5)]
        plan=[{'home':h,'actor_suffix':'Build_ConveyorAttachmentSplitterSmart_C_'+str(i)} for i,h in enumerate(homes,1)]
        args=argparse.Namespace(expected_fleet_size=4,command_timeout=1,journey_timeout=5,verify_refueling=True,fix_fuel_branches=True,refuel_only=True,fixture_file=Path('/fixture.json'))
        sent=[]
        def response(inbox,reader,request,command,timeout):
            sent.append(command)
            verb=command.split()[0]
            rows={'autotest_fleet_audit':['FLEET_VEHICLE home='+h+' id=V'+str(i) for i,h in enumerate(homes,1)]+['FLEET_AUDIT_PASS enabled=4'],
                  'autotest_fleet_fix_fuel_rules':['FUEL_BRANCH_FIX_PASS changed=4'],
                  'autotest_fleet_depot_fuel_ready':['DEPOT_FUEL_READY count=4 cargo_empty=true'],
                  'autotest_fleet_fuel_ready':['FLEET_FUEL_READY all=true'],
                  'autotest_fleet_stationary_refuel_ready':['STATIONARY_REFUEL_PASS homes=4 dispatch_paused=true routes_unchanged=true ordinary_preserved=true']}
            return {'status':'PASS','detail':'','lines':rows.get(verb,[])}
        with mock.patch.object(runner,'rpc',side_effect=response),mock.patch.object(Path,'read_text',return_value=__import__('json').dumps({'fleet':{'fuel_splitters':plan}})):
            report={'commands':[]};runner.run_fleet_journey(args,report,Path('/tmp'),argparse.Namespace(history=[]),'test')
        self.assertEqual(report['stationary_refueling']['status'],'PASS')
        self.assertEqual(report['fleet']['status'],'NOT_RUN')
        self.assertNotIn('auto_start',sent)

    def test_fleet_source_mismatch_blocks_before_game_shutdown(self):
        with tempfile.TemporaryDirectory() as td:
            _,_,source,output,args,report=self.fixture(td)
            args.fleet_delivery=True;args.automatic_delivery=True;args.save_file=source
            with mock.patch.object(Path,'read_text',return_value=__import__('json').dumps({'sha256':'different-source'})),mock.patch.object(runner,'stop_game') as stop:
                with self.assertRaisesRegex(runner.Blocked,'exact pinned fixture'):
                    runner.run(args,report,output)
                stop.assert_not_called()

    def test_cargo_selection_failure_prevents_departure(self):
        args = argparse.Namespace(command_timeout=1, journey_timeout=5)
        with mock.patch.object(runner, 'rpc', return_value={'status': 'FAIL', 'detail': 'no supply', 'lines': []}) as rpc:
            with self.assertRaisesRegex(RuntimeError, 'selection failed'):
                runner.run_cargo_journey(args, {'commands': []}, Path('/tmp'), None, 'token', {'vehicle': {'route': ['home']}})
            self.assertEqual(rpc.call_count, 1)

    def test_launch_arguments_are_named_save_map_override(self):
        args = runner.autoload_args('FILN_AutoTest_012abc')
        self.assertIn('?loadgame=FILN_AutoTest_012abc', args[1])
        self.assertIn('GameDefaultMap=' + runner.MAP, args[1])
        with self.assertRaises(ValueError):
            runner.autoload_args('bad?listen')

    def test_log_reader_handles_replacement_and_partial_line(self):
        with tempfile.TemporaryDirectory() as td:
            p = Path(td) / 'log'
            p.write_text('stale\npartial')
            reader = runner.LogReader(p)
            self.assertEqual(reader.lines(), ['stale'])
            replacement = Path(td) / 'replacement'
            replacement.write_text('fresh\n'); replacement.replace(p)
            self.assertEqual(reader.lines(), ['fresh'])

    def test_process_detection_includes_bootstrap_but_excludes_steam_and_proton(self):
        with tempfile.TemporaryDirectory() as td:
            proc = Path(td)
            rows = [
                ('1', 'FactoryGameStea', 'S:\\game\\FactoryGameSteam.exe'),
                ('2', 'FactoryGameStea', 'S:\\game\\Engine\\Binaries\\Win64\\FactoryGameSteam-Win64-Shipping.exe'),
                ('3', 'steam', 'steam'),
                ('4', 'python3', 'proton\0S:\\game\\FactoryGameSteam.exe'),
            ]
            for pid, comm, cmd in rows:
                p = proc / pid; p.mkdir()
                (p / 'comm').write_text(comm)
                (p / 'cmdline').write_bytes(cmd.encode() + b'\0')
            original_path = runner.pathlib.Path
            def path(value):
                return proc if value == '/proc' else original_path(value)
            with mock.patch.object(runner.pathlib, 'Path', side_effect=path):
                self.assertCountEqual(runner.game_pids(), [1, 2])

    def test_wine_cleanup_is_limited_to_selected_game_prefix(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td); proc = root / 'proc'; proc.mkdir()
            selected = root / '526870/pfx'; selected.mkdir(parents=True)
            other = root / 'other/pfx'; other.mkdir(parents=True)
            binary = root / 'wineserver'; binary.write_text('fixture')
            for pid, appid, prefix in [('1', '526870', selected), ('2', '999', other), ('3', '526870', other)]:
                p = proc / pid; p.mkdir(); (p / 'comm').write_text('wineserver')
                (p / 'environ').write_bytes(f'SteamAppId={appid}\0WINEPREFIX={prefix}\0'.encode())
                (p / 'exe').symlink_to(binary)
            original_path = runner.pathlib.Path
            def path(value):
                return proc if value == '/proc' else original_path(value)
            with mock.patch.object(runner.pathlib, 'Path', side_effect=path), mock.patch.object(runner.subprocess, 'run') as run:
                runner.close_wine_prefix(selected)
                self.assertEqual([c.args[0] for c in run.call_args_list], [[str(binary), '-k'], [str(binary), '-w']])
                self.assertEqual(run.call_args_list[0].kwargs['env']['WINEPREFIX'], str(selected))

    def fixture(self, directory):
        root = Path(directory)
        game = root / 'steamapps/common/Satisfactory'
        scripts = game / 'Engine/Binaries/Win64/ue4ss/Mods/FILNLua/Scripts'
        scripts.mkdir(parents=True)
        for name in ('main.lua', 'Core.lua', 'Config.lua'):
            (scripts / name).write_text('-- original ' + name)
        saves = root / 'Saved/SaveGames'; (saves / '123').mkdir(parents=True)
        (saves / 'common').mkdir()
        source = saves / '123/latest.sav'; source.write_bytes(b'latest-gameplay-snapshot')
        output = root / 'results'; output.mkdir()
        args = argparse.Namespace(game_dir=game, save_root=saves, start_timeout=2, command_timeout=2, min_vehicles=1)
        report = {'commands': [], 'status': 'RUNNING'}
        return scripts, saves, source, output, args, report

    def test_blocked_preflight_never_stops_game_or_updates_files(self):
        with tempfile.TemporaryDirectory() as td:
            scripts, _, _, output, args, report = self.fixture(td)
            with mock.patch.object(runner, 'writable', side_effect=runner.Blocked('read-only')), mock.patch.object(runner, 'stop_game') as stop:
                with self.assertRaises(runner.Blocked):
                    runner.run(args, report, output)
                stop.assert_not_called()
            self.assertEqual((scripts / 'main.lua').read_text(), '-- original main.lua')

    def complete_pipeline(self, mode):
        with tempfile.TemporaryDirectory() as td:
            scripts, saves, source, output, args, report = self.fixture(td)
            if mode == 'delayed-ping':
                args.command_timeout = .3
            args.route_bridge = mode in ('bridge', 'bridge-error')
            args.station_retrofit = mode in ('retrofit', 'retrofit-missing-proof')
            args.rest_stop_building = mode in ('building', 'building-missing-proof', 'hologram', 'hologram-missing-proof')
            args.rest_stop_hologram = mode in ('hologram', 'hologram-missing-proof')
            actual_run = subprocess.run
            done = threading.Event()
            workers = []
            def fake_launch(command, *positional, **kwargs):
                if command[0] != '/fixture/steam':
                    return actual_run(command, *positional, **kwargs)
                token = (scripts / 'AutoTestRun.lua').read_text().split('"')[1]
                alias = 'FILN_AutoTest_' + token
                self.assertEqual((saves / 'common' / (alias + '.sav')).read_bytes(), source.read_bytes())
                ue_log = scripts.parents[2] / 'UE4SS.log'
                ue_log.write_text('[FILN-Lua] AUTOTEST_SESSION ' + token + '\n')
                game_log = saves.parent / 'Logs/FactoryGame.log'; game_log.parent.mkdir(exist_ok=True)
                game_log.write_text('LogLoad: LoadMap: ' + runner.MAP + '?loadgame=' + alias + '?listen\n')
                def respond():
                    consumed = 0
                    deadline = time.monotonic() + 10
                    while not done.is_set() and time.monotonic() < deadline:
                        inbox = scripts / 'commands.txt'
                        text = inbox.read_text() if inbox.exists() else ''
                        for line in text[consumed:].splitlines():
                            request, verb, *params = line.split()
                            request = request[1:]
                            if mode == 'delayed-ping' and verb == 'ping':
                                time.sleep(.5)  # longer than normal command timeout, within startup budget
                            with ue_log.open('a') as stream:
                                if verb == 'scan' and mode != 'empty':
                                    stream.write('[FILN-Lua] V1 runtime-vehicle\n[FILN-Lua] S1 runtime-station\n')
                                if verb == 'autotest_prepare':
                                    stream.write('[FILN-Lua] AUTOTEST_PREPARED autosave=false\n')
                                if verb == 'autotest_rest_stop_building' and mode in ('building','hologram','hologram-missing-proof'):
                                    stream.write('[FILN-Lua] REST_STOP_BUILDING registered=true unlocked=true available=true\n')
                                if verb == 'autotest_rest_stop_hologram' and mode == 'hologram':
                                    stream.write('[FILN-Lua] REST_STOP_HOLOGRAM PASS created=true build_class=true descriptor=true\n')
                                if verb == 'autotest_station_retrofit' and mode == 'retrofit':
                                    stream.write('[FILN-Lua] STATION_RETROFIT_PASS filters=true insertion_check=true restored=true\n')
                                if verb == 'stations':
                                    stream.write('[FILN-Lua] HOME_FIXTURES eligible=0 stations=1\n')
                                if verb == 'autotest_route_bridge' and mode != 'bridge-error':
                                    stream.write('[FILN-Lua] BRIDGE_PROOF V1 PASS changed_route=true restored_route=true restored_autopilot=true\n')
                                status = 'FAIL' if mode == 'lua-error' and verb == 'scan' else 'PASS'
                                if mode == 'bridge-error' and verb == 'autotest_route_bridge': status='FAIL'
                                stream.write(f'[FILN-Lua] AUTOTEST_RESULT {request} {status} {verb} '+ ('adapter broke' if status == 'FAIL' else '') + '\n')
                        consumed = len(text)
                        time.sleep(.01)
                thread = threading.Thread(target=respond); thread.start(); workers.append(thread)
                return subprocess.CompletedProcess(command, 0)
            try:
                with mock.patch.object(runner.shutil, 'which', return_value='/fixture/steam'), mock.patch.object(runner.subprocess, 'run', side_effect=fake_launch), mock.patch.object(runner, 'stop_game') as stop, mock.patch.object(runner, 'game_pids', return_value=[]), mock.patch.dict(os.environ, {'DISPLAY': ':0'}):
                    if mode in ('pass', 'delayed-ping', 'bridge', 'building', 'hologram', 'retrofit'):
                        runner.run(args, report, output)
                        self.assertEqual(report['status'], 'PASS')
                        expected=['ping','scan','autotest_prepare','stations','inspect V1']
                        if mode in ('building','hologram'):expected.insert(3,'autotest_rest_stop_building')
                        if mode=='hologram':expected.insert(4,'autotest_rest_stop_hologram')
                        if mode=='retrofit':expected.insert(4,'autotest_station_retrofit')
                        if mode=='bridge':expected.append('autotest_route_bridge')
                        self.assertEqual([c['command'] for c in report['commands']], expected)
                        self.assertEqual(report['vehicle_count'], 1)
                        self.assertTrue(report['autosave_disabled'])
                        if mode=='bridge':self.assertEqual(report['route_bridge']['status'],'PASS')
                    else:
                        with self.assertRaises(RuntimeError):
                            runner.run(args, report, output)
                    self.assertEqual(stop.call_count, 2)
                self.assertEqual((scripts / 'Config.lua').read_text(), '-- original Config.lua')
                self.assertFalse((scripts / 'AutoTestRun.lua').exists())
                self.assertEqual(list((saves / 'common').glob('FILN_AutoTest_*.sav')), [])
                self.assertTrue(report['source_unchanged'])
                self.assertTrue((output / 'save-backup/123/latest.sav').is_file())
                self.assertTrue((output / 'ue4ss.log').is_file())
            finally:
                done.set()
                for thread in workers:
                    thread.join(timeout=2)

    def test_complete_mocked_lifecycle(self):
        self.complete_pipeline('pass')

    def test_first_ping_uses_startup_timeout(self):
        self.complete_pipeline('delayed-ping')

    def test_lua_failure_cannot_pass(self):
        self.complete_pipeline('lua-error')

    def test_opt_in_route_bridge_lifecycle(self):
        self.complete_pipeline('bridge')

    def test_route_bridge_failure_cannot_pass(self):
        self.complete_pipeline('bridge-error')

    def test_building_registration_lifecycle(self):
        self.complete_pipeline('building')

    def test_building_receipt_without_evidence_cannot_pass(self):
        self.complete_pipeline('building-missing-proof')

    def test_hologram_creation_lifecycle(self):
        self.complete_pipeline('hologram')

    def test_hologram_receipt_without_creation_cannot_pass(self):
        self.complete_pipeline('hologram-missing-proof')

    def test_station_retrofit_lifecycle(self):
        self.complete_pipeline('retrofit')

    def test_retrofit_receipt_without_native_evidence_cannot_pass(self):
        self.complete_pipeline('retrofit-missing-proof')

    def test_empty_world_fallback_cannot_pass(self):
        self.complete_pipeline('empty')


if __name__ == '__main__':
    unittest.main()
