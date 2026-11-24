"""Explicit, conservative registration planning from telemetry; runtime rechecks every pair."""
import math
import time
import secrets


def plan(snapshot, now=None):
    now = time.time() if now is None else now
    if now-snapshot['exported_at']>45 or any(not isinstance(e,dict) or e.get('kind')!='station' for e in snapshot.get('errors',[])):
        raise ValueError('Fresh telemetry with complete vehicle samples is required')
    vehicles=snapshot['vehicles']
    claimed={g for v in vehicles for g in v.get('route',[])}
    depots=[s for s in snapshot['stations'] if s.get('is_depot') and s['guid'] not in claimed]
    proposals=[]
    for v in vehicles:
        if v.get('route') or v.get('driver') or v.get('job') or v.get('cargo',{}).get('amount',0):continue
        p=v.get('position');near=[]
        if not p:continue
        for s in depots:
            q=s.get('position')
            if q and abs(p['z']-q['z'])<=2 and math.dist([p[k] for k in ('x','y','z')],[q[k] for k in ('x','y','z')])<=8:
                near.append(s)
        if len(near)==1:proposals.append(dict(vehicle=v['key'],vehicle_id=v['id'],home=near[0]['guid'],station_id=near[0]['id']))
    counts={p['home']:sum(q['home']==p['home'] for q in proposals) for p in proposals}
    return [p for p in proposals if counts[p['home']]==1]


def queue(scripts,snapshot):
    pairs=plan(snapshot)
    if not pairs:raise ValueError('No unambiguous empty parked trucks found')
    pending=scripts/'depot-registration.tsv';result=scripts/'depot-registration-result.json'
    if pending.exists():
        import json
        try:
            receipt=json.loads(result.read_text());finished=receipt.get('request_id') if receipt.get('status')=='complete' else None
        except (OSError,ValueError):finished=None
        if pending.read_text().splitlines()[0]!=finished:raise ValueError('A registration request is already pending')
    request=secrets.token_hex(16)
    lines=[request]
    for p in pairs:
        if any(c in p['vehicle'] for c in '\r\n\t') or len(p['home'])!=32:raise ValueError('Invalid object identity')
        lines.append(p['home']+'\t'+p['vehicle'])
    temp=scripts/'depot-registration.tsv.tmp';temp.write_text('\n'.join(lines)+'\n');temp.replace(pending)
    return dict(request_id=request,status='queued',pairs=pairs)
