local root=assert(arg[1]);package.path=root..'/?.lua;'..package.path
local C=require('Core');local n=0;local function check(v)assert(v,debug.traceback());n=n+1 end
local function g(i)return {A=i,B=0,C=0,D=0}end;local function k(i)return C.guid_key(g(i))end
local function object(name)return {IsValid=function()return true end,GetFullName=function()return name end}end
local recipe=object('rest');local item=object('item');local powered=true;local mismatch=false
local stations={};for i=1,8 do
 local b=object('building'..i);if i==1 or i==4 then b.mBuiltWithRecipe=recipe end
 local load=i==2 or i==5;b.GetIsInLoadMode=function()return load end
 b.HasPower=function()return powered end;b.IsProductionPaused=function()return false end
 local s=object('station'..i);s.GetStation=function()return b end;stations[k(i)]=s
 s.data={depot=i==1 or i==4,role=(i==2 or i==5)and 'provider'or 'requester',configured=true,resource=item,item='item'}
end
local old=package.loaded.CargoAudit
package.loaded.CargoAudit={station=function(_,s)return s.data end,inventory=function()return {kinds=1,items=mismatch and {other=true}or{item=true},quantity=50}end}
package.loaded.OrphanRoutes=nil;local Orphans=require('OrphanRoutes');package.loaded.CargoAudit=old
local clock=100;local entries={};local writes=0;local restores=0;local fail=false;local runtime={jobs={},reservations={}}
local function vehicle(name,h,p,r)
 local v=object(name);v.enabled=true;v.driver=false;v.r={g(h),g(p),g(r),g(h)}
 v.GetVehicleRoute=function()return v.r end;v.IsAutopilotEnabled=function()return v.enabled end
 local owner=object(name..'-owner');owner.HasActiveDriver=function()return v.driver end;owner.GetStorageInventory=function()return {}end
 v.GetOwnerVehicle=function()return owner end
 v.SetVehicleRoute=function()error('reconciler must not write a native route')end;v.SetAutopilotEnabled=v.SetVehicleRoute
 return v
end
local a,b=vehicle('truck-a',1,2,3),vehicle('truck-b',4,5,6)
local api={call=function(o,m,...)return o[m](o,...)end,valid=function(o)return o and o.IsValid and o:IsValid()end,
 find=function()return recipe end,now=function()return clock end,log=function()end,authority=function()end,runtime=function()return runtime end,
 journal={entries=entries,put=function(_,h,p,r)entries[h]={provider=p,requester=r};writes=writes+1 end,remove=function(_,h)entries[h]=nil;writes=writes+1 end}}
function runtime:restore_persisted(h,p,r,v)
 if fail and v==a then error('unavailable native endpoint')end
 restores=restores+1;self.jobs[h]={vehicle_key=v:GetFullName(),state='Recovery'}
end
local function fresh(list)
 runtime.jobs={};runtime.reservations={};entries={};api.journal.entries=entries;restores=0;writes=0
 local r=Orphans.new(api);local rows={};for _,v in ipairs(list or {a,b})do rows[#rows+1]={vehicle=v,route=v.r}end
 r:refresh(rows,stations);return r
end
local r=fresh();r:step();check(restores==1);r:step();check(restores==2);r:step();check(restores==2)
check(r.records['truck-a'].status=='adopted');check(entries[k(1)].requester==k(3))
a.enabled=false;r=fresh({a});r:step();check(restores==0 and writes==0);check(r.records['truck-a'].status=='opted-out');check(not a.enabled)
a.enabled=true;a.driver=true;r=fresh({a});r:step();check(restores==0 and writes==0);check(r.records['truck-a'].status=='deferred');a.driver=false
r:refresh({{vehicle=a,route=a.r}},stations);r:step();check(restores==1)
local ordinary=vehicle('coal',1,2,3);ordinary.r={g(2),g(3)};r=fresh({ordinary});r:step();check(restores==0 and #r.queue==0)
local clone=vehicle('duplicate',1,2,3);clone.r={g(1)};r=fresh({a,clone});r:step();check(restores==0 and writes==0);check(r.records['truck-a'].reason:find('multiple vehicles',1,true))
mismatch=true;r=fresh({a});r:step();check(restores==0 and writes==0);mismatch=false
r=fresh({a});runtime.reservations[k(3)]=k(4);r:step();check(restores==0 and writes==0)
powered=false;r=fresh({a});r:step();check(restores==0 and writes==0);powered=true
r=fresh({a});entries[k(1)]={provider=k(7),requester=k(8)};r:step();check(entries[k(1)].provider==k(2)and restores==1)
fail=true;r=fresh();entries[k(1)]={provider=k(7),requester=k(8)};r:step();check(entries[k(1)].provider==k(7)and not runtime.jobs[k(1)])
r:step();check(restores==1 and runtime.jobs[k(4)]);check(r.records['truck-a'].status=='blocked');fail=false
r=fresh({a});runtime.jobs[k(1)]={state='Detached',vehicle_key=a:GetFullName()};r:step();check(restores==0 and writes==0);check(r.records['truck-a'].status=='opted-out')
r=fresh({a});local keep=a.r;a.r={g(1)};r:step();check(restores==0 and writes==0);a.r=keep
r=fresh({a});r:step();r:refresh({},stations);check(next(r.records)==nil)
-- A broken station wrapper in discovery cannot block a different vehicle.
r=fresh();local original_station=stations[k(1)].GetStation
stations[k(1)].GetStation=function()error('stale home wrapper')end
r:refresh({{vehicle=a,route=a.r},{vehicle=b,route=b.r}},stations);r:step()
check(restores==1 and runtime.jobs[k(4)]);check(r.records['truck-a'].status=='blocked')
stations[k(1)].GetStation=original_station
-- A fleet larger than the 30-second/250-ms sweep window must not starve at its tail.
local many,rows={},{}
for i=1,300 do
 local h=1000+i*3
 for offset=0,2 do
  local building=object('scale-building'..(h+offset));if offset==0 then building.mBuiltWithRecipe=recipe end
  local load=offset==1;building.GetIsInLoadMode=function()return load end
  building.HasPower=function()return true end;building.IsProductionPaused=function()return false end
  local station=object('scale-station'..(h+offset));station.GetStation=function()return building end
  station.data={depot=offset==0,role=offset==1 and 'provider'or'requester',configured=true,resource=item,item='item'}
  stations[k(h+offset)]=station
 end
 local v=vehicle(string.format('fleet-%03d',i),h,h+1,h+2);many[#many+1]=v;rows[#rows+1]={vehicle=v,route=v.r}
end
r=fresh(many)
for i=1,120 do local before=restores;r:step();check(restores-before==1)end
r:refresh(rows,stations);r:step();check(restores==121 and r.records['fleet-121'].status=='adopted')
for i=122,300 do r:step()end;check(restores==300)
print(n..' orphan-route checks passed: bounded adoption, explicit opt-out, driver deferral, normal truck exclusion, conflicts and journal rollback; no native integration claimed')
