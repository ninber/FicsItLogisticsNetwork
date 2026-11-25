"""Read-only explanation of observed FILN state; never controls dispatch."""
import re


def item_name(path):
    raw = (path or '').rsplit('.', 1)[-1]
    if raw == 'Desc_Stone_C':
        return 'Limestone'
    if raw == 'Desc_OreIron_C':
        return 'Iron Ore'
    if raw == 'Desc_OreCopper_C':
        return 'Copper Ore'
    return re.sub(r'(?<=[a-z])(?=[A-Z])', ' ', raw.removeprefix('Desc_').removesuffix('_C')) or 'Unconfigured'


def analyze(snapshot, now, history=None):
    history = history if history is not None else {}
    stations = snapshot.get('stations', [])
    vehicles = snapshot.get('vehicles', [])
    by = {s['guid']: s for s in stations}
    dispatch = snapshot.get('dispatcher', {})
    reservations = dispatch.get('reservations', {})
    errors = snapshot.get('errors', [])
    stale = now - snapshot['exported_at'] > 45
    world = snapshot.get('world')
    if history.get('world') != world or snapshot['exported_at'] < history.get('last_at', 0):
        history.clear()
        history['world'] = world
    previous = history.setdefault('previous', {})
    empty = history.setdefault('empty', {})
    fresh = history.get('last_at') != snapshot['exported_at']
    fleet = []
    for v in vehicles:
        row = dict(v)
        job = v.get('job') or {}
        reconciliation = dispatch.get('orphans', {}).get(v.get('key'), {})
        route = v.get('route', [])
        observed_home = route[0] if len(route)==4 and route[0]==route[-1] and by.get(route[0],{}).get('is_depot') else None
        observed_route = not job and observed_home is not None
        home = by.get(v.get('home') or observed_home, {})
        requester = by.get(job.get('requester') or (route[2] if observed_route else None), {})
        cargo = v.get('cargo', {})
        free = max(0,requester.get('capacity',0)-requester.get('amount',0)) if requester.get('item') and requester.get('item')==cargo.get('item') else None
        fuel = v.get('fuel', {})
        idle = not job or job.get('state') == 'Idle'
        if not v.get('managed'):
            if observed_home and not v.get('autopilot'):
                status = 'Outside dispatcher / autopilot off'
            elif observed_home and v.get('driver'):
                status = 'Untracked FILN route / player driving'
            elif observed_home:
                status = 'Untracked FILN route' + (' / '+reconciliation['status'] if reconciliation.get('status') else '')
            elif any(by.get(g,{}).get('is_depot') for g in route):
                status = 'Untracked FILN route / incomplete schedule'
            else:
                status = 'Outside FILN / no depot route'
        elif not v.get('autopilot') or job.get('state') == 'Detached':
            status = 'Autopilot off / opted out'
        elif v.get('driver'):
            status = 'Player driving'
        elif not v.get('home_powered'):
            status = 'Home disabled'
        elif job.get('kind')=='cargo' and job.get('state')=='Recovery':
            status = 'Retained cargo: '+str(job.get('recovery') or 'Assess')
        elif job.get('state') == 'Recovery' or (not idle and job.get('recovery') not in (None, 'HomeReady')):
            status = 'Recovery: ' + str(job.get('recovery') or job.get('error') or job.get('state'))
        elif idle and cargo.get('amount',0)>0 and not (dispatch.get('cargo_reuse_enabled') and cargo.get('item') and cargo.get('kinds',1)==1):
            status = 'Blocked: cargo remains'
        elif job.get('state') == 'Relocating':
            status = 'Relocating to free depot'
        elif snapshot.get('runtime') == 'native' and idle and not v.get('held'):
            status = 'Returning to depot / awaiting parking'
        elif not v.get('has_fuel'):
            status = 'Out of fuel'
        elif idle and (not fuel.get('capacity') or fuel.get('amount', 0) < fuel['capacity']):
            status = 'Waiting for depot refuel'
        elif idle and cargo.get('amount',0)>0:
            status = 'Matching cargo / waiting for requester'
        elif idle:
            status = 'Available / waiting for dispatch'
        elif job.get('kind') == 'cargo':
            status = 'Delivering retained cargo / returning home'
        elif job.get('state') not in ('Active', 'Prepared', 'Trip'):
            status = job.get('state', 'Working')
        elif job.get('visited', 0) >= 2:
            status = 'Returning to depot'
        elif job.get('visited', 0) >= 1:
            status = 'Unloading' if v.get('docking') and job.get('loaded', 0) > v.get('cargo', {}).get('amount', 0) else 'Loading / going to requester'
        else:
            status = 'Going to provider / loading'
        row.update(status=status, home_name=home.get('name', v.get('home', '—')),
                   provider_name=by.get(job.get('provider') or (route[1] if observed_route else None), {}).get('name', '—'),
                   requester_name=requester.get('name', '—'), observed_route=observed_route,
                   requester_free_space=free,unload_space_shortfall=max(0,cargo.get('amount',0)-free) if free is not None else None,
                   reconciliation=reconciliation,
                   dispatcher_reason=(job.get('recovery_details') or {}).get('last_error') or job.get('error') or reconciliation.get('reason') or v.get('native_pending_reason') or dispatch.get('reasons', {}).get(v.get('home'), ''))
        fleet.append(row)
    active = [v for v in fleet if v.get('managed') and v.get('home_powered') and v.get('autopilot') and not v.get('driver') and (v.get('job')or{}).get('state')!='Detached']
    available = [v for v in active if v['status'] == 'Available / waiting for dispatch']
    working = [v for v in active if v.get('job') and v['job'].get('state') not in ('Idle', 'Detached')]
    inventory = []
    for s in stations:
        row = dict(s)
        stamp = s.get('sampled_at', snapshot['exported_at'])
        old = previous.get(s['guid'])
        rate = None
        transferring = s.get('occupied') or s['guid'] in reservations
        if old and s.get('item') == old.get('item') and 0 < stamp-old['at'] <= 60 and not transferring and not old.get('transferring'):
            rate = (s.get('amount', 0)-old['amount'])*60/(stamp-old['at'])
        elif old and stamp == old['at']:
            rate = old.get('rate')
        row['net_items_per_minute'] = rate
        row['item_name'] = item_name(s.get('item'))
        row['sample_age'] = max(0, now-stamp)
        if fresh:
            previous[s['guid']] = dict(at=stamp, item=s.get('item'), amount=s.get('amount', 0), transferring=transferring, rate=rate)
        if s.get('amount', 0) == 0 and s.get('configured') and s.get('role') == 'requester' and not s.get('is_depot'):
            empty.setdefault(s['guid'], stamp)
        else:
            empty.pop(s['guid'], None)
        inventory.append(row)
    providers = [s for s in inventory if s.get('role') == 'provider' and not s.get('is_depot') and s.get('configured') and len(s.get('contents', [])) <= 1 and s.get('powered') and not s.get('standby')]
    requests = []
    capacities = snapshot.get('capacity_samples', [])
    for s in inventory:
        if s.get('role') != 'requester' or s.get('is_depot') or not s.get('configured') or not s.get('item'):
            continue
        eligible_types = {(v.get('class'), v.get('cargo', {}).get('slots')) for v in active}
        needs = [t['by_item'][s['item']] for t in capacities if (t.get('class'), t.get('slots')) in eligible_types and t.get('by_item', {}).get(s['item'], 0)>0]
        need = min(needs) if needs else None
        matching = [p for p in providers if p.get('item') == s['item']]
        storage_excluded = [p for p in matching if dispatch.get('storage_tag_rule_enabled') and '[S]' in p.get('name','').upper() and '[S]' in s.get('name','').upper()]
        matching = [p for p in matching if p not in storage_excluded]
        usable = [p for p in matching if not p.get('occupied') and p['guid'] not in reservations]
        best = max(usable, key=lambda p: p.get('amount', 0), default=None)
        stock = best.get('amount', 0) if best else 0
        total = sum(p.get('amount', 0) for p in usable)
        full = [p for p in usable if need and p.get('amount', 0) >= need]
        jobs = [v for v in working if (v.get('job') or {}).get('requester') == s['guid']]
        checks = []
        for home, evidence in dispatch.get('request_checks', {}).items():
            check = evidence.get(s['guid'])
            if check and 0 <= now-check.get('at', 0) <= 60:
                checks.append(dict(check, home=home, home_name=by.get(home, {}).get('name', home), home_id=by.get(home, {}).get('id', ''), age=now-check['at']))
        checked = {c['home']: c for c in checks if c['age'] <= 30 and c.get('item') == s['item']}
        failed_homes = [v for v in available if checked.get(v.get('home'), {}).get('reason') in ('different_network', 'unreachable_legs', 'native_path_rejected')]
        priority = 0
        if s.get('amount', 0)>0:
            reason, code = 'Stock present; request inactive until station is empty', 'stock_present'
        elif not s.get('powered') or s.get('standby'):
            reason, code, priority = 'Requester disabled', 'disabled', 40
        elif jobs or s['guid'] in reservations:
            reason, code, priority = 'Delivery assigned / endpoint reserved', 'delivery_in_progress', 20
        elif stale or errors:
            reason, code, priority = 'Telemetry stale or incomplete; check sampling errors', 'incomplete', 90
        elif not need:
            reason, code, priority = 'No active vehicle capacity sample; truckload unknown', 'unknown_capacity', 80
        elif s.get('capacity', 0)<need:
            reason, code, priority = 'Requester cannot fit smallest sampled truckload', 'requester_too_small', 95
        elif not matching and storage_excluded:
            reason, code, priority = '[S] storage-to-storage deliveries are blocked', 'storage_pair_blocked', 50
        elif not matching:
            reason, code, priority = 'No stocked matching LOAD provider observed (empty unfiltered providers have unknown item)', 'no_observed_provider', 100
        elif not usable:
            reason, code, priority = 'Matching providers occupied or reserved', 'providers_busy', 35
        elif not full:
            reason, code, priority = ('Stock split across providers; one provider must hold a full truckload', 'split_stock', 85) if total>=need else ('Waiting for one provider to produce a full truckload', 'provider_short', 85)
        elif not dispatch.get('enabled'):
            reason, code, priority = 'Automatic dispatch paused', 'dispatcher_paused', 100
        elif not available:
            reason, code, priority = 'No available truck; inspect fleet cargo, fuel, active jobs and recovery', 'no_available_truck', 70
        elif available and len(failed_homes) == len(available):
            reason, code, priority = f'Recent route checks failed for all {len(available)} currently available trucks; inspect depot evidence', 'route_checks_failed', 95
        else:
            reason, code, priority = 'Stock available; awaiting dispatcher route / vehicle selection', 'awaiting_dispatcher_check', 60
        if code=='route_checks_failed':
            failures = {(f.get('provider'),f.get('leg'),f.get('reason')) for c in checks for f in c.get('failures',[])}
            details=[]
            for provider,leg,why in sorted(failures,key=str):
                station=by.get(provider,{})
                name=(station.get('id','')+' '+station.get('name',provider or '?')).strip()
                details.append(name+': '+str(leg).replace('_',' ')+' ('+str(why).replace('_',' ')+')')
            if details:reason+=' · '+'; '.join(details[:3])
        eta = None
        if best and need and need>stock and not stale:
            rate = best.get('net_items_per_minute')
            if rate and rate>0:
                eta = (need-stock)/rate*60
        retained = [v for v in active if (not dispatch.get('storage_tag_rule_enabled') or '[S]' not in s.get('name','').upper()) and v['status']=='Matching cargo / waiting for requester' and v.get('cargo',{}).get('item')==s['item'] and 0<v['cargo']['amount']<=s.get('capacity',0)]
        if retained and s.get('amount',0)==0 and s.get('powered') and not s.get('standby') and not jobs and s['guid'] not in reservations and not stale and not errors:
            reason,code,priority = ('Matching retained cargo available; awaiting route check','retained_cargo_available',55) if dispatch.get('enabled') else ('Automatic dispatch paused','dispatcher_paused',100)
        requests.append(dict(retained_vehicles=[v['id'] for v in retained],guid=s['guid'],id=s['id'],name=s['name'],item=s['item'],item_name=s['item_name'],inventory=s.get('amount',0),
                             code=code,reason=reason,priority=priority,route_checks=checks,truckload=need,truckload_max=max(needs) if needs else None,
                             best_stock=stock,best_provider=best['name'] if best else '—',missing=max(0,need-stock) if need else None,
                             combined_stock=total,full_providers=len(full),providers=matching,vehicles=[v['id'] for v in jobs],
                             observed_empty_seconds=max(0,now-empty[s['guid']]) if s['guid'] in empty else None,
                             batch_fill_seconds=eta,position=s.get('position'),sample_age=s['sample_age']))
    requests.sort(key=lambda r: (-r['priority'], -(r['observed_empty_seconds'] or 0), r['name']))
    if fresh:
        history['last_at'] = snapshot['exported_at']
    return dict(item_groups=group_items(inventory,requests,fleet,reservations),requests=requests,fleet=fleet,inventories=inventory,errors=errors,stale=stale,
                exported_at=snapshot['exported_at'],collection_seconds=snapshot.get('collection_seconds'),sequence=snapshot.get('sequence'),
                dispatcher=dispatch,counts=dict(stations=len(stations),vehicles=len(vehicles),active=len(active),available=len(available),working=len(working),untracked_routes=sum(v['status'].startswith('Untracked FILN route') for v in fleet),cargo_blocked=sum(v['status']=='Blocked: cargo remains' for v in fleet),
                retained_ready=sum(v['status']=='Matching cargo / waiting for requester' for v in fleet),empty_requests=sum(r['inventory']==0 for r in requests),blocked=sum(r['priority']>=60 for r in requests)))


def group_items(inventory, requests, fleet, reservations):
    """Group observed roles without inferring an UNLOAD station's filter from stock."""
    groups = {}
    request_by = {r['guid']: r for r in requests}
    for s in inventory:
        if s.get('is_depot'):
            continue
        known = s.get('configured') and s.get('item') and len(s.get('contents', [])) <= 1
        key = s['item'] if known else ''
        g = groups.setdefault(key, dict(item=key or None, item_name=item_name(key), providers=[], requesters=[], unconfigured=[], retained_vehicles=[]))
        row = dict(s)
        assigned = [v['id'] for v in fleet if s['guid'] in ((v.get('job') or {}).get('provider') if not (v.get('job') or {}).get('provider_released') else None, (v.get('job') or {}).get('requester')) and (v.get('job') or {}).get('state') not in ('Idle','Detached')]
        row.update(vehicles=assigned, reservation=reservations.get(s['guid']))
        if not known:
            row['reason'] = 'Explicit requester filter or single-item provider identity missing'
            g['unconfigured'].append(row)
        elif s.get('role') == 'requester':
            r = request_by.get(s['guid'], {})
            row.update(reason=r.get('reason', 'Unconfigured'), code=r.get('code'), empty_seconds=r.get('observed_empty_seconds'), truckload=r.get('truckload'))
            g['requesters'].append(row)
        elif s.get('role') == 'provider':
            row['reason'] = ('Disabled' if not s.get('powered') or s.get('standby') else
                             'Vehicle assigned' if assigned else 'Reserved' if row['reservation'] else
                             'Occupied' if s.get('occupied') else 'Empty' if not s.get('amount') else 'Stock available')
            g['providers'].append(row)
    for g in groups.values():
        for role in ('providers','requesters','unconfigured'):
            g[role].sort(key=lambda r: (r.get('name','').casefold(),r['guid']))
        g['provided'] = sum(s.get('amount',0) for s in g['providers'])
        g['requested'] = sum(s.get('amount',0)==0 for s in g['requesters'])
        g['retained_vehicles'] = [dict(id=v['id'],amount=v['cargo']['amount'],status=v['status']) for v in fleet if g['item'] and v.get('cargo',{}).get('item')==g['item'] and v.get('cargo',{}).get('amount',0)>0]
        raw = (g['item'] or '').rsplit('.',1)[-1].lower().replace('_','-')
        g['icon'] = '/icons/'+raw+'_64.png' if re.fullmatch(r'desc-[a-z0-9-]+-c',raw) else '/icons/unknown.svg'
    return sorted(groups.values(),key=lambda g:(g['item'] is None,g['item_name'].casefold()))
