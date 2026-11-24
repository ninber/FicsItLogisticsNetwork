local root=assert(arg[1]);package.path=root..'/?.lua;'..package.path
local Traffic=require('TrafficIsolation');local Core=require('Core')
local checks=0
local function check(value)assert(value);checks=checks+1 end
local function guid(n)return {A=n,B=0,C=0,D=0}end
local home=Core.guid_key(guid(1));local writes=0;local logs={}
local function vehicle(n,on,driver)
 local v={route={guid(n)},enabled=on,driver=driver}
 v.GetVehicleRoute=function(self)return self.route end
 v.IsAutopilotEnabled=function(self)return self.enabled end
 v.SetAutopilotEnabled=function(self,value)writes=writes+1;self.enabled=value end
 v.GetOwnerVehicle=function(self)return self end
 v.HasActiveDriver=function(self)return self.driver end
 return v
end
local depot=vehicle(1,true,true);local ordinary=vehicle(2,true,false);local inactive=vehicle(3,false,false)
local all={depot,ordinary,inactive}
local api={snapshot=true,vehicles=function()return all end,call=function(o,m,...)return o[m](o,...)end,
 authority=function()end,valid=function(o)return o~=nil end,log=function(s)logs[#logs+1]=s end}
api.snapshot=false;check(not pcall(Traffic.pause,api,home));check(writes==0)
api.snapshot=true;ordinary.driver=true;check(not pcall(Traffic.pause,api,home));check(writes==0 and depot.enabled)
ordinary.driver=false;depot.enabled=false;check(not pcall(Traffic.pause,api,home));check(writes==0)
depot.enabled=true;local old_route=ordinary.route
check(Traffic.pause(api,home)==1);check(writes==1 and depot.enabled and depot.driver)
check(not ordinary.enabled and not inactive.enabled);check(ordinary.route==old_route)
check(logs[1]:find('active=1 routes_unchanged=true snapshot_only=true',1,true))
check(Traffic.pause(api,home)==0 and writes==1)
all={depot,vehicle(1,true,false)};check(not pcall(Traffic.pause,api,home));check(writes==1)
print(checks..' snapshot traffic-isolation checks passed; native pause not tested')
