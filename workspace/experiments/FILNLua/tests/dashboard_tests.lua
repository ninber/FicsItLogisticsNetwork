local root=assert(arg[1]);package.path=root..'/?.lua;'..package.path
local n=0;local function check(v)assert(v);n=n+1 end
local Json=require('Json')
check(Json.encode({empty=Json.array(),object={},s='"\\\n\0'})=='{"empty":[],"object":{},"s":"\\\"\\\\\\n\\u0000"}')
local cyclic={};cyclic.x=cyclic;check(not pcall(Json.encode,cyclic));check(not pcall(Json.encode,math.huge))
local path=os.tmpname();Json.write(path,{ok=true});local f=assert(io.open(path));check(f:read('*a')=='{"ok":true}');f:close();os.remove(path)
local Dashboard=require('Dashboard');local opens=0;local stamp=100
local messages={}
local api={now=function()return stamp end,valid=function()error('native API forbidden')end,find=function()error('native API forbidden')end,
 call=function()opens=opens+1;error('native browser call forbidden')end,log=function(message)messages[#messages+1]=message end}
check(not pcall(Dashboard.open,api,path));check(opens==0)
f=assert(io.open(path,'w'));f:write('100');f:close();check(Dashboard.open(api,path)==false);check(opens==0)
check(messages[1]:find('native_browser_disabled=true',1,true)~=nil)
check(messages[1]:find('http://filn.localhost:8765/',1,true)~=nil)
stamp=121;check(not pcall(Dashboard.open,api,path));check(opens==0);os.remove(path)
-- Stub native audit only: verify demand gating, one sample per pass, snapshot safety and world reset.
local saved=package.loaded.CargoAudit;local samples=0
local function object(name)return {GetFullName=function()return name end}end
local world=object('World city');local context=object('Context');local building=object('Station');local identifier=object('Identifier')
package.loaded.CargoAudit={station=function()
 samples=samples+1;return {building=building,items={},guid='home',label='Home',role='provider',configured=false,depot=true,quantity=0,capacity=0}
 end,inventory=function()return {items={},capacity=0,slots=1}end}
package.loaded.Telemetry=nil
local calls={};local runtime={jobs={},reservations={}};local scheduler={enabled=true,reasons={},paths={},stations={},homes={}}
local collector=require('Telemetry').new({now=function()return stamp end,demand_path=path..'.demand',output_path=path,progress_path=path..'.progress',
 lifecycle_path=path..'.mod',context=function()return context end,valid=function(o)return o~=nil end,refresh=function()calls.refresh=(calls.refresh or 0)+1 end,
 stations=function()return {S1=identifier,S2=identifier}end,vehicles=function()return {}end,runtime=function()return runtime end,scheduler=function()return scheduler end,
 call=function(o,m)
  if m=='GetWorld'then return world elseif m=='GetDockedActor'then return nil elseif m=='HasPower'then return true
  elseif m=='IsProductionPaused'then return false elseif m=='K2_GetActorLocation'then return {X=100,Y=200,Z=300}
  elseif m=='GetFuelInventory'then return {} elseif m=='GetDockingPathNode'then return {} elseif m=='GetArrivingConnections'or m=='GetLeavingConnections'then return {{}}end;error('unexpected '..m)
 end})
collector:step();check(samples==0 and not calls.refresh)
f=assert(io.open(path..'.mod'));check(f:read('*a')==tostring(stamp));f:close()
collector:request();collector:step();check(samples==1);collector:step();check(samples==2);collector:step()
check(collector.sequence==1);f=assert(io.open(path));local result=f:read('*a');f:close();check(result:find('"stations":%[')~=nil);check(result:find('"errors":%[%]')~=nil)
check(result:find('"incoming":1')~=nil and result:find('"outgoing":1')~=nil)
local waiting=require('Telemetry').new({now=function()return stamp end,ready=function()return false end,refresh=function()error('menu must not scan world')end,output_path=path})
waiting:request();waiting:step();check(waiting.cycle==nil)
collector:reset();check(not collector.cycle);stamp=1000;collector:step();check(samples==2)
for _,suffix in ipairs({'','.progress','.demand','.mod'})do os.remove(path..suffix)end
package.loaded.CargoAudit=saved;package.loaded.Telemetry=nil
print(n..' dashboard checks passed: serialization, native browser calls disabled, bounded demand-driven sampling; no native UI tested')
