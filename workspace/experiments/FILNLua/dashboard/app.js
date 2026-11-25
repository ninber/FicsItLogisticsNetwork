'use strict';
let current=null, tab='items', sortKey=null, descending=false;
const $=id=>document.getElementById(id);
const number=n=>n===null||n===undefined?'—':Math.round(n).toLocaleString();
const duration=n=>n===null||n===undefined?'—':n<60?`${Math.round(n)}s`:`${Math.floor(n/60)}m ${Math.round(n%60)}s`;
const item=p=>(p||'').split('.').pop().replace(/^Desc_/,'').replace(/_C$/,'').replace(/([a-z])([A-Z])/g,'$1 $2')||'Unknown item';
function node(tag,text,cls){const n=document.createElement(tag);if(text!==undefined)n.textContent=text;if(cls)n.className=cls;return n;}
function detail(cell,text){cell.append(node('small',text));}
function coords(p){return p?`x ${p.x}, y ${p.y}, z ${p.z} m`:'';}
function cargo(inv){return (inv?.items||[]).map(v=>`${number(v.amount)} ${item(v.item)}`).join(' · ')||'Empty';}
function named(row){const c=node('td',row.name||row.id,'name');detail(c,`${row.id} · ${coords(row.position)}`);return c;}
function num(text,sub){const c=node('td',text,'numeric');if(sub)detail(c,sub);return c;}
const columns={requests:[['name','Requester / item'],['reason','Why it is waiting'],['best_stock','Best provider / truckload'],['missing','Missing at best provider'],['combined_stock','Combined stock'],['observed_empty_seconds','Observed empty'],['vehicles','Assigned vehicles']],fleet:[['id','Vehicle'],['status','State'],['home_name','Route / job'],['cargo','Cargo'],['fuel','Fuel'],['dispatcher_reason','Dispatcher / recovery']],inventories:[['name','Station'],['role','Role / state'],['amount','Cargo inventory'],['capacity','Item capacity'],['net_items_per_minute','Net stock / min'],['fuel','Dedicated fuel inventory'],['sample_age','Sample age']]};
function render(){if(!current)return;const data=current;
 $('overview').replaceChildren();for(const [label,value]of [['Empty requests',data.counts.empty_requests],['Potential blockers',data.counts.blocked],['Active trucks',data.counts.active],['On a job',data.counts.working],['Available',data.counts.available],['Matching cargo ready',data.counts.retained_ready],['Cargo blocked',data.counts.cargo_blocked],['Untracked FILN',data.counts.untracked_routes],['Stations',data.counts.stations]]){const m=node('div',undefined,'metric');m.append(node('strong',number(value)),node('span',label));$('overview').append(m);}
 const q=$('search').value.trim().toLowerCase();
 if(tab==='items'){renderItems(data,q);$('empty').parentElement.hidden=true;$('debug').textContent=JSON.stringify({dispatcher:data.dispatcher,sampling_errors:data.errors},null,2);return;}
 let rows=data[tab].filter(r=>JSON.stringify(r).toLowerCase().includes(q));
 if(tab==='requests'&&$('empty').checked)rows=rows.filter(r=>r.inventory===0);
 if(sortKey)rows.sort((a,b)=>{let x=a[sortKey],y=b[sortKey];if(x===null||x===undefined)return 1;if(y===null||y===undefined)return -1;const result=typeof x==='number'&&typeof y==='number'?x-y:String(x).localeCompare(String(y));return descending?-result:result;});
 $('rows').textContent=`${rows.length} shown`;$('empty').parentElement.hidden=tab!=='requests';
 const wrap=node('div',undefined,'table-wrap'),table=node('table'),head=node('thead'),hr=node('tr');
 for(const [key,label]of columns[tab]){const th=node('th'),b=node('button',label+(sortKey===key?(descending?' ▼':' ▲'):''));b.addEventListener('click',()=>{descending=sortKey===key?!descending:false;sortKey=key;render();});th.append(b);hr.append(th);}head.append(hr);table.append(head);const body=node('tbody');
 for(const r of rows){const tr=node('tr');
 if(tab==='requests'){
  const name=named(r);detail(name,r.item_name);detail(name,`Requester inventory: ${number(r.inventory)}`);
  const why=node('td',undefined,'reason');why.append(node('span',r.reason,`badge ${r.priority>=60?'bad':r.code==='delivery_in_progress'?'good':''}`));
  if(r.providers.length){const d=node('details'),s=node('summary',`${r.providers.length} matching providers`);d.append(s);for(const p of r.providers){d.append(node('p',`${p.id} ${p.name}: ${number(p.amount)}${p.occupied?' · occupied':''} · ${coords(p.position)}`));}why.append(d);}
  if(r.route_checks.length){const d=node('details');d.append(node('summary',`Recent dispatcher checks (${r.route_checks.length} depots)`));for(const c of r.route_checks){d.append(node('p',`${c.home_id} ${c.home_name}: ${c.reason.replaceAll('_',' ')} · ${duration(c.age)} ago${c.full!==undefined?' · full providers '+c.full:''}${c.provider?' · provider '+(current.inventories.find(s=>s.guid===c.provider)?.name||c.provider):''}`));for(const failure of c.failures||[]){const provider=current.inventories.find(s=>s.guid===failure.provider);d.append(node('p',`${provider?provider.id+' '+provider.name:failure.provider}: ${failure.leg.replaceAll('_',' ')} · ${failure.reason.replaceAll('_',' ')}`));}}why.append(d);}
  const supply=num(`${number(r.best_stock)} / ${number(r.truckload)}`,r.best_provider);
  if(r.truckload){const p=node('progress');p.max=r.truckload;p.value=r.best_stock;supply.append(p);if(r.truckload_max!==r.truckload)detail(supply,`Vehicle capacities ${number(r.truckload)}–${number(r.truckload_max)}`);}
  tr.append(name,why,supply,num(number(r.missing),r.batch_fill_seconds?`Batch-fill estimate ~${duration(r.batch_fill_seconds)}`:'Batch-fill estimate unavailable'),num(number(r.combined_stock),`${r.full_providers} full / unreserved`),num(duration(r.observed_empty_seconds),`Sample ${duration(r.sample_age)} old`),num(r.vehicles.join(', ')||'—'));
 }else if(tab==='fleet'){
  const name=named(r);detail(name,r.class?.split('/').pop()||'');
  const status=node('td',r.status);detail(status,`AP ${r.autopilot?'on':'off'} · native error ${r.native_error}`);
  const route=node('td',(r.observed_route||(r.job&&r.job.state!=='Idle'))?`${r.job?.kind==='cargo'?'Retained cargo':r.provider_name} → ${r.requester_name} → ${r.home_name}`:r.home_name,'name');
  if(r.job){detail(route,`Loaded ${number(r.job.loaded)} · unloaded ${number(r.job.unloaded)} / ${number(r.job.truckload)}`);if(r.job.started)detail(route,`Job age ${duration(Date.now()/1000-r.job.started)}`);}
  if(r.job?.recovery_details){const d=r.job.recovery_details;detail(route,`Recovery attempts: ${number(d.attempts)} · retry in ${duration(Math.max(0,(d.next_retry||0)-Date.now()/1000))}`);}
  if(r.reconciliation?.next_retry)detail(route,`Reconciliation retry in ${duration(Math.max(0,r.reconciliation.next_retry-Date.now()/1000))}`);
  if(r.observed_route)detail(route,'Observed native route; no dispatcher assignment');
  const load=num(cargo(r.cargo));
  if(r.requester_free_space!==null&&r.requester_free_space!==undefined)detail(load,`Requester free: ${number(r.requester_free_space)} · cannot unload now: ${number(r.unload_space_shortfall)}`);
  const fuel=num(`${number(r.fuel.amount)} / ${number(r.fuel.capacity)}`,cargo(r.fuel));
  tr.append(name,status,route,load,fuel,node('td',[...new Set([r.job?.error,r.dispatcher_reason].filter(Boolean))].join(' · ')||'—','reason'));
 }else{
  const name=named(r);detail(name,r.item_name);
  const state=node('td',r.is_depot?'Rest depot':r.role==='provider'?'LOAD provider':'UNLOAD requester');detail(state,`${r.powered&&!r.standby?'Powered':'Disabled'}${r.occupied?' · occupied':''}${!r.configured?' · unconfigured':''}`);
  tr.append(name,state,num(cargo({items:r.contents})),num(number(r.capacity),r.item_name),num(r.net_items_per_minute===null?'—':`${r.net_items_per_minute>=0?'+':''}${number(r.net_items_per_minute)}`,'Observed net change'),num(cargo(r.fuel)),num(duration(r.sample_age)));
 }
 body.append(tr);}
 table.append(body);wrap.append(table);$('content').replaceChildren(rows.length?wrap:node('div','No matching rows.','empty'));
 $('debug').textContent=JSON.stringify({dispatcher:data.dispatcher,sampling_errors:data.errors},null,2);
}
for(const b of document.querySelectorAll('[data-tab]'))b.addEventListener('click',()=>{tab=b.dataset.tab;sortKey=null;for(const other of document.querySelectorAll('[data-tab]'))other.classList.toggle('selected',other===b);render();});
$('search').addEventListener('input',render);$('empty').addEventListener('change',render);
async function poll(){try{const r=await fetch('/api/status',{cache:'no-store'});if(!r.ok)throw Error(`Server HTTP ${r.status}`);const result=await r.json();showRegistration(result);const d=result.data;
 if(d){const changed=!current||current.sequence!==d.sequence||current.exported_at!==d.exported_at;current=d;const age=Math.max(0,result.server_time-d.exported_at);$('freshness').textContent=`Snapshot ${duration(age)} old · scan ${duration(d.collection_seconds)}`;
 const pipeline=d.dispatcher.pipeline;const scheduling=pipeline?` · Dispatch: ${pipeline.ranking?`ranking ${pipeline.ranked}/${pipeline.total_homes}`:'between rounds'}${pipeline.sampling?` · inventory refresh ${pipeline.sampled}/${pipeline.total_stations}`:''}${pipeline.catalog_age!==undefined?` · catalog ${duration(pipeline.catalog_age)} old`:''}`:'';
 const progress=result.progress;const collecting=progress&&progress.started_at>d.exported_at-1&&result.server_time-progress.exported_at<10;
 $('notice').textContent=d.stale?'Telemetry is stale. Is the save loaded and the updated FILN mod running?':d.errors.length?`${d.errors.length} objects could not be sampled. Bottleneck conclusions are incomplete; expand sampling errors below.`:`${d.dispatcher.enabled?'Automatic dispatch enabled':'Automatic dispatch PAUSED'} · ${collecting?`Collecting next snapshot ${progress.completed}/${progress.total}`:'Read-only monitor active'} · full fresh pickups / retained cargo first · empty requesters${scheduling}`;
 $('notice').classList.toggle('warning',d.stale||d.errors.length>0||!d.dispatcher.enabled);if(changed)render();
 }else{$('notice').textContent=result.progress?`Collecting first snapshot: ${result.progress.completed}/${result.progress.total}. Keep the save loaded.`:'Waiting for telemetry. Load a save with FILN running; the first scan takes several seconds.';}
 }catch(e){$('notice').textContent=`Dashboard disconnected: ${e.message}. Start scripts/start-filn-dashboard.sh.`;$('notice').classList.add('warning');}finally{setTimeout(poll,2000);}}
poll();

function renderItems(data,q){
 const board=node('div',undefined,'item-board');let shown=0;
 for(const g of data.item_groups||[]){
  if(q&&!JSON.stringify(g).toLowerCase().includes(q))continue;
  shown++;
  const group=node('article',undefined,'item-group');
  for(const [role,label] of [['providers','Providers · LOAD'],['requesters','Requesters · UNLOAD']]){
   const panel=node('section',undefined,`item-panel ${role}`),heading=node('div',undefined,'item-heading');
   const icon=node('div',undefined,'item-icon'),img=node('img');img.src=g.icon;img.alt=g.item_name;img.width=48;img.height=48;
   img.addEventListener('error',()=>{if(!img.src.endsWith('/unknown.svg'))img.src='/icons/unknown.svg';});icon.append(img);
   const title=node('div');title.append(node('h2',g.item_name),node('small',label));heading.append(icon,title);panel.append(heading);
   panel.append(node('div',role==='providers'?`${number(g.provided)} items · ${g.providers.length} stations`:`${g.requested} empty / ${g.requesters.length} stations`, 'item-total'));
   const table=node('table'),head=node('thead'),tr=node('tr');for(const h of ['Station','Stock / capacity','State / vehicles'])tr.append(node('th',h));head.append(tr);table.append(head);
   const body=node('tbody');for(const s of g[role]){
    const row=node('tr'),name=named(s),stock=num(`${number(s.amount)} / ${number(s.capacity)}`);
    const p=node('progress');p.max=s.capacity||1;p.value=s.amount||0;stock.append(p);
    if(s.net_items_per_minute!==null)detail(stock,`${s.net_items_per_minute>=0?'+':''}${number(s.net_items_per_minute)} / min net`);
    const state=node('td',s.reason,'station-state');
    if(s.vehicles.length)detail(state,`Assigned: ${s.vehicles.join(', ')}`);
    if(s.reservation&&!s.vehicles.length)detail(state,`Reserved by ${data.inventories.find(x=>x.guid===s.reservation)?.name||s.reservation}`);
    if(s.empty_seconds!==null&&s.empty_seconds!==undefined)detail(state,`Empty ${duration(s.empty_seconds)}`);
    if(s.road)detail(state,`Road: ${s.road.incoming} in / ${s.road.outgoing} out`);
    detail(state,`Sample ${duration(s.sample_age)} old${s.occupied?' · occupied':''}${!s.powered||s.standby?' · disabled':''}`);
    row.append(name,stock,state);body.append(row);
   }table.append(body);panel.append(g[role].length?table:node('p','No observed stations for this role.','no-stations'));group.append(panel);
  }
  if(g.retained_vehicles.length)group.append(node('div',`Cargo on trucks: ${g.retained_vehicles.map(v=>`${v.id}: ${number(v.amount)} (${v.status})`).join(' · ')}`,'retained-summary'));
  if(g.unconfigured.length){const unknown=node('div',undefined,'unconfigured-stations');unknown.append(node('h3','Stations requiring configuration'));for(const s of g.unconfigured)unknown.append(node('p',`${s.id} · ${s.name} · ${cargo({items:s.contents})} · ${s.reason}`));group.append(unknown);}
  board.append(group);
 }
 $('rows').textContent=`${shown} item groups`;$('content').replaceChildren(shown?board:node('div','No matching item groups.','empty'));
}

let registrationToken=null;
$('register-depots').addEventListener('click',async()=>{
 const button=$('register-depots');button.disabled=true;
 try{
  const r=await fetch('/api/register-depots',{method:'POST',headers:{'Content-Type':'application/json','X-FILN-Token':registrationToken},body:JSON.stringify({action:'register_depots'})});
  if(!r.ok)throw Error(await r.text());const result=await r.json();
  $('registration-status').textContent=`Queued ${result.pairs.length} truck/depot pairs. Awaiting game confirmation…`;
 }catch(e){$('registration-status').textContent=`Registration failed: ${e.message}`;}
});
function showRegistration(result){
 registrationToken=result.csrf_token;const reg=result.registration;if(!reg)return;
 if(result.runtime==='native') { $('registration').hidden=true; $('register-depots').disabled=true; return; }
 $('registration').hidden=false;
 const pending=reg.result&&reg.result.status!=='complete';
 $('register-depots').disabled=!reg.supported||pending||!reg.candidates.length;
 $('register-depots').textContent=`Register parked trucks (${reg.candidates.length})`;
 $('registration-status').textContent=!reg.supported?'Registration needs the updated mod loaded.':pending?'Registration queued/running; waiting for confirmation.':reg.result?`${reg.result.results.filter(r=>r.ok).length} registered/already assigned; ${reg.result.results.filter(r=>!r.ok).length} skipped or failed.`:'Assigns one home stop and enables autopilot for empty, unoccupied trucks within 8 m of an unclaimed depot.';
 $('registration-results').textContent=reg.result?JSON.stringify(reg.result,null,2):reg.candidates.map(p=>`${p.vehicle_id} → ${p.station_id}`).join('\n');
}
