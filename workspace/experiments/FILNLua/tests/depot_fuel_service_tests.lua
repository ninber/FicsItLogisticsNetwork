local root=assert(arg[1]);package.path=root..'/?.lua;'..package.path
local Service=require('DepotFuelService');local Core=require('Core');local n=0;local function check(v)assert(v);n=n+1 end
local function obj(name)return {IsValid=function()return true end,GetFullName=function()return name end}end
local item=obj('coal');local home=Core.guid_key({A=1,B=0,C=0,D=0});local clock=0
local enabled,power,paused,driver=true,true,false,false;local amount=0;local stock=20;local docked;local calls=0
local target=obj('truck-fuel');target.GetNumItems=function()return amount end;target.GetSlotSize=function()return 100 end;target.IsEmpty=function()return amount==0 end
target.GetInventoryStacks=function(_,out)if amount>0 then out[1]={NumItems=amount,Item={ItemClass=item}}end end
local fuel=obj('station-fuel');fuel.GetInventoryStacks=function(_,out)if stock>0 then out[1]={NumItems=stock,Item={ItemClass=item}}end end
local cargo=obj('cargo');cargo.IsEmpty=function()return true end
local owner=obj('truck');owner.HasActiveDriver=function()return driver end;owner.GetFuelInventory=function()return target end
owner.GetVehicleLocationAndRotation=function(_,p)p.X=0;p.Y=0;p.Z=0 end
local route={{A=1,B=0,C=0,D=0}};local v=obj('vehicle');v.GetVehicleRoute=function()return route end;v.IsAutopilotEnabled=function()return enabled end;v.GetOwnerVehicle=function()return owner end
local b=obj('bay');b.HasPower=function()return power end;b.IsProductionPaused=function()return paused end;b.GetFuelInventory=function()return fuel end;b.GetInventory=function()return cargo end
b.GetDockedActor=function()return docked end;b.GetIsInLoadMode=function()return true end;b.GetDockingStationResourceForm=function()return 1 end;b.K2_GetComponentsByClass=function()return {}end
b.GetDockingPathNode=function()return {K2_GetActorLocation=function()return {X=0,Y=0,Z=0}end}end
b.DockActor=function(_,actor)check(actor==owner and not driver);calls=calls+1;docked=owner;return true end
local s=obj('station');s.GetStation=function()return b end;s.GetPathNodeGUID=function()return route[1]end
local api={call=function(o,m,...)return o[m](o,...)end,valid=function(o)return o and o.IsValid and o:IsValid()end,find=function()return obj('class')end,now=function()return clock end,log=function()end}
local service=Service.new(api)
power=false;check(not service:ready(home,s,v) and calls==0);power=true;driver=true;check(not service:ready(home,s,v) and calls==0);driver=false
enabled=false;check(not service:ready(home,s,v) and calls==0);enabled=true
check(not service:ready(home,s,v) and calls==1);check(enabled and #route==1)
check(not service:ready(home,s,v) and calls==1);service:tick();check(service.pending[home]~=nil)
amount=20;stock=0;service:tick();check(service.pending[home]==nil and service.receipts[home].after==20)
check(enabled and #route==1);check(not service:ready(home,s,v) and calls==1)
-- Empty depot must hold a partially fueled truck, even after the retry interval.
clock=20;check(not service:ready(home,s,v) and calls==1)
amount=100;check(service:ready(home,s,v) and calls==1)
stock=20;check(service:ready(home,s,v) and calls==1)
amount=0;stock=0;check(not service:ready(home,s,v) and calls==1)
clock=40;amount=0;stock=20;docked=nil;check(not service:ready(home,s,v));enabled=false;service:tick();check(service.pending[home]==nil and calls==2)
service:clear();check(next(service.pending)==nil and next(service.receipts)==nil and #service.queue==0 and not enabled)
print(n..' stationary depot fuel checks passed; native docking/refueling not tested')
