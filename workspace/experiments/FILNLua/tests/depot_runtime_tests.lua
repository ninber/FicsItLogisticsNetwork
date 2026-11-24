local root=assert(arg[1]);package.path=root..'/?.lua;'..package.path
local Depot=require('DepotRuntime')
local Core=require('Core')
local function g(n)return {A=n,B=0,C=0,D=0}end
local function k(n)return Core.guid_key(g(n))end
local checks=0
local function check(value)assert(value,debug.traceback());checks=checks+1 end
local function object(name)return {IsValid=function()return true end,GetFullName=function()return name end}end
local recipe=object('custom-recipe');local fuel=object('empty-fuel');fuel.IsEmpty=function()return true end;local fuel_items=0;fuel.GetInventoryStacks=function(_,out)if fuel_items>0 then out[1]={NumItems=fuel_items}end end
local cargo=object('empty-cargo');cargo.IsEmpty=fuel.IsEmpty
local station,buildings={},{ }
local occupant=nil;local location={X=0,Y=0,Z=0};local clock=0;local logs={};local blocked=false
local vehicle=object('truck-info');local owner=object('truck');local network=object('network')
local autopilot=true;local current_route={g(1)};local error_status=3
vehicle.GetVehicleRoute=function()return current_route end
vehicle.IsAutopilotEnabled=function()return autopilot end
vehicle.SetAutopilotEnabled=function(_,v)autopilot=v;error_status=#current_route<2 and 3 or 0 end
vehicle.SetVehicleRoute=function(_,r)current_route=r;error_status=#r<2 and 3 or 0 end
vehicle.GetAutopilotErrorStatus=function()return error_status end
vehicle.IsCurrentlyDocking=function()return occupant~=nil end
vehicle.GetOwnerVehicle=function()return owner end
owner.HasFuel=function()return true end
owner.GetVehicleLocationAndRotation=function(_,out)for n,v in pairs(location)do out[n]=v end end
owner.GetVehiclePathPreset=function()return object('preset')end
owner.GetFuelInventory=function()return fuel end;owner.GetStorageInventory=function()return cargo end
local driver=false;owner.HasActiveDriver=function()return driver end
for i=1,3 do
 local index=i;local b=object('building'..i);b.HasPower=function()return true end;b.IsProductionPaused=function()return false end;buildings[i]=b;b.mBuiltWithRecipe=recipe
 b.GetDockedActor=function()return occupant==index and owner or nil end
 b.DockActor=function(_,actor)check(actor==owner and not driver);occupant=index;return true end
 b.GetFuelInventory=function()return fuel end;b.GetInventory=function()return cargo end
 b.GetIsInLoadMode=function()return true end;b.GetDockingStationResourceForm=function()return 1 end
 b.K2_GetComponentsByClass=function()return {}end
 local node=object('node'..i);node.K2_GetActorLocation=function()return {X=(index-1)*1000,Y=0,Z=0}end
 b.GetDockingPathNode=function()return node end
 local s=object('station'..i);s.GetStation=function()return b end;s.GetPathNodeGUID=function()return g(index)end;s.GetStationName=function()return 'station'..index end;station[i]=s
end
network.FindVehiclePath=function()return not blocked end
local context=object('context');context.FindNetworkByPathNodeGuid=function()return network end
local api={call=function(o,m,...)return assert(o[m],m)(o,...)end,
 valid=function(o)return o and o:IsValid()end,find=function(path)if path:find('Recipe_RestStop',1,true)then return recipe end;return object('connection-class')end,
 stations=function()return station end,vehicles=function()return {vehicle}end,context=function()return context end,
 authority=function()end,now=function()return clock end,log=function(s)logs[#logs+1]=s end}
local function new()return Depot.new(api)end
local r=new();check(r:probe(k(1),k(2),k(3)).home==k(1))
local player=object('local-player');player.IsLocallyControlled=function()return true end
owner.GetDriver=function()return player end
driver=true;check(pcall(r.prepare,r,k(1),k(2),k(3)));check(driver and autopilot);driver=false

-- Occupied dispatch preserves autopilot and possession; never toggle controls.
driver=true
local original_set=vehicle.SetAutopilotEnabled
vehicle.SetAutopilotEnabled=function()error('occupied dispatch toggled autopilot')end
local occupied=new();local riding=occupied:begin(k(1),k(2),k(3))
check(riding.state=='Trip' and driver and autopilot and #current_route==4)
autopilot=false;occupied:tick();check(riding.state=='Detached' and driver)
check(not occupied.reservations[k(2)] and not occupied.reservations[k(3)])
current_route={g(1)};local disabled=new();check(not pcall(disabled.begin,disabled,k(1),k(2),k(3)))
-- Native route-only changes can leave the old TooFewStations status until a later tick.
autopilot=true;current_route={g(1)};error_status=3
local immediate_route=vehicle.SetVehicleRoute
vehicle.SetVehicleRoute=function(_,value)current_route=value end
local delayed=new();local delayed_job=delayed:begin(k(1),k(2),k(3))
check(delayed_job.state=='Trip' and delayed_job.enable_deadline==5)
delayed:tick();check(delayed_job.state=='Trip')
error_status=0;delayed:tick();check(delayed_job.enable_deadline==nil)
occupant=2;location={X=1200,Y=0,Z=0};delayed:tick()
occupant=3;delayed:tick();occupant=1;location={X=0,Y=0,Z=0};delayed:tick()
check(delayed_job.state=='Holding' and autopilot and driver and #current_route==1)
clock=12;delayed:tick();check(delayed_job.state=='Idle' and not delayed.reservations[k(2)])
occupant=nil;clock=0;current_route={g(1)};error_status=3
local timed=new();local timed_job=timed:begin(k(1),k(2),k(3))
clock=5;timed:tick();check(timed_job.state=='Recovery' and autopilot and driver)
autopilot=false;timed:tick();check(timed_job.state=='Detached')
vehicle.SetVehicleRoute=immediate_route;clock=0

vehicle.SetAutopilotEnabled=original_set
autopilot=true;current_route={g(1)};error_status=3
location={X=17500,Y=0,Z=0}
check(not pcall(r.begin,r,k(1),k(2),k(3)));check(#current_route==1 and autopilot and driver)
location={X=0,Y=0,Z=0}
local set_route=vehicle.SetVehicleRoute
vehicle.SetVehicleRoute=function()error('native route write failed')end
local failed=new();check(not pcall(failed.begin,failed,k(1),k(2),k(3)))
check(failed.jobs[k(1)].state=='Recovery' and #failed.active==1)
check(driver and autopilot and #current_route==1)
autopilot=false;failed:tick()
check(failed.jobs[k(1)].state=='Detached' and #failed.active==0)
check(not failed.reservations[k(2)] and not failed.reservations[k(3)])
autopilot=true
local abandoned=new();check(not pcall(abandoned.begin,abandoned,k(1),k(2),k(3)))
vehicle.SetVehicleRoute=set_route;driver=false;abandoned:tick()
check(abandoned.jobs[k(1)].state=='Idle' and #abandoned.active==1)
check(autopilot and #current_route==1)
autopilot=true
blocked=true;check(not pcall(r.begin,r,k(1),k(2),k(3)));check(autopilot and #current_route==1);blocked=false
local job=r:begin(k(1),k(2),k(3));check(job.state=='Trip');check(autopilot and #current_route==4)
check(not pcall(r.begin,r,k(1),k(2),k(3)))
location={X=400,Y=0,Z=0};occupant=nil;r:tick()
occupant=1;r:tick();check(job.state=='Trip' and job.visited==0 and job.departed)
occupant=nil
driver=true;local held_route=current_route;r:tick();check(job.state=='Trip' and autopilot);check(current_route==held_route);driver=false
local previous_owner=owner
owner=object('replacement-truck-actor');owner.HasFuel=function()return true end;owner.GetFuelInventory=function()return fuel end;owner.GetStorageInventory=function()return cargo end
owner.GetVehicleLocationAndRotation=previous_owner.GetVehicleLocationAndRotation
owner.GetVehiclePathPreset=previous_owner.GetVehiclePathPreset
owner.HasActiveDriver=previous_owner.HasActiveDriver
previous_owner.GetVehicleLocationAndRotation=function()error('stale owner wrapper')end
location={X=1200,Y=0,Z=0};occupant=2;r:tick();check(job.visited==1 and job.departed)
check(job.owner==owner)

occupant=3;r:tick();check(job.visited==2)
occupant=1;location={X=0,Y=0,Z=0};r:tick();check(job.state=='Holding');check(autopilot and #current_route==1)
clock=11;r:tick();check(job.state=='Holding');clock=12;r:tick();check(job.state=='Idle')
check(not r.reservations[k(2)] and not r.reservations[k(3)])
r:guard(k(1),k(2),k(3));check(autopilot and #current_route==1)
blocked=true;r:blocked_check(k(1),k(2),k(3));check(autopilot and #current_route==1);blocked=false
occupant=1
local next_job=r:begin(k(1),k(2),k(3));check(next_job.cycle==2);check(#r.active==1)
location={X=1200,Y=0,Z=0};occupant=2;r:tick();occupant=3;r:tick()
occupant=1;location={X=0,Y=0,Z=0};r:tick();clock=24;r:tick();check(next_job.state=='Idle')
autopilot=false;r:tick();check(next_job.state=='Detached');check(not autopilot and #current_route==1)

-- Active TooFewStations is a failure; the same status was accepted for the one-home idle fixture.
clock=20;occupant=nil;error_status=3;current_route={g(1)}
local q=new();local second=q:begin(k(1),k(2),k(3));error_status=3;q:tick()
check(second.state=='Recovery');check(autopilot and #current_route==4)
-- Missing first-stop observations must not fabricate a successful return.
local z=new();occupant=nil;error_status=3;current_route={g(1)};autopilot=true
local third=z:begin(k(1),k(2),k(3));occupant=3;z:tick();check(third.state=='Recovery')
-- Refueling on return must finish natively before the home-only route is restored.
clock=50;occupant=nil;location={X=0,Y=0,Z=0};current_route={g(1)};autopilot=true;error_status=3
local servicing=new();local service_job=servicing:begin(k(1),k(2),k(3))
check(#current_route==4 and Core.guid_key(current_route[1])==k(1) and Core.guid_key(current_route[4])==k(1))
occupant=2;location={X=1200,Y=0,Z=0};servicing:tick();occupant=3;servicing:tick()
fuel.IsEmpty=function()return false end;occupant=1;location={X=0,Y=0,Z=0}
servicing:tick();check(service_job.state=='Servicing' and #current_route==4 and autopilot)
clock=58;servicing:tick();check(service_job.state=='Servicing')
local native_docking=vehicle.IsCurrentlyDocking;vehicle.IsCurrentlyDocking=function()return false end
servicing:tick();check(service_job.state=='Holding' and #current_route==1 and autopilot)
clock=70;servicing:tick();check(service_job.state=='Idle')
vehicle.IsCurrentlyDocking=native_docking
-- Manual autopilot-off during native servicing releases leases without route writes.
occupant=nil;current_route={g(1)};error_status=3;autopilot=true
local opt_service=new();local opt_job=opt_service:begin(k(1),k(2),k(3))
occupant=2;location={X=1200,Y=0,Z=0};opt_service:tick();occupant=3;opt_service:tick()
occupant=1;location={X=0,Y=0,Z=0};opt_service:tick();check(opt_job.state=='Servicing')
local service_route=current_route;autopilot=false;opt_service:tick()
check(opt_job.state=='Detached' and current_route==service_route)
check(not opt_service.reservations[k(2)] and not opt_service.reservations[k(3)])
-- A dry truck may plan a home-first route only when its depot contains fuel.
owner.HasFuel=function()return false end;current_route={g(1)};error_status=4;autopilot=true;occupant=nil
local bootstrap=new();local boot=bootstrap:begin(k(1),k(2),k(3));check(boot.fuel_deadline==clock+45 and #current_route==4)
error_status=4;bootstrap:tick();check(boot.state=='Trip' and autopilot)
owner.HasFuel=function()return true end;error_status=0;bootstrap:tick();check(boot.fuel_deadline==nil)
autopilot=false;bootstrap:tick();check(boot.state=='Detached' and #current_route==4)
owner.HasFuel=function()return false end;fuel.IsEmpty=function()return true end;local fuel_items=0;fuel.GetInventoryStacks=function(_,out)if fuel_items>0 then out[1]={NumItems=fuel_items}end end;current_route={g(1)};autopilot=true;error_status=4
local dry=new();check(not pcall(dry.begin,dry,k(1),k(2),k(3)))
owner.HasFuel=function()return true end;clock=30;error_status=3;occupant=nil;current_route={g(1)}
-- Real-endpoint branch: LOAD provider, UNLOAD requester and measured truck cargo.
local resource=object('Class /Game/Item');local carried=0
local previous_find=api.find;api.find=function(path)if path=='/Game/Item'then return resource end;return previous_find(path)end
local function stock_inventory(amount)
 local inv=object('stock')
 inv.IsEmpty=function()return amount==0 end
 inv.GetSizeLinear=function()return 2 end
 inv.GetAllowedItemOnIndex=function()return nil end
 inv.GetSlotSize=function()return 100 end
 inv.GetInventoryStacks=function(_,out)if amount>0 then out[1]={Item={ItemClass=resource},NumItems=amount}end end
 return inv
end
for _,i in ipairs({2,3})do buildings[i].GetClass=function()return {GetFullName=function()return 'BlueprintGeneratedClass /TruckStationSettings/Build_ProgrammableTruckStation.Build_ProgrammableTruckStation_C' end}end;buildings[i].FilterItem={resource}end
local source_inv,target_inv=stock_inventory(200),stock_inventory(0)
buildings[2].mBuiltWithRecipe=nil;buildings[3].mBuiltWithRecipe=nil
buildings[2].GetInventory=function()return source_inv end;buildings[3].GetInventory=function()return target_inv end
buildings[3].GetIsInLoadMode=function()return false end
local truck_inv=object('truck-inventory');truck_inv.IsEmpty=function()return carried==0 end
truck_inv.GetInventoryStacks=function()end;truck_inv.GetSizeLinear=function()return 2 end;truck_inv.GetAllowedItemOnIndex=function()end;truck_inv.GetSlotSize=function()return 100 end
truck_inv.GetNumItems=function()return carried end
owner.GetStorageInventory=function()return truck_inv end
occupant=nil;current_route={g(1)};error_status=3;clock=30
local delivery=new();local shipment=delivery:begin(k(1),k(2),k(3),resource)
occupant=2;location={X=1200,Y=0,Z=0};delivery:tick();carried=200;delivery:tick();check(shipment.loaded==200)
occupant=3;delivery:tick();carried=0;delivery:tick();check(shipment.unloaded==200)
occupant=1;location={X=0,Y=0,Z=0};delivery:tick();check(shipment.state=='Holding')
clock=42;delivery:tick();check(shipment.state=='Idle' and #current_route==1 and autopilot)
autopilot=true;current_route={g(1)};occupant=nil;driver=false;error_status=3
local manual=new();local detached=manual:begin(k(1),k(2),k(3),resource);local kept_route=current_route
autopilot=false;driver=true;manual:tick()
check(detached.state=='Detached');check(current_route==kept_route and not autopilot)
check(manual.reservations[k(2)]==nil and manual.reservations[k(3)]==nil);check(manual.owners[detached.vehicle_key]==nil)
driver=false
-- Fuel recovery remains observed and resumes the same trip after native fuel is restored.
autopilot=true;current_route={g(1)};occupant=nil;location={X=0,Y=0,Z=0};error_status=3;clock=100;carried=0
local repaired=new();local repairing=repaired:begin(k(1),k(2),k(3),resource)
location={X=1500,Y=0,Z=0};error_status=4;owner.HasFuel=function()return false end
repaired:tick();check(repairing.state=='Recovery' and #repaired.active==1 and autopilot and #current_route==4)
repaired:tick();check(repairing.recovery.state=='FuelWait' and repaired.reservations[k(2)]==k(1))
owner.HasFuel=function()return true end;error_status=0;repaired:tick()
check(repairing.state=='Trip' and #repaired.active==1 and autopilot and #current_route==4)
-- Player opt-out while waiting releases leases without any route/control write.
error_status=4;owner.HasFuel=function()return false end;repaired:tick();local unchanged=current_route
autopilot=false;repaired:tick();check(repairing.state=='Detached' and current_route==unchanged and not autopilot)
check(not repaired.reservations[k(2)] and not repaired.reservations[k(3)])
owner.HasFuel=function()return true end
-- Partial pickup, fully unloaded, is a real completed delivery rather than Recovery.
autopilot=true;current_route={g(1)};occupant=nil;location={X=0,Y=0,Z=0};error_status=3;clock=200;carried=0
local partial=new();local parcel=partial:begin(k(1),k(2),k(3),resource)
occupant=2;location={X=1200,Y=0,Z=0};partial:tick();carried=197;partial:tick()
occupant=3;partial:tick();carried=0;partial:tick()
occupant=1;location={X=0,Y=0,Z=0};partial:tick();clock=212;partial:tick()
check(parcel.state=='Idle' and parcel.loaded==197 and parcel.unloaded==197 and not partial.reservations[k(2)])
-- Returning with cargo must fail BEFORE changing route or autopilot, then bypass the provider.
autopilot=true;current_route={g(1)};occupant=nil;location={X=0,Y=0,Z=0};error_status=3;clock=300;carried=0
vehicle.GetCurrentToPathNodeGUID=function()return g(1)end
local incomplete=new();local remaining=incomplete:begin(k(1),k(2),k(3),resource)
occupant=2;location={X=1200,Y=0,Z=0};incomplete:tick();carried=200;incomplete:tick()
occupant=3;incomplete:tick();carried=150;incomplete:tick()
occupant=1;location={X=0,Y=0,Z=0};local before_return=current_route
incomplete:tick();check(remaining.state=='Recovery' and current_route==before_return and autopilot)
check(incomplete.owners[remaining.vehicle_key]==k(1) and incomplete.reservations[k(3)]==k(1))
incomplete:tick();check(remaining.state=='Recovery' and #current_route==2 and Core.guid_key(current_route[1])==k(3))
check(Core.guid_key(current_route[2])==k(1) and incomplete.owners[remaining.vehicle_key]==k(1))
carried=0;incomplete:tick();check(remaining.state=='Idle' and #current_route==1 and not incomplete.reservations[k(3)])
-- A missed provider observation is a logical error, not permission to resume the old loop.
autopilot=true;current_route={g(1)};occupant=nil;error_status=3;clock=400;carried=0
local missed=new();local missed_job=missed:begin(k(1),k(2),k(3),resource)
occupant=3;location={X=1200,Y=0,Z=0};carried=200;missed:tick();check(missed_job.state=='Recovery')
missed:tick();check(missed_job.state=='Recovery' and #current_route==2 and Core.guid_key(current_route[1])==k(3))
-- AP-off in this recovery still leaves the route and controls untouched.
local preserved=current_route;autopilot=false;missed:tick();check(missed_job.state=='Detached' and current_route==preserved)
-- A route write that applies before throwing must retain recovery ownership.
autopilot=true;current_route={g(1)};occupant=nil;location={X=0,Y=0,Z=0};error_status=3;clock=500;carried=0
local write_failure=new();local failed_return=write_failure:begin(k(1),k(2),k(3),resource)
occupant=2;location={X=1200,Y=0,Z=0};write_failure:tick();carried=200;write_failure:tick()
occupant=3;write_failure:tick();carried=0;write_failure:tick()
occupant=1;location={X=0,Y=0,Z=0}
local regular_write=vehicle.SetVehicleRoute
vehicle.SetVehicleRoute=function(_,intended)regular_write(vehicle,intended);error('post-write native failure')end
write_failure:tick();check(failed_return.state=='Recovery' and #current_route==1 and autopilot)
check(write_failure.owners[failed_return.vehicle_key]==k(1))
vehicle.SetVehicleRoute=regular_write;write_failure:tick();check(failed_return.state=='Idle' and write_failure.owners[failed_return.vehicle_key]==k(1))
-- Persisted recovery must acquire endpoint leases atomically.
autopilot=true;driver=false;current_route={g(1),g(2),g(3),g(1)};carried=50
local conflict=new();conflict.reservations[k(3)]=k(99)
check(not pcall(conflict.restore_persisted,conflict,k(1),k(2),k(3),vehicle))
check(conflict.reservations[k(2)]==nil and conflict.reservations[k(3)]==k(99))
check(not conflict.jobs[k(1)] and #conflict.active==0)
-- A journal cleanup failure after returning home must not orphan the route or spam writes.
autopilot=true;driver=false;current_route={g(1),g(2),g(3),g(1)};carried=0;location={X=0,Y=0,Z=0};occupant=1;clock=600
local cleanup=new();api.on_detach=function()error('journal locked')end
cleanup:restore_persisted(k(1),k(2),k(3),vehicle)
local cleanup_job=cleanup.jobs[k(1)];local cleanup_writes=0
vehicle.SetVehicleRoute=function(_,r)cleanup_writes=cleanup_writes+1;regular_write(vehicle,r)end
cleanup:tick();check(cleanup_job.state=='Recovery' and #current_route==1 and cleanup_writes==1)
check(cleanup.owners[cleanup_job.vehicle_key]==k(1) and cleanup_job.recovery.last_error:find('journal cleanup',1,true))
cleanup:tick();check(cleanup_job.state=='Recovery' and cleanup_writes==1)
api.on_detach=nil;clock=631;cleanup:tick();check(cleanup_job.state=='Idle' and cleanup_writes==1)
vehicle.SetVehicleRoute=regular_write
-- Clearing a native fuel fault with cargo must not resume the pickup loop.
autopilot=true;driver=false;current_route={g(1)};carried=0;occupant=nil;location={X=0,Y=0,Z=0};error_status=3;clock=700
local loaded_fuel=new();local loaded_fuel_job=loaded_fuel:begin(k(1),k(2),k(3),resource)
occupant=2;location={X=1200,Y=0,Z=0};loaded_fuel:tick();carried=200;loaded_fuel:tick();occupant=nil
owner.HasFuel=function()return false end;error_status=4;loaded_fuel:tick();loaded_fuel:tick()
check(loaded_fuel_job.recovery.state=='FuelWait')
owner.HasFuel=function()return true end;error_status=0;loaded_fuel:tick()
check(loaded_fuel_job.state=='Recovery' and #current_route==2 and Core.guid_key(current_route[1])==k(3))
local registered=0
_G.LoopInGameThreadWithDelay=function(delay,fn)check(delay==250);check(fn()==false);registered=registered+1 end
local watch=new();watch:watch();watch:watch();check(registered==1)
_G.LoopInGameThreadWithDelay=nil
print(checks..' depot lifecycle checks passed; game integration NOT tested')
-- Direct retained-cargo jobs have no provider and finish with a home-only route.
truck_inv.GetInventoryStacks=function(_,out)if carried>0 then out[1]={Item={ItemClass=resource},NumItems=carried}end end
local function cargo_reset()
 autopilot=true;driver=false;current_route={g(1)};carried=47;location={X=0,Y=0,Z=0};occupant=nil;error_status=3;clock=900
end
cargo_reset();local direct=new();local retained=direct:begin_cargo(k(1),k(3),resource,47)
check(retained.kind=='cargo' and retained.truckload==47 and retained.vehicle_capacity==200)
check(direct.reservations[k(3)]==k(1) and not direct.reservations[k(2)])
check(#current_route==1 and #direct.active==1)
direct:tick();check(#current_route==2 and Core.guid_key(current_route[1])==k(3) and Core.guid_key(current_route[2])==k(1))
location={X=2000,Y=0,Z=0};occupant=3;carried=0;direct:tick()
location={X=0,Y=0,Z=0};occupant=1;direct:tick()
check(retained.state=='Idle' and #current_route==1 and retained.unloaded==47)
check(not direct.reservations[k(3)] and autopilot)
-- Reusing the idle observer never creates a duplicate active job.
carried=32;occupant=nil;local second=direct:begin_cargo(k(1),k(3),resource,32)
check(#direct.active==1 and direct.active[1]==second)
autopilot=false;direct:tick();check(second.state=='Detached' and not direct.reservations[k(3)])
cargo_reset();local riding_direct=new();driver=true
check(not pcall(riding_direct.begin_cargo,riding_direct,k(1),k(3),resource,47));check(not riding_direct.jobs[k(1)])
driver=false;check(not pcall(riding_direct.begin_cargo,riding_direct,k(1),k(3),resource,46))
-- Reload attaches only matching native routes; failed validation cannot claim a lease.
cargo_reset();current_route={g(3),g(1)};local reload=new();reload:restore_cargo(k(1),k(3),vehicle)
check(reload.jobs[k(1)].kind=='cargo' and #reload.active==1)
cargo_reset();current_route={g(2),g(1)};local wrong=new()
check(not pcall(wrong.restore_cargo,wrong,k(1),k(3),vehicle));check(not wrong.jobs[k(1)] and not wrong.reservations[k(3)])
cargo_reset();local leased=new();leased.reservations[k(3)]=k(99)
check(not pcall(leased.begin_cargo,leased,k(1),k(3),resource,47));check(not leased.jobs[k(1)])
print('Retained cargo lifecycle, return, duplicate observer, reload and opt-out checks passed')
-- Providers become reusable after loaded departure, while requester leases remain exclusive.
autopilot=true;driver=false;current_route={g(1)};carried=0;occupant=nil;location={X=0,Y=0,Z=0};error_status=3;clock=1100
local concurrent=new();local first_delivery=concurrent:begin(k(1),k(2),k(3),resource)
occupant=2;location={X=1200,Y=0,Z=0};concurrent:tick();carried=200;concurrent:tick()
check(concurrent.reservations[k(2)]==k(1))
occupant=nil;location={X=1700,Y=0,Z=0};concurrent:tick()
check(first_delivery.provider_released and concurrent.reservations[k(2)]==nil)
check(concurrent.reservations[k(3)]==k(1))
-- A new job can own that provider; the first job's eventual cleanup must not erase it.
concurrent.reservations[k(2)]=k(99)
occupant=3;location={X=2000,Y=0,Z=0};concurrent:tick();carried=0;concurrent:tick()
occupant=1;location={X=0,Y=0,Z=0};concurrent:tick();clock=1113;concurrent:tick()
check(first_delivery.state=='Idle' and concurrent.reservations[k(2)]==k(99) and not concurrent.reservations[k(3)])
-- Reloaded loaded trips must not reacquire a shared provider they will never revisit.
current_route={g(1),g(2),g(3),g(1)};carried=50;occupant=nil
local shared_restore=new();shared_restore.reservations[k(2)]=k(99)
shared_restore:restore_persisted(k(1),k(2),k(3),vehicle)
check(shared_restore.jobs[k(1)].state=='Recovery' and shared_restore.reservations[k(2)]==k(99))
check(shared_restore.reservations[k(3)]==k(1))
print('Concurrent provider release, owner-safe cleanup and shared-provider reload checks passed')
-- Live names are rechecked before claiming leases or writing a fresh pickup route.
autopilot=true;driver=false;current_route={g(1)};carried=0;occupant=nil;location={X=0,Y=0,Z=0};error_status=3
station[2].GetStationName=function()return '[S] warehouse OUT'end
station[3].GetStationName=function()return '[S] warehouse IN'end
local warehouse_guard=new();local allowed,problem=pcall(warehouse_guard.begin,warehouse_guard,k(1),k(2),k(3),resource)
check(not allowed and tostring(problem):find('storage%-to%-storage'))
check(not warehouse_guard.jobs[k(1)] and not next(warehouse_guard.reservations) and #current_route==1)
carried=47;check(not pcall(warehouse_guard.begin_cargo,warehouse_guard,k(1),k(3),resource,47))
print('Live storage-tag guard rejects fresh and unknown-origin warehouse deliveries before route writes')
