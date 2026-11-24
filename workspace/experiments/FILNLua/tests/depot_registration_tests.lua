local root=arg[1];package.path=root..'/?.lua;'..package.path
local B=require('Bridge');local old=B.home_audit;B.home_audit=function()return {eligible=true}end
local R=require('DepotRegistration');local C=require('Core');local g={A=1,B=0,C=0,D=0};local h=C.guid_key(g)
local file=os.tmpname();os.remove(file);local request=file..'.tsv';local result=file..'.json'
local function obj(n)return {GetFullName=function()return n end}end
local actor=obj('actor');local truck=obj('truck');local station=obj('station');local building=obj('building');local node=obj('node');local cargo=obj('cargo')
local recipe=obj('recipe');building.mBuiltWithRecipe=recipe
actor.GetForwardSpeed=function()return 0 end
local route={};local ap=false;local driver=false;local moved=false
truck.GetOwnerVehicle=function()return actor end;truck.GetVehicleRoute=function()return route end
truck.SetVehicleRoute=function(_,r)route=r end;truck.IsAutopilotEnabled=function()return ap end;truck.SetAutopilotEnabled=function(_,v)ap=v end
actor.HasActiveDriver=function()return driver end;actor.GetStorageInventory=function()return cargo end;cargo.IsEmpty=function()return true end
actor.GetVehicleLocationAndRotation=function(_,p)p.X=moved and 10000 or 0;p.Y=0;p.Z=0 end
station.GetPathNodeGUID=function()return g end;station.GetStation=function()return building end;building.GetDockingPathNode=function()return node end;node.K2_GetActorLocation=function()return {X=0,Y=0,Z=0}end
local api={call=function(o,m,...)return assert(o[m],m)(o,...)end,valid=function(o)return o~=nil end,find=function()return recipe end,authority=function()end,now=function()return 100 end,log=function()end,refresh=function()end,vehicles=function()return {truck}end,stations=function()return {station}end,request_path=request,result_path=result}
local function queue(id)local f=assert(io.open(request,'w'));f:write(string.rep(id,32)..'\n'..h..'\ttruck\n');f:close()end
queue('a');local r=R.new(api);r:step();assert(#route==1 and ap and r.results[1].ok);r:step();assert(r.completed==string.rep('a',32))
-- Restarted handler recognizes the durable completed request without another route write.
truck.SetVehicleRoute=function()error('replayed completed request')end;local resumed=R.new(api);resumed:step();assert(resumed.completed==string.rep('a',32))
route={};ap=false;driver=true;queue('b');local occupied=R.new(api);occupied:step();assert(not occupied.results[1].ok and #route==0)
driver=false;moved=true;queue('c');local far=R.new(api);far:step();assert(not far.results[1].ok and #route==0)
moved=false;truck.SetVehicleRoute=function(_,r)route=r end;truck.SetAutopilotEnabled=function(_,v)if v then error('native rejected')end;ap=v end
queue('d');local rollback=R.new(api);rollback:step();assert(not rollback.results[1].ok and #route==0 and not ap)
actor.GetForwardSpeed=function()return 10 end;queue('e');local moving=R.new(api);moving:step();assert(not moving.results[1].ok and #route==0)
actor.GetForwardSpeed=function()return 0 end;building.mBuiltWithRecipe=nil;queue('f');local wrong=R.new(api);wrong:step();assert(not wrong.results[1].ok and #route==0)
for _,p in ipairs({request,result})do os.remove(p)end;B.home_audit=old
print('Depot registration checks passed: native readback, durable idempotence, driver/movement rejection and rollback')
