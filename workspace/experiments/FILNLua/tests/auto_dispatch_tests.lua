local root=assert(arg[1]);package.path=root..'/?.lua;'..package.path
local Core=require('Core');local n=0;local function check(v)assert(v);n=n+1 end
local function obj(name)return {IsValid=function()return true end,GetFullName=function()return name end}end
local function guid(i)return {A=i,B=0,C=0,D=0}end
local recipe,resource=obj('rest'),obj('item');local homes={};local vehicles={};local powered={true,true};local standby={false,false}
for i=1,2 do local index=i;local b=obj('bay');b.mBuiltWithRecipe=recipe;b.HasPower=function()return powered[index]end;b.IsProductionPaused=function()return standby[index]end
 homes[i]=obj('station'..i);homes[i].GetStation=function()return b end;homes[i].GetPathNodeGUID=function()return guid(index)end
 vehicles[i]=obj('vehicle'..i);vehicles[i].GetVehicleRoute=function()return {guid(index)}end
end
local selections,samples,refreshes,starts,clock=0,0,0,0,0;local fail=false;local runtime={jobs={}}
local old=package.loaded.CargoDispatch or require('CargoDispatch')
local selector={better=old.better,sample=function()samples=samples+1;return {}end,add=function()end,select=function(_,home,catalog)selections=selections+1;assert(catalog);if fail then error('no supply')end;return {home=home,provider='p',requester='r',item='item',truckload=4800,distance=home==Core.guid_key(guid(2)) and 20 or 100}end}
package.loaded.CargoDispatch=selector
function runtime:begin(home)starts=starts+1;local job={state='Trip'};self.jobs[home]=job;return job end
package.loaded.AutoDispatch=nil;local Auto=require('AutoDispatch');package.loaded.CargoDispatch=old
local api={now=function()return clock end,refresh=function()refreshes=refreshes+1 end,stations=function()return homes end,vehicles=function()return vehicles end,find=function(path)return path=='item'and resource or recipe end,valid=function(o)return o and o.IsValid and o:IsValid()end,call=function(o,m,...)return o[m](o,...)end,runtime=function()return runtime end,log=function()end}
local s=Auto.new(api);s:step();check(refreshes==0);s.enabled=true
for i=1,2 do local before=samples;s:step();check(samples-before==1);check(starts==0)end
s:step();check(starts==0 and selections==1)
s:step();check(starts==0 and selections==2)
s:step();check(starts==1 and selections==3);check(runtime.jobs[Core.guid_key(guid(2))].state=='Trip');check(not runtime.jobs[Core.guid_key(guid(1))]);check(refreshes==1)
fail=true;clock=6;s:step();s:step();s:step();check(starts==1);check(selections==4)
for i=1,20 do s:step()end;check(selections==4)
runtime.jobs[Core.guid_key(guid(1))]={state='Trip'}
local before=samples;for i=1,20 do clock=clock+.25;s:step()end;check(samples==before);check(refreshes==1)
runtime.jobs[Core.guid_key(guid(1))].state='Recovery';s:step();check(samples==before)
-- Registry excludes both unpowered and standby homes; busy jobs still get runtime observation.
powered[1]=false;clock=40;s:step();check(#s.homes==1 and s.homes[1]==Core.guid_key(guid(2)))
powered[1]=true;standby[1]=true;clock=80;s:step();check(#s.homes==1)
s.enabled=false;local prior_refreshes=refreshes;clock=120;s:step();check(refreshes==prior_refreshes)
-- Pausing cargo dispatch keeps one bounded stationary fuel candidate per pass.
local fuel_calls=0;runtime.jobs={};standby[1]=false
runtime.fuel_service={ready=function()fuel_calls=fuel_calls+1;return true end}
local paused=Auto.new(api);local old_samples=samples;local old_starts=starts
paused:step();paused:step();check(fuel_calls==2);check(samples==old_samples and starts==old_starts)
-- A deferred journal entry must not freeze unrelated dispatch selection.
runtime.fuel_service=nil;runtime.jobs={};fail=false;clock=200
local recover_calls=0;api.journal={entries={stale=true},put=function()end,remove=function()end,recover=function()recover_calls=recover_calls+1;return false end}
local progressing=Auto.new(api);progressing.enabled=true;local started_before=starts
for i=1,6 do progressing:step()end
check(recover_calls==6 and starts>started_before)
-- Pausing new dispatch must also pause unjournaled orphan adoption.
local adopt_calls=0;runtime.restore_persisted=function()end;runtime.fuel_service={ready=function()return true end}
local adoption_gate=Auto.new(api);adoption_gate.orphans={refresh=function()end,step=function()adopt_calls=adopt_calls+1 end}
adoption_gate:step();check(adopt_calls==0)
adoption_gate.enabled=true;adoption_gate:step();check(adopt_calls==1)
-- One broken vehicle wrapper must not freeze the registry for valid vehicles.
local broken=obj('broken');broken.GetVehicleRoute=function()error('stale identifier')end
vehicles[#vehicles+1]=broken;runtime.jobs={};clock=500
local tolerant=Auto.new(api);tolerant.enabled=true;local prior_starts=starts
for i=1,6 do tolerant:step()end
check(starts>prior_starts and #tolerant.registry_vehicles==2)
vehicles[#vehicles]=nil
print(n..' scheduler checks passed: one inventory/pass, shared catalog, busy/recovery quiet, retry backoff and disabled gate')

-- Cargo priority must reach the runtime and journal even with a longer route.
clock=1000;runtime.jobs={};runtime.fuel_service=nil
local cargo_intents=0
api.journal={entries={},put_cargo=function(_,home,requester)check(home==Core.guid_key(guid(1))and requester=='r');cargo_intents=cargo_intents+1 end,remove=function()end}
local cargo_home=Core.guid_key(guid(1));local cargo_started=0
selector.select=function(_,home)
 if home==cargo_home then return {home=home,requester='r',item='item',truckload=47,amount=47,distance=1000,cargo_delivery=true}end
 return {home=home,provider='p',requester='r',item='item',truckload=4800,distance=1}
end
runtime.begin_cargo=function(self,home,requester,item,amount)
 check(home==cargo_home and requester=='r' and item==resource and amount==47)
 cargo_started=cargo_started+1;self.jobs[home]={state='Recovery'};return self.jobs[home]
end
local prioritized=Auto.new(api);prioritized.enabled=true
for i=1,5 do prioritized:step()end
check(cargo_started==1 and cargo_intents==1 and prioritized.last_choice.kind=='cargo')
api.journal=nil
-- An unavailable loaded candidate never prevents the eligible fresh truck.
runtime.jobs={};clock=1100;local fresh_before=starts
selector.select=function(_,home)
 if home==cargo_home then error('cargo has no reachable requester')end
 return {home=home,provider='p',requester='r',item='item',truckload=4800,distance=1}
end
local fallback=Auto.new(api);fallback.enabled=true
for i=1,5 do fallback:step()end
check(starts==fresh_before+1 and fallback.last_choice.kind=='pickup')
print('Scheduler cargo-priority and unavailable-cargo fallback checks passed')
-- A 105-station catalog must not consume 26 seconds again for every assignment.
-- Simulate the real 250ms scheduler cadence with ten idle trucks and independent requests.
local large_stations,large_vehicles={},{}
for i=1,105 do
 local index=i;local building=obj('building'..i);building.mBuiltWithRecipe=i<=10 and recipe or nil
 building.HasPower=function()return true end;building.IsProductionPaused=function()return false end
 local st=obj('large-station'..i);st.GetPathNodeGUID=function()return guid(index)end;st.GetStation=function()return building end
 large_stations[i]=st
 if i<=10 then local v=obj('fleet'..i);v.GetVehicleRoute=function()return {guid(index)}end;large_vehicles[i]=v end
end
clock=0;runtime.jobs={};local dispatch_times={};local checks_before=n
api.stations=function()return large_stations end;api.vehicles=function()return large_vehicles end
selector.select=function(_,home)
 return {home=home,provider='p'..home,requester='r'..home,item='item',truckload=4800,distance=1}
end
runtime.begin=function(self,home)
 dispatch_times[#dispatch_times+1]=clock;local job={state='Trip'};self.jobs[home]=job;return job
end
local fleet=Auto.new(api);fleet.enabled=true
for i=1,320 do
 clock=(i-1)*.25;local before_samples=fleet.samples;local before_searches=fleet.searches
 fleet:step();assert(fleet.samples-before_samples<=1);assert(fleet.searches-before_searches<=1)
end
check(#dispatch_times==10)
check(dispatch_times[1]>=26 and dispatch_times[#dispatch_times]-dispatch_times[1]<30)
check(fleet.refreshes>=2)
print('105-station fleet: '..#dispatch_times..' jobs started, warm fleet fill seconds='..(dispatch_times[#dispatch_times]-dispatch_times[1])..'; one inventory and one vehicle comparison per pass')
-- Registry refresh overdue during a long ranking must not discard its winner.
clock=0;runtime.jobs={};dispatch_times={};local long=Auto.new(api);long.enabled=true
long.catalog={providers={},requesters={},expires=999};long.next_registry=0
for i=1,11 do clock=(i-1)*4;long:step()end
check(#dispatch_times==1 and long.rank_rounds==1)
print('Overdue registry refresh preserves in-progress ranking check passed')
