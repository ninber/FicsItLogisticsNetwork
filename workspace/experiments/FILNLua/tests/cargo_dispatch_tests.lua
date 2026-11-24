local root=assert(arg[1]);package.path=root..'/?.lua;'..package.path
local Select=require('CargoDispatch');local Core=require('Core')
local function g(n)return {A=n,B=0,C=0,D=0}end
local function obj(n)return {IsValid=function()return true end,GetFullName=function()return 'Class '..n end}end
local item,recipe,preset=obj('/Game/Item'),obj('rest-recipe'),obj('preset')
local quantities={0,300,500,0};local modes={true,true,true,false};local stations={}
for i=1,4 do
 local index=i;local inventory=obj('inventory'..i)
 inventory.GetInventoryStacks=function(_,out)if quantities[index]>0 then out[1]={Item={ItemClass=item},NumItems=quantities[index]}end end
 inventory.GetSizeLinear=function()return 10 end;inventory.GetAllowedItemOnIndex=function()return nil end
 inventory.GetSlotSize=function()return 100 end
 local b=obj('station-building'..i);b.HasPower=function()return true end;b.IsProductionPaused=function()return false end;b.GetClass=function()return {GetFullName=function()return 'BlueprintGeneratedClass /TruckStationSettings/Build_ProgrammableTruckStation.Build_ProgrammableTruckStation_C' end}end;b.FilterItem={item};if i==1 then b.mBuiltWithRecipe=recipe end
 b.GetDockedActor=function()return nil end;b.GetInventory=function()return inventory end;b.GetIsInLoadMode=function()return modes[index]end
 local s=obj('station-info'..i);stations[i]=s;s.GetStation=function()return b end
 s.GetPathNodeGUID=function()return g(index)end
 s.GetStationName=function()return modes[index]and 'Misleading REQUEST label'or 'Misleading PROVIDE label'end
end
local network=obj('network');network.GetPathNodes=function()
 return {{PathNodeGuid=g(1)},{PathNodeGuid=g(2)},{PathNodeGuid=g(3)},{PathNodeGuid=g(4)}}
end
local roads={{FromNodeIndex=0,ToNodeIndex=1,SegmentLength=1},
 {FromNodeIndex=1,ToNodeIndex=3,SegmentLength=20},{FromNodeIndex=2,ToNodeIndex=3,SegmentLength=50},
 {FromNodeIndex=3,ToNodeIndex=0,SegmentLength=10},{FromNodeIndex=0,ToNodeIndex=2,SegmentLength=2}}
network.GetPathSegments=function()return roads end
network.CanVehicleTraverseSegment=function()return true end;network.FindVehiclePath=function()return true end
local context=obj('context');context.FindNetworkByPathNodeGuid=function()return network end
local cargo=obj('cargo');cargo.IsEmpty=function()return true end;cargo.GetInventoryStacks=function()end;cargo.GetSizeLinear=function()return 3 end;cargo.GetAllowedItemOnIndex=function()end;cargo.GetSlotSize=function()return 100 end
local owner=obj('truck');owner.HasActiveDriver=function()return false end;owner.HasFuel=function()return true end
owner.GetStorageInventory=function()return cargo end;owner.GetVehiclePathPreset=function()return preset end
local truck=obj('truck-info');truck.IsAutopilotEnabled=function()return true end;truck.GetOwnerVehicle=function()return owner end;truck.GetVehicleRoute=function()return {g(1)}end
local api={call=function(o,m,...)return o[m](o,...)end,valid=function(o)return o and type(o)=='table' and o.IsValid and o:IsValid()end,
 find=function(path)if path=='/Game/Item'then return item end;return recipe end,stations=function()return stations end,vehicles=function()return {truck}end,
 context=function()return context end,authority=function()end,log=function()end}
stations[2]:GetStation().GetClass=function()return {GetFullName=function()return 'BlueprintGeneratedClass /Game/Vanilla.Build_TruckStation_C' end}end
local evidence={}
api.report_request=function(home,requester,record)evidence[#evidence+1]=record end
local a=Select.select(api,Core.guid_key(g(1)))
assert(a.provider==Core.guid_key(g(2)) and a.requester==Core.guid_key(g(4)) and a.distance==31 and a.delivery_distance==20)
assert(evidence[1].reason=='route_candidate_available' and evidence[2].reason=='selected')
-- Diagnostic failures must never alter vehicle selection.
api.report_request=function()error('diagnostic failure')end
quantities[2]=299;a=Select.select(api,Core.guid_key(g(1)));assert(a.provider==Core.guid_key(g(3)))
-- Explicit item configuration survives an empty requester buffer.
quantities[4]=0;a=Select.select(api,Core.guid_key(g(1)));assert(a.requester==Core.guid_key(g(4)) and a.stock==0)
quantities[4]=1;assert(not pcall(Select.select,api,Core.guid_key(g(1))))
quantities[4]=20;modes[4]=true;assert(not pcall(Select.select,api,Core.guid_key(g(1))))
modes[4]=false;stations[4]:GetStation().FilterItem={};assert(not pcall(Select.select,api,Core.guid_key(g(1))))
quantities[4]=0;stations[4]:GetStation().FilterItem={item};quantities[3]=299;assert(not pcall(Select.select,api,Core.guid_key(g(1))))
quantities[3]=500;quantities[2]=300
roads[1].SegmentLength=200
a=Select.select(api,Core.guid_key(g(1)));assert(a.provider==Core.guid_key(g(3)) and a.distance==62)
roads[4].FromNodeIndex=0;assert(not pcall(Select.select,api,Core.guid_key(g(1))))
roads[4].FromNodeIndex=3
stations[1]:GetStation().HasPower=function()return false end
assert(not pcall(Select.select,api,Core.guid_key(g(1))))
stations[1]:GetStation().HasPower=function()return true end
stations[1]:GetStation().IsProductionPaused=function()return true end
assert(not pcall(Select.select,api,Core.guid_key(g(1))))
print('13 truckload cargo selection checks passed: switch roles, stock eligibility and directed road ranking; game APIs NOT tested')
-- Retained cargo bypasses provider pickup, but never explicit request rules.
local cargo_checks=0
local function check(v)assert(v);cargo_checks=cargo_checks+1 end
stations[1]:GetStation().IsProductionPaused=function()return false end
local amount=47;cargo.IsEmpty=function()return amount==0 end
cargo.GetInventoryStacks=function(_,out)if amount>0 then out[1]={Item={ItemClass=item},NumItems=amount}end end
quantities[2]=0;quantities[3]=0;quantities[4]=0
local h=Core.guid_key(g(1));local request=Core.guid_key(g(4))
a=Select.select(api,h);check(a.cargo_delivery and a.requester==request and not a.provider and a.amount==47)
check(a.distance==62 and a.pickup_distance==0)
check(Select.better(a,{home=h,distance=1}));check(not Select.better({home=h,distance=1},a))
check(Select.better({cargo_delivery=true,home='a',distance=3},{cargo_delivery=true,home='b',distance=4}))
check(Select.better({cargo_delivery=true,home='a',distance=3},{cargo_delivery=true,home='b',distance=3}))
quantities[4]=1;check(not pcall(Select.select,api,h));quantities[4]=0
stations[4]:GetStation().FilterItem={};check(not pcall(Select.select,api,h));stations[4]:GetStation().FilterItem={item}
amount=1001;check(not pcall(Select.select,api,h));amount=47
network.FindVehiclePath=function()return false end;check(not pcall(Select.select,api,h));network.FindVehiclePath=function()return true end
truck.IsAutopilotEnabled=function()return false end;api.enrolled=function()return true end;check(not pcall(Select.select,api,h));truck.IsAutopilotEnabled=function()return true end
owner.HasActiveDriver=function()return true end;check(not pcall(Select.select,api,h));owner.HasActiveDriver=function()return false end
local other=obj('/Game/Other')
cargo.GetInventoryStacks=function(_,out)out[1]={Item={ItemClass=item},NumItems=47};out[2]={Item={ItemClass=other},NumItems=1}end
check(not pcall(Select.select,api,h))
cargo.GetInventoryStacks=function(_,out)out[1]={Item={ItemClass=other},NumItems=47}end
check(not pcall(Select.select,api,h))
print(cargo_checks..' retained cargo priority and eligibility checks passed')
-- A disconnected full provider must be named; do not imply requester/home are disconnected.
amount=0;cargo.IsEmpty=function()return true end;quantities[2]=300;quantities[3]=0;quantities[4]=0
local isolated=obj('isolated-provider-network')
context.FindNetworkByPathNodeGuid=function(_,id)return id.A==2 and isolated or network end
api.distances=function(_,_,destination)
 if destination==request then return {[h]=10}end
 if destination==h then return {[request]=10}end
 return {}
end
local failed
api.report_request=function(_,_,r)if r.reason=='unreachable_legs'then failed=r end end
check(not pcall(Select.select,api,h));check(failed.failures[1].provider==Core.guid_key(g(2)))
check(failed.failures[1].leg=='provider_to_requester' and failed.failures[1].reason=='provider_other_network')
print('Disconnected-provider evidence names the failed endpoint/leg check passed')
-- Warehouse pairs are forbidden; choose an ordinary provider even when farther away.
api.distances=nil;context.FindNetworkByPathNodeGuid=function()return network end
quantities[2]=300;quantities[3]=500;quantities[4]=0
stations[2].GetStationName=function()return '[S] warehouse OUT'end
stations[3].GetStationName=function()return 'Factory OUT'end
stations[4].GetStationName=function()return '[S] warehouse IN'end
local warehouse=Select.select(api,h);check(warehouse.provider==Core.guid_key(g(3)))
quantities[3]=0;check(not pcall(Select.select,api,h))
stations[4].GetStationName=function()return 'Factory IN'end
check(Select.select(api,h).provider==Core.guid_key(g(2)))
print('Warehouse loop blocked; warehouse/factory delivery directions remain eligible')
-- Unknown-origin retained cargo cannot refill a tagged warehouse and recreate a loop.
cargo.IsEmpty=function()return false end
cargo.GetInventoryStacks=function(_,out)out[1]={Item={ItemClass=item},NumItems=47}end
stations[4].GetStationName=function()return '[S] warehouse IN'end
check(not pcall(Select.select,api,h))
stations[4].GetStationName=function()return 'Factory IN'end
check(Select.select(api,h).cargo_delivery==true)
-- A missing retained-cargo route reports an ordinary eligibility failure, not undefined provider.
api.distances=function()return {}end
local ok,why=pcall(Select.select,api,h)
check(not ok and tostring(why):find('retained cargo has no empty matching requester',1,true))
print('Unknown-origin retained cargo warehouse exclusion and missing-path checks passed')
