import copy
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import threading
import time
import unittest
import urllib.error
import urllib.request

SCRIPTS = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(SCRIPTS))
from filn_dashboard_model import analyze
spec=importlib.util.spec_from_file_location('filn_dashboard', SCRIPTS/'filn-dashboard.py')
server_module=importlib.util.module_from_spec(spec);spec.loader.exec_module(server_module)


def fixture():
    def station(guid,role,amount):
        return dict(guid=guid,id=guid,name=guid,role=role,amount=amount,item='iron',configured=True,powered=True,standby=False,is_depot=False,capacity=4800,contents=[dict(item='iron',amount=amount)] if amount else [],sampled_at=100)
    return dict(schema_version=1,world='city',sequence=1,exported_at=100,
                stations=[station('provider1','provider',3000),station('provider2','provider',2000),station('requester','requester',0)],
                vehicles=[dict(id='V1',**{'class':'Truck'},cargo=dict(slots=48,amount=0),fuel=dict(amount=100,capacity=100),home='home',managed=True,home_powered=True,autopilot=True,has_fuel=True)],
                capacity_samples=[dict(**{'class':'Truck'},slots=48,by_item={'iron':4800})],errors=[],dispatcher=dict(enabled=True,reservations={}))

class ModelTests(unittest.TestCase):
    def request(self,s,now=100,history=None):return analyze(s,now,history)['requests'][0]
    def test_split_stock_is_not_a_full_single_provider(self):
        r=self.request(fixture());self.assertEqual(r['code'],'split_stock');self.assertEqual(r['missing'],1800);self.assertEqual(r['combined_stock'],5000)
    def test_full_provider_pending_route_is_not_claimed_disconnected(self):
        s=fixture();s['stations'][0]['amount']=4800
        self.assertEqual(self.request(s)['code'],'awaiting_dispatcher_check')
    def test_recent_route_failures_scoped_to_every_available_vehicle(self):
        s=fixture();s['stations'][0]['amount']=4800
        s['dispatcher']['request_checks']={'home':{'requester':dict(at=100,item='iron',reason='different_network')}}
        self.assertEqual(self.request(s)['code'],'route_checks_failed')
        self.assertEqual(self.request(s,135)['code'],'awaiting_dispatcher_check')
        other=copy.deepcopy(s['vehicles'][0]);other.update(id='V2',home='untested');s['vehicles'].append(other)
        self.assertEqual(self.request(s)['code'],'awaiting_dispatcher_check')
    def test_route_failure_names_the_provider_and_failed_leg(self):
        s=fixture();s['stations'][0]['amount']=4800
        s['dispatcher']['request_checks']={'home':{'requester':dict(at=100,item='iron',reason='unreachable_legs',failures=[dict(provider='provider1',leg='provider_to_requester',reason='provider_other_network')])}}
        r=self.request(s);self.assertEqual(r['code'],'route_checks_failed')
        self.assertIn('provider1: provider to requester',r['reason']);self.assertIn('provider other network',r['reason'])
    def test_nonempty_requester_inactive_and_all_inventory_retained(self):
        s=fixture();s['stations'][2]['amount']=8
        self.assertEqual(self.request(s)['code'],'stock_present');self.assertEqual(len(analyze(s,100)['inventories']),3)
    def test_empty_unconfigured_provider_not_inferred(self):
        s=fixture();s['stations'][0]['configured']=False;s['stations'][1]['configured']=False
        self.assertEqual(self.request(s)['code'],'no_observed_provider')
    def test_reserved_provider_cannot_satisfy_available_stock(self):
        s=fixture();s['dispatcher']['reservations']['provider1']='other'
        r=self.request(s);self.assertEqual(r['best_stock'],2000);self.assertEqual(r['code'],'provider_short')
    def test_stale_and_missing_samples_prevent_bottleneck_claim(self):
        s=fixture();self.assertEqual(self.request(s,200)['code'],'incomplete')
        s['errors']=[dict(error='missing station')];self.assertEqual(self.request(s)['code'],'incomplete')
    def test_fuel_gate_full_minus_one_not_available(self):
        s=fixture();s['stations'][0]['amount']=4800;s['vehicles'][0]['fuel']['amount']=99
        r=analyze(s,100);self.assertEqual(r['counts']['available'],0);self.assertEqual(r['fleet'][0]['status'],'Waiting for depot refuel');self.assertEqual(r['requests'][0]['code'],'no_available_truck')
    def test_cargo_at_depot_is_blocked_not_available(self):
        s=fixture();s['vehicles'][0]['cargo']['amount']=647
        d=analyze(s,100);self.assertEqual(d['fleet'][0]['status'],'Blocked: cargo remains')
        self.assertEqual(d['counts']['available'],0);self.assertEqual(d['counts']['cargo_blocked'],1)
    def test_opted_out_route_can_be_displayed_without_enrollment(self):
        s=fixture();s['stations'].append(dict(guid='depot',name='Depot',id='S0',is_depot=True))
        s['stations'][2]['amount']=4430
        v=s['vehicles'][0];v.update(managed=False,autopilot=False,home=None,route=['depot','provider1','requester','depot'])
        v['cargo'].update(amount=3781,item='iron')
        d=analyze(s,100);row=d['fleet'][0]
        self.assertFalse(row['managed']);self.assertTrue(row['observed_route'])
        self.assertEqual(row['status'],'Outside dispatcher / autopilot off')
        self.assertEqual(row['requester_free_space'],370);self.assertEqual(row['unload_space_shortfall'],3411)
        self.assertEqual(d['counts']['active'],0)
    def test_untracked_filn_route_shows_reconciliation_blocker(self):
        s=fixture();s['stations'].append(dict(guid='depot',name='Depot',id='S0',is_depot=True))
        s['vehicles'][0].update(key='truck',managed=False,route=['depot','provider1','requester','depot'])
        s['dispatcher']['orphans']={'truck':dict(status='blocked',reason='endpoint reserved by another delivery',home='depot')}
        d=analyze(s,100);self.assertEqual(d['fleet'][0]['status'],'Untracked FILN route / blocked')
        self.assertIn('endpoint reserved',d['fleet'][0]['dispatcher_reason']);self.assertEqual(d['counts']['untracked_routes'],1)
        self.assertEqual(d['counts']['available'],0)
    def test_ordinary_two_stop_coal_route_not_labeled_orphan(self):
        s=fixture();s['vehicles'][0].update(managed=False,route=['provider1','requester'])
        d=analyze(s,100);self.assertEqual(d['fleet'][0]['status'],'Outside FILN / no depot route')
        self.assertEqual(d['counts']['untracked_routes'],0)
    def test_latest_recovery_failure_is_visible_instead_of_only_original_fault(self):
        s=fixture();s['vehicles'][0]['job']=dict(state='Recovery',recovery='Replan',error='returned with cargo',recovery_details=dict(attempts=3,next_retry=120,last_error='destination unreachable'))
        row=analyze(s,100)['fleet'][0]
        self.assertEqual(row['dispatcher_reason'],'destination unreachable');self.assertEqual(row['job']['error'],'returned with cargo')
    def test_detached_vehicle_with_autopilot_on_is_not_counted_as_active(self):
        s=fixture();s['vehicles'][0]['job']=dict(state='Detached')
        d=analyze(s,100);self.assertEqual(d['counts']['active'],0);self.assertEqual(d['counts']['available'],0)
    def test_two_stop_route_with_filn_depot_is_not_an_ordinary_cargo_route(self):
        s=fixture();s['stations'].append(dict(guid='depot',name='Depot',id='S0',is_depot=True))
        s['vehicles'][0].update(managed=False,route=['requester','depot'])
        self.assertEqual(analyze(s,100)['fleet'][0]['status'],'Untracked FILN route / incomplete schedule')
    def test_busy_truck_with_cargo_is_not_an_idle_cargo_blocker(self):
        s=fixture();s['vehicles'][0].update(job=dict(state='Trip',visited=1));s['vehicles'][0]['cargo']['amount']=4800
        d=analyze(s,100);self.assertEqual(d['counts']['working'],1);self.assertEqual(d['counts']['cargo_blocked'],0)
    def test_idle_home_ready_recovery_is_not_failed(self):
        s=fixture();s['vehicles'][0]['job']=dict(state='Idle',recovery='HomeReady')
        self.assertEqual(analyze(s,100)['counts']['available'],1)
    def test_driver_and_opt_out_are_not_available(self):
        for field,value in [('driver',True),('autopilot',False)]:
            s=fixture();s['vehicles'][0][field]=value
            self.assertEqual(analyze(s,100)['counts']['available'],0)
    def test_job_departure_target_index_does_not_mean_returning(self):
        s=fixture();s['vehicles'][0].update(target=3,job=dict(state='Trip',visited=0))
        self.assertEqual(analyze(s,100)['fleet'][0]['status'],'Going to provider / loading')
    def test_delivery_priority_and_vehicle_visible(self):
        s=fixture();s['vehicles'][0]['job']=dict(state='Trip',requester='requester',visited=1)
        r=self.request(s);self.assertEqual(r['code'],'delivery_in_progress');self.assertEqual(r['vehicles'],['V1'])
    def test_net_rate_and_eta_only_after_two_observations(self):
        s=fixture();h={};self.assertIsNone(self.request(s,100,h)['batch_fill_seconds'])
        s['exported_at']=110;s['stations'][0].update(amount=3100,sampled_at=110)
        r=self.request(s,110,h);self.assertEqual(r['batch_fill_seconds'],170)
        self.assertEqual(r['providers'][0]['net_items_per_minute'],600)
    def test_transfer_and_long_gaps_suppress_rate(self):
        for occupied,gap in [(True,10),(False,80)]:
            s=fixture();h={};self.request(s,100,h);s['exported_at']=100+gap;s['stations'][0].update(amount=3100,sampled_at=100+gap,occupied=occupied)
            self.assertIsNone(self.request(s,100+gap,h)['batch_fill_seconds'])
    def test_empty_observation_resets_on_world_change(self):
        s=fixture();h={};self.request(s,100,h);self.assertEqual(self.request(s,110,h)['observed_empty_seconds'],10)
        s['world']='other';s['stations'][2]['sampled_at']=110;s['exported_at']=110
        self.assertEqual(self.request(s,110,h)['observed_empty_seconds'],0)

class HTTPTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.path=Path(self.temp.name)
        self.server=server_module.make_server(self.path,0);self.thread=threading.Thread(target=self.server.serve_forever,daemon=True);self.thread.start()
        self.base=f'http://127.0.0.1:{self.server.server_port}'
    def tearDown(self):self.server.shutdown();self.server.server_close();self.temp.cleanup()
    def get(self,path):
        with urllib.request.urlopen(self.base+path) as r:return r.read()
    def test_no_snapshot_returns_explicit_waiting_and_requests_sampling(self):
        data=json.loads(self.get('/api/status'));self.assertFalse(data['ready']);self.assertIsNone(data['data']);self.assertTrue((self.path/'dashboard-demand.txt').exists())
    def test_bad_snapshot_keeps_last_good_then_marks_stale(self):
        data=fixture();data['exported_at']=time.time();(self.path/'telemetry.json').write_text(json.dumps(data))
        self.assertTrue(json.loads(self.get('/api/status'))['ready'])
        (self.path/'telemetry.json').write_text('{')
        response=json.loads(self.get('/api/status'));self.assertTrue(response['ready']);self.assertIsNotNone(response['error'])
    def test_static_assets_health_and_no_file_or_control_routes(self):
        self.assertIn(b'Logistics monitor',self.get('/'));self.assertIn(b'render',self.get('/app.js'));self.assertIn(b'color',self.get('/style.css'))
        self.assertEqual(json.loads(self.get('/health'))['service'],'filn-dashboard')
        for path in ['/../Config.lua','/api/dispatch','/api/save']:
            with self.assertRaises(urllib.error.HTTPError) as e:self.get(path)
            self.assertEqual(e.exception.code,404)
    def test_icons_are_local_and_no_extracted_assets_are_shipped(self):
        with self.assertRaises(urllib.error.HTTPError) as e:self.get('/icons/desc-oreiron-c_64.png')
        self.assertEqual(e.exception.code,404)
        self.assertIn(b'<svg',self.get('/icons/unknown.svg'))
        for path in ['/icons/manifest.json','/icons/../app.js','/icons/not-an-item.png']:
            with self.assertRaises(urllib.error.HTTPError) as e:self.get(path)
            self.assertEqual(e.exception.code,404)
    def test_registration_control_requires_origin_and_token(self):
        payload=json.dumps({'action':'register_depots'}).encode()
        for headers in [{},{'Origin':self.base,'X-FILN-Token':'wrong'}]:
            request=urllib.request.Request(self.base+'/api/register-depots',data=payload,headers={'Content-Type':'application/json',**headers})
            with self.assertRaises(urllib.error.HTTPError) as e:urllib.request.urlopen(request)
            self.assertEqual(e.exception.code,403)
    def test_registration_queue_is_explicit_and_not_a_success_receipt(self):
        s=fixture();s['exported_at']=time.time();s['dispatcher']['registration_supported']=True
        s['stations'].append(dict(id='D1',guid='a'*32,is_depot=True,position=dict(x=0,y=0,z=0)))
        s['vehicles'][0].update(key='parked truck',route=[],job=None,position=dict(x=0,y=0,z=0))
        (self.path/'telemetry.json').write_text(json.dumps(s))
        status=json.loads(self.get('/api/status'))
        request=urllib.request.Request(self.base+'/api/register-depots',data=b'{"action":"register_depots"}',headers={'Content-Type':'application/json','Origin':self.base,'X-FILN-Token':status['csrf_token']})
        with urllib.request.urlopen(request) as r:
            self.assertEqual(r.status,202);result=json.load(r)
        self.assertEqual(result['status'],'queued');self.assertTrue((self.path/'depot-registration.tsv').exists())
        with self.assertRaises(urllib.error.HTTPError) as e:urllib.request.urlopen(request)
        self.assertEqual(e.exception.code,409)
    def test_named_loopback_host_is_allowed(self):
        request=urllib.request.Request(self.base+'/health',headers={'Host':f'filn.localhost:{self.server.server_port}'})
        with urllib.request.urlopen(request) as r:self.assertEqual(json.load(r)['service'],'filn-dashboard')
    def test_rebinding_and_cross_origin_not_allowed(self):
        request=urllib.request.Request(self.base+'/api/status',headers={'Host':'evil.example'})
        with self.assertRaises(urllib.error.HTTPError) as e:urllib.request.urlopen(request)
        self.assertEqual(e.exception.code,403)
        with urllib.request.urlopen(self.base+'/health') as r:self.assertIsNone(r.headers.get('Access-Control-Allow-Origin'))

if __name__=='__main__':unittest.main()

class ItemBoardTests(unittest.TestCase):
    def test_groups_keep_disabled_stocked_and_empty_roles(self):
        s=fixture();s['stations'][0]['powered']=False;s['stations'][2]['amount']=8
        d=analyze(s,100);g=d['item_groups'][0]
        self.assertEqual(len(g['providers']),2);self.assertEqual(len(g['requesters']),1)
        self.assertEqual(g['provided'],5000);self.assertEqual(g['requested'],0)
        self.assertEqual(g['providers'][0]['reason'],'Disabled')
    def test_unconfigured_requester_stock_is_never_used_as_filter(self):
        s=fixture();s['stations'][2].update(configured=False,amount=8)
        g=next(g for g in analyze(s,100)['item_groups'] if g['item'] is None)
        self.assertEqual(len(g['unconfigured']),1);self.assertFalse(g['requesters'])
    def test_retained_partial_cargo_explains_request_without_provider(self):
        s=fixture();s['dispatcher']['cargo_reuse_enabled']=True
        s['vehicles'][0]['cargo'].update(item='iron',amount=47)
        for p in s['stations'][:2]:p.update(amount=0,configured=False)
        d=analyze(s,100)
        self.assertEqual(d['fleet'][0]['status'],'Matching cargo / waiting for requester')
        self.assertEqual(d['requests'][0]['code'],'retained_cargo_available')
        self.assertEqual(d['requests'][0]['retained_vehicles'],['V1'])
        self.assertEqual(d['item_groups'][0]['retained_vehicles'][0]['amount'],47)
    def test_matching_cargo_still_obeys_fuel_and_player_opt_out(self):
        for field,value in [('autopilot',False),('driver',True),('has_fuel',False)]:
            s=fixture();s['dispatcher']['cargo_reuse_enabled']=True
            s['vehicles'][0]['cargo'].update(item='iron',amount=47);s['vehicles'][0][field]=value
            self.assertNotEqual(analyze(s,100)['requests'][0]['code'],'retained_cargo_available')
    def test_direct_delivery_assignment_is_visible_under_requester(self):
        s=fixture();s['vehicles'][0]['job']=dict(kind='cargo',state='Recovery',requester='requester',loaded=47,truckload=47)
        g=analyze(s,100)['item_groups'][0]
        self.assertEqual(g['requesters'][0]['vehicles'],['V1']);self.assertEqual(g['providers'][0]['vehicles'],[])
    def test_collected_provider_is_reusable_while_requester_remains_assigned(self):
        s=fixture();s['vehicles'][0]['job']=dict(state='Trip',provider='provider1',requester='requester',provider_released=True)
        g=analyze(s,100)['item_groups'][0]
        self.assertEqual(g['providers'][0]['vehicles'],[])
        self.assertEqual(g['providers'][0]['reason'],'Stock available')
        self.assertEqual(g['requesters'][0]['vehicles'],['V1'])
    def test_icon_id_is_local_and_cannot_be_a_path(self):
        s=fixture();item='/Game/FactoryGame/Resource/RawResources/OreIron/Desc_OreIron.Desc_OreIron_C'
        for station in s['stations']:station['item']=item
        import hashlib
        self.assertEqual(analyze(s,100)['item_groups'][0]['icon'],'/icons/unknown.svg')
        s.update(runtime='native',icon_cache_revision='a'*40)
        self.assertEqual(analyze(s,100)['item_groups'][0]['icon'],'/icons/'+'a'*40+'/'+hashlib.sha1(item.encode()).hexdigest()+'.png')
        s['stations'][0]['item']='../../not-an-icon'
        self.assertEqual(next(g for g in analyze(s,100)['item_groups'] if g['item']=='../../not-an-icon')['icon'],'/icons/unknown.svg')


class StoragePairTests(unittest.TestCase):
    def test_storage_only_supply_explains_warehouse_rule(self):
        s=fixture();s['dispatcher']['storage_tag_rule_enabled']=True
        for station in s['stations']:station['name']='[S] '+station['name']
        r=analyze(s,100)['requests'][0]
        self.assertEqual(r['code'],'storage_pair_blocked');self.assertEqual(r['combined_stock'],0)
    def test_ordinary_provider_can_fill_storage_request(self):
        s=fixture();s['dispatcher']['storage_tag_rule_enabled']=True
        s['stations'][0].update(name='[S] OUT',amount=4800)
        s['stations'][1].update(name='Factory OUT',amount=4800)
        s['stations'][2]['name']='[S] IN'
        r=analyze(s,100)['requests'][0]
        self.assertEqual(r['code'],'awaiting_dispatcher_check');self.assertEqual(r['combined_stock'],4800)
    def test_old_runtime_is_not_claimed_to_enforce_new_rule(self):
        s=fixture()
        for station in s['stations']:station['name']='[S] '+station['name']
        self.assertEqual(analyze(s,100)['requests'][0]['code'],'split_stock')

class NativeDashboardTests(unittest.TestCase):
    def test_native_never_queues_registration(self):
        with tempfile.TemporaryDirectory() as directory:
            d=Path(directory)
            snap=fixture();snap['runtime']='native';snap['exported_at']=time.time()
            snap['vehicles'][0]['held']=True
            (d/'telemetry.json').write_text(json.dumps(snap))
            server=server_module.make_server(d,0,native=True)
            thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
            url=f'http://127.0.0.1:{server.server_port}'
            try:
                with urllib.request.urlopen(url+'/api/status') as response: data=json.load(response)
                self.assertEqual(data['runtime'],'native')
                self.assertTrue(data['ready'])
                self.assertFalse(data['registration']['supported'])
                self.assertTrue((d/'dashboard-demand.txt').is_file())
                self.assertFalse((d/'depot-registration.tsv').exists())
                with self.assertRaises(urllib.error.HTTPError) as error:
                    urllib.request.urlopen(urllib.request.Request(url+'/api/register-depots',data=b'{}',method='POST'))
                self.assertEqual(error.exception.code,405)
            finally:
                server.shutdown();server.server_close();thread.join()
    def test_native_never_reports_cached_snapshot_ready_after_error(self):
        with tempfile.TemporaryDirectory() as directory:
            d=Path(directory)
            path=d/'telemetry.json'
            snap=fixture();snap.update(runtime='native',exported_at=time.time())
            path.write_text(json.dumps(snap))
            state=server_module.State(d,native=True)
            self.assertTrue(state.status()['ready'])
            for replacement in ('invalid JSON',json.dumps(dict(snap,exported_at=time.time()-60)),json.dumps(dict(snap,exported_at=time.time()+60))):
                path.write_text(replacement)
                result=state.status()
                self.assertFalse(result['ready'])
                self.assertIsNone(result['data'])
                self.assertTrue(result['error'])
                path.write_text(json.dumps(snap))
                self.assertTrue(state.status()['ready'])
            path.write_text(json.dumps(dict(snap,errors=[{'error':'unreadable inventory'}])))
            result=state.status()
            self.assertFalse(result['ready'])
            self.assertIn('sampling incomplete',result['error'])
    def test_native_rejects_lua_snapshot(self):
        with tempfile.TemporaryDirectory() as directory:
            d=Path(directory);(d/'telemetry.json').write_text(json.dumps(fixture()))
            result=server_module.State(d,native=True).status()
            self.assertFalse(result['ready']);self.assertIn('refusing Lua',result['error'])
    def test_native_relocating_and_unheld_not_available(self):
        s=fixture();s['runtime']='native';v=s['vehicles'][0];v['held']=False
        self.assertEqual(analyze(s,100)['counts']['available'],0)
        v['job']={'state':'Relocating'}
        self.assertEqual(analyze(s,100)['fleet'][0]['status'],'Relocating to free depot')

    def test_native_fluid_vehicle_not_active(self):
        s=fixture();s['runtime']='native';v=s['vehicles'][0];v.update(held=True,solid_supported=False)
        result=analyze(s,100)
        self.assertEqual(result['counts']['active'],0)
        self.assertEqual(result['fleet'][0]['status'],'Outside FILN / unsupported or legacy cargo medium')

    def test_native_fluid_capacity_and_matching_are_medium_specific(self):
        s=fixture();s['runtime']='native'
        for station in s['stations']:
            station.update(medium='fluid',quantity_unit='native fluid units',item='water')
        s['stations'][0]['amount']=4800
        v=s['vehicles'][0];v.update(held=True,medium='fluid',solid_supported=False)
        s['capacity_samples'][0].update(medium='fluid',by_item={'water':4800})
        r=analyze(s,100)
        self.assertEqual(r['counts']['available'],1)
        self.assertEqual(r['requests'][0]['truckload'],4800)
        self.assertEqual(r['requests'][0]['quantity_unit'],'native fluid units')
        s['stations'][0]['medium']='solid'
        self.assertEqual(analyze(s,100)['requests'][0]['code'],'provider_short')
        v['medium']='solid'
        self.assertEqual(analyze(s,100)['requests'][0]['code'],'unknown_capacity')
