local root=assert(arg[1]);package.path=root..'/?.lua;'..package.path
local Core=require('Core');local J=require('DispatchJournal');local n=0
local function check(v)assert(v);n=n+1 end
local function guid(i)return {A=i,B=0,C=0,D=0}end
local home,p,r=Core.guid_key(guid(1)),Core.guid_key(guid(2)),Core.guid_key(guid(3))
local file=os.tmpname();os.remove(file)
local j=J.open(file);j:put(home,p,r);check(j.entries[home].provider==p);j=J.open(file);check(j.entries[home].requester==r)
local enabled=true;local route={guid(2),guid(3),guid(1)};local empty=true
local inventory={IsEmpty=function()return empty end};local owner={HasActiveDriver=function()return false end,GetStorageInventory=function()return inventory end}
local v={GetVehicleRoute=function()return route end,SetAutopilotEnabled=function(_,value)enabled=value end,IsAutopilotEnabled=function()return enabled end,GetOwnerVehicle=function()return owner end}
local api={call=function(o,m,...)return o[m](o,...)end,vehicles=function()return {v}end,authority=function()end,log=function()end}
j:recover(api);check(enabled);check(j.entries[home]~=nil)
route={guid(1)};empty=false;j:recover(api);check(j.entries[home]~=nil)
empty=true;j:recover(api);check(j.entries[home]==nil);check(next(J.open(file).entries)==nil)
j:put(home,p,r);route={guid(1),guid(2),guid(3),guid(1)};enabled=true;j:recover(api);check(enabled and j.entries[home]~=nil)
route={guid(1)};empty=true;j:recover(api);check(j.entries[home]==nil)
j:put(home,p,r);os.rename(file,file..'.bak');check(J.open(file).entries[home]~=nil)
check(not pcall(j.put,j,'bad',p,r))
-- Runtime-aware reload hands the owned trip to observed recovery without disabling autopilot.
j=J.open(file..'.new');j:put(home,p,r);enabled=true;route={guid(1),guid(2),guid(3),guid(1)}
local restored=0;api.runtime=function()return {restore_persisted=function(_,h,provider,requester,vehicle)
 check(h==home and provider==p and requester==r and vehicle==v);restored=restored+1
end}end
j:recover(api);check(enabled and restored==1 and j.entries[home]~=nil)
route={guid(1),guid(4)};j:recover(api);check(enabled and restored==2)
enabled=false;j:recover(api);check(not enabled and j.entries[home]==nil and restored==2)
-- One broken entry cannot prevent a different valid entry from restoring; budget is explicit.
local isolated=J.open(file..'.isolated');isolated:put(home,p,r);local other=Core.guid_key(guid(4));isolated:put(other,p,r)
local v2={GetVehicleRoute=function()return {guid(4),guid(2),guid(3),guid(4)}end,IsAutopilotEnabled=function()return true end,GetOwnerVehicle=function()return owner end}
enabled=true;route={guid(1),guid(2),guid(3),guid(1)};api.vehicles=function()return {v,v2}end
local attempts=0;api.runtime=function()return {restore_persisted=function(_,h)attempts=attempts+1;if h==home then error('missing endpoint')end end}end
check(isolated:recover(api,1)==false and attempts==1)
check(isolated:recover(api,1)==true and attempts==2)
check(isolated.entries[home]~=nil)
-- Cached registry matching must recheck only the selected truck, not query the entire fleet.
local cached={{vehicle=v2,route={guid(4),guid(2),guid(3),guid(4)}}}
for i=1,300 do cached[#cached+1]={vehicle={},route={guid(100+i)}}end
api.recovery_vehicles=function()return cached end
local route_calls=0;local normal_call=api.call
api.call=function(o,m,...)if m=='GetVehicleRoute'then route_calls=route_calls+1 end;return normal_call(o,m,...)end
isolated.entries={[other]={provider=p,requester=r}};check(isolated:recover(api,1)==true);check(route_calls==1)
-- A stale cache cannot bind a player-edited route to the old journal endpoints.
v2.GetVehicleRoute=function()return {guid(4),guid(8),guid(9),guid(4)}end
local before_attempts=attempts;isolated:recover(api,1);check(attempts==before_attempts)
os.remove(file..'.isolated');os.remove(file..'.isolated.bak');os.remove(file..'.isolated.tmp')
os.remove(file..'.new');os.remove(file..'.new.bak')
os.remove(file);os.remove(file..'.bak');os.remove(file..'.tmp')
print(n..' dispatch journal checks passed: reload observation without control writes, cargo guard, completed cleanup and backup recovery')
-- Typed retained-cargo intents coexist with old rows and survive a reload.
local cargo_file=os.tmpname();os.remove(cargo_file);local cj=J.open(cargo_file)
cj:put_cargo(home,r);cj:put(p,home,r);cj=J.open(cargo_file)
check(cj.entries[home].kind=='cargo' and cj.entries[home].provider==nil)
check(cj.entries[p].provider==home)
cj:remove(p);local carry_restore=0;local driven=false
api.call=normal_call;api.recovery_vehicles=nil;api.vehicles=function()return {v}end
owner.HasActiveDriver=function()return driven end
api.runtime=function()return {restore_cargo=function(_,h,requester,vehicle)check(h==home and requester==r and vehicle==v);carry_restore=carry_restore+1 end}end
route={guid(1)};empty=false;enabled=true;cj:recover(api);check(carry_restore==1 and cj.entries[home]~=nil)
route={guid(3),guid(1)};cj:recover(api);check(carry_restore==2)
driven=true;cj:recover(api);check(carry_restore==2 and cj.entries[home]~=nil);driven=false
enabled=false;cj:recover(api);check(carry_restore==2 and cj.entries[home]==nil)
cj:put_cargo(home,r);enabled=true;route={guid(1)};empty=true;cj:recover(api);check(cj.entries[home]==nil)
cj:put_cargo(home,r);route={guid(5),guid(6)};empty=false;cj:recover(api);check(carry_restore==2)
local flush=cj.flush;cj.flush=function()error('disk full')end
check(not pcall(cj.put_cargo,cj,home,p));check(cj.entries[home].requester==r)
cj.flush=flush;os.rename(cargo_file,cargo_file..'.bak');check(J.open(cargo_file).entries[home].kind=='cargo')
check(not pcall(cj.put_cargo,cj,home,home))
os.remove(cargo_file);os.remove(cargo_file..'.bak');os.remove(cargo_file..'.tmp')
print('Retained cargo journal reload, player opt-out and rollback checks passed')
