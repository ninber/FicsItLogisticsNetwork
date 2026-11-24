local root = assert(arg[1], 'pass Scripts directory')
package.path = root..'/?.lua;'..package.path
local Core=require('Core')
local checks=0
local function check(value) assert(value); checks=checks+1 end
local function rejects(f) check(not pcall(f)) end
local g=Core.guid({A=1,B=2,C=3,D=4})
check(Core.guid_key(g)=='00000001000000020000000300000004')
rejects(function() Core.guid({A=0,B=0,C=0,D=0}) end)
check(Core.guid({A=-1,B=0,C=0,D=0}).A==4294967295)
check(Core.guid({A=-2147483648,B=0,C=0,D=0}).A==2147483648)
rejects(function() Core.guid({A=-2147483649,B=0,C=0,D=0}) end)
rejects(function() Core.guid({A=4294967296,B=0,C=0,D=0}) end)
check(Core.same_route({g},{g}))
check(not Core.same_route({g},{}))
local r=Core.new()
check(r:pair('v1','h1').state=='Idle')
check(r:pair('v1','h1').home=='h1')
rejects(function() r:pair('v2','h1') end)
rejects(function() r:pair('v1','h2') end)
r:pair('v2','h2')
r:reserve('v1','p1','q1')
rejects(function() r:reserve('v1','p2','q2') end)
rejects(function() r:reserve('v2','p1','q2') end)
check(r.slots.q2==nil)
rejects(function() r:reserve('v2','q1','q2') end)
rejects(function() r:reserve('v2','h1','q2') end)
r:recover('v1','reload')
check(r.slots.p1=='v1' and r.pairs.v1.state=='Recovery')
rejects(function() r:returned('v1',false) end)
r:returned('v1',true)
check(r.slots.p1==nil and r.pairs.v1.home=='h1')
r:reserve('v2','p1','q2')
check(Core.advance('ToProvider','Provider')=='Loading')
check(Core.advance('Loading','Road')=='ToRequester')
check(Core.advance('ToRequester','Requester')=='Unloading')
check(Core.advance('Unloading','Road')=='Returning')
check(Core.advance('Returning','Home')=='HomeArrived')
check(Core.advance('ToRequester','Home')=='Recovery')
for _,n in ipairs({100,500,1000}) do
    local fleet=Core.new()
    local started=os.clock()
    for i=1,n do fleet:pair('v'..i,'h'..i) end
    for i=1,n do
        local slot=(i-1)%150
        local ok=pcall(function() fleet:reserve('v'..i,'p'..slot,'q'..slot) end)
        check(ok==(i<=150))
    end
    print(string.format('Lua reservation exercise: vehicles=%d station-slots=300 elapsed_ms=%.3f',n,(os.clock()-started)*1000))
end
-- Load the actual bridge with a mocked UE4SS host: startup must never mutate game routes.
local registered,queued,tick={},{},nil
local keybinds={}
_G.Key={J='J'}
_G.ModifierKey={CONTROL='CONTROL',SHIFT='SHIFT'}
_G.RegisterKeyBind=function(key,modifiers,fn) check(modifiers[1]=='CONTROL'); keybinds[key]=fn end
_G.RegisterConsoleCommandHandler=function() error('viewport-only handler must not be used') end
_G.RegisterConsoleCommandGlobalHandler=function(name,fn) registered[name]=fn end
_G.ExecuteInGameThread=function(fn) queued[#queued+1]=fn end
_G.LoopAsync=function(delay,fn) check(delay==1000); check(fn()==false); tick=fn end
_G.FindAllOf=function() error('startup must not scan') end
local oldprint=print
local logs={}; _G.print=function(msg) logs[#logs+1]=msg end
package.preload.AutoTestRun=function() return 'test-session-token' end
local original_open=io.open
local inbox='ping\n' -- historical commands must never replay on script startup
io.open=function(path,mode)
    if path:match('[/\\]commands%.txt$') then
        local position=0
        return {
            seek=function(_,whence,offset)
                if whence=='end' then position=#inbox else position=offset end
                return position
            end,
            read=function(_,size) local data=inbox:sub(position+1,position+size); position=position+#data; return data end,
            close=function() end,
        }
    end
    return original_open(path,mode)
end
dofile(root..'/main.lua')
check(#queued==0)
check(logs[#logs]:find('AUTOTEST_SESSION test-session-token',1,true)~=nil)
check(type(registered['filn_lua.scan'])=='function')
check(type(registered['filn_lua.apply'])=='function')
check(type(registered['filn_lua.ping'])=='function')
local console_output={}
local output={Log=function(_,message) console_output[#console_output+1]=message end}
check(registered['filn_lua.ping']('',{},output)==true)
check(#console_output==1 and console_output[1]:find('received ping',1,true)~=nil)
table.remove(queued,1)()
check(logs[#logs]:find('ping OK',1,true)~=nil)
registered['filn_lua.apply']('',{'V1'})
check(#queued==1)
queued[1]()
check(logs[#logs]:find('route writes disabled',1,true)~=nil)
queued={}
-- Integration-shaped mock: exercises real main.lua commands and state handling, not UE marshaling.
local function object(name)
    return {IsValid=function() return true end,GetFullName=function() return name end}
end
local raw_arrays='wrapped'
local function array(items)
    if raw_arrays=='wrapped' then
        local out={}
        for i,item in ipairs(items) do out[i]={get=function() return item end} end
        return out
    end
    if raw_arrays then return items end
    return {ForEach=function(_,fn)
        for i,item in ipairs(items) do fn(i,{get=function() return item end}) end
    end}
end
local world=object('test-world')
local inventory=object('inventory'); inventory.IsEmpty=function() return true end
local owner=object('owner')
owner.GetStorageInventory=function() return inventory end
owner.GetVehiclePathPreset=function() return object('preset') end
local v=object('truck'); local autopilot,route_writes=false,0
local original={g,{A=2,B=3,C=4,D=5}}; local current_route=original
local fail_next_write=false
v.HasAuthority=function() return true end
v.GetWorld=function() return world end
v.GetOwnerVehicle=function() return owner end
v.IsAutopilotEnabled=function() return autopilot end
v.SetAutopilotEnabled=function(_,enabled) autopilot=enabled end
v.CanEnableAutopilot=function() return true end
v.GetAutopilotErrorStatus=function() return 0 end
v.GetCurrentTargetWaypointIndex=function() return 0 end
v.IsCurrentlyDocking=function() return false end
v.GetVehicleRoute=function() return array(current_route) end
v.SetVehicleRoute=function(_,newroute)
    if fail_next_write then fail_next_write=false;error('injected route write failure')end
    route_writes=route_writes+1; current_route=newroute
end
local home,p,q=object('home-building'),object('provider-building'),object('requester-building')
local docked=home
local requester_load=false
for _,b in ipairs({home,p,q}) do
    b.GetInventory=function() return inventory end
    b.GetDockedActor=function(self) return docked==self and owner or nil end
    b.GetIsInLoadMode=function(self) return self~=q or requester_load end
    b.GetFuelInventory=function() return inventory end
    b.GetDockingStationResourceForm=function() return 1 end
    b.K2_GetComponentsByClass=function() return array({}) end
end
local save_session=object('save-session');save_session.mAutosaveEnabled=true
save_session.SetAutoSaveEnabled=function(self,value)self.mAutosaveEnabled=value end
local save_default=object('save-default');save_default.Get=function()return save_session end
_G.StaticFindObject=function(path)
    if path=='/Script/FactoryGame.Default__FGSaveSession' then return save_default end
    return object('component-class')
end
local stations={}
for i,b in ipairs({home,p,q}) do
    local s=object('station'..i)
    s.GetStation=function() return b end
    s.GetStationName=function()return 'Station '..i end
    s.GetPathNodeGUID=function() return {A=i,B=0,C=0,D=0} end
    stations[i]=s
end
local disconnected=object('station4')
disconnected.GetPathNodeGUID=function() return {A=0,B=0,C=0,D=0} end
disconnected.GetStation=function()return nil end
disconnected.GetStationName=function()return 'Disconnected' end
stations[#stations+1]=disconnected
local reachable=true
local network=object('network'); network.FindVehiclePath=function() return reachable end
local subsystem=object('subsystem')
subsystem.GetWorld=function() return world end
subsystem.HasAuthority=function() return true end
subsystem.GetAllVehicles=function() return array({v}) end
subsystem.GetDockingStations=function() return array(stations) end
subsystem.FindNetworkByPathNodeGuid=function() return network end
_G.FindAllOf=function(name) check(name=='FGVehicleSubsystem'); return {subsystem} end
local function invoke(name,args)
    registered['filn_lua.'..name]('',args or {})
    check(#queued==1)
    table.remove(queued,1)()
    return logs[#logs]
end
local function pulse()
    tick()
    if #queued>0 then table.remove(queued,1)() end
end
invoke('scan')
check(logs[#logs]:find('scan complete',1,true)~=nil)
local reported_unavailable=false
for _,line in ipairs(logs) do if line:find('S4',1,true) and line:find('unavailable',1,true) then reported_unavailable=true end end
check(reported_unavailable)
check(route_writes==0)
check(invoke('pair',{'V1','S1'}):find('paired',1,true)~=nil)
check(invoke('scan'):find('cannot rescan',1,true)~=nil)
reachable=false
check(invoke('plan',{'V1','S2','S3'}):find('unreachable directed leg',1,true)~=nil)
check(route_writes==0)
reachable=true
check(invoke('plan',{'V1','S2','S3'}):find('validated',1,true)~=nil)
check(invoke('plan',{'V1','S2','S2'}):find('three distinct',1,true)~=nil)
require('Config').AllowRouteWrites=true
check(invoke('apply',{'V1'}):find('plan first',1,true)~=nil)
invoke('plan',{'V1','S2','S3'})
require('Config').AllowRouteWrites=true -- only in this isolated mocked host
check(invoke('apply',{'V1'}):find('route applied',1,true)~=nil)
check(route_writes==1 and not autopilot)
check(invoke('apply',{'V1'}):find('already assigned',1,true)~=nil)
requester_load=true
check(invoke('start',{'V1'}):find('station modes changed',1,true)~=nil)
check(not autopilot)
requester_load=false;invoke('start',{'V1'})
check(autopilot)
docked=p; pulse()
check(invoke('status'):find('Loading',1,true)~=nil)
docked=nil; pulse()
check(invoke('status'):find('ToRequester',1,true)~=nil)
docked=q; pulse()
check(invoke('status'):find('Unloading',1,true)~=nil)
docked=nil; pulse()
check(invoke('status'):find('Returning',1,true)~=nil)
docked=home; pulse()
check(not autopilot)
check(invoke('status'):find('Idle',1,true)~=nil)
invoke('restore',{'V1'})
check(Core.same_route(current_route,original) and not autopilot)
raw_arrays=false -- repeat command/route flow with remote TArray-style wrappers
-- A second departure, interrupted by operator stop, keeps recovery until physical home reset.
invoke('plan',{'V1','S2','S3'}); invoke('apply',{'V1'}); invoke('start',{'V1'})
docked=nil
invoke('stop',{'V1'})
check(not autopilot and invoke('status'):find('Recovery',1,true)~=nil)
check(invoke('reset',{'V1'}):find('must return to home',1,true)~=nil)
docked=home
check(invoke('reset',{'V1'}):find('reset at home',1,true)~=nil)
raw_arrays=true -- direct values also remain supported
check(invoke('inspect',{'V1'}):find('stop ',1,true)~=nil)
check(invoke('stations'):find('HOME_FIXTURES eligible=',1,true)~=nil)
check(invoke('autotest_route_bridge'):find('autosave preparation required',1,true)~=nil)
invoke('autotest_prepare');check(save_session.mAutosaveEnabled==false)
check(invoke('autotest_route_bridge'):find('not enabled',1,true)~=nil)
require('Config').AutoTestAllowRouteBridge=true
local before_bridge=route_writes;local saved_route=current_route
check(invoke('autotest_route_bridge'):find('BRIDGE_PROOF V1 PASS',1,true)~=nil)
check(route_writes==before_bridge+2 and Core.same_route(current_route,saved_route) and not autopilot)
fail_next_write=true
check(invoke('autotest_route_bridge'):find('original route restored',1,true)~=nil)
check(Core.same_route(current_route,saved_route) and not autopilot)
keybinds.J(); check(#queued==1);check(logs[#logs]:find('received dashboard via Ctrl+Shift+J',1,true)~=nil);table.remove(queued,1)()
check(keybinds.F8==nil and keybinds.F9==nil and keybinds.F10==nil)
inbox=inbox..'ping\n'
tick()
while #queued>0 do table.remove(queued,1)() end
check(logs[#logs]:find('ping OK',1,true)~=nil)
tick(); check(#queued==0) -- consumed inbox line does not replay
local before=route_writes
inbox=inbox..'not_a_command\n'
tick(); check(#queued==0 and route_writes==before)
inbox='scan\n' -- truncated inbox is skipped rather than replayed
tick(); check(#queued==0)
inbox=inbox..'@test-pass ping\n'
tick(); while #queued>0 do table.remove(queued,1)() end
check(logs[#logs]:find('AUTOTEST_RESULT test-pass PASS ping',1,true)~=nil)
inbox=inbox..'@test-fail inspect V999\n'
tick(); while #queued>0 do table.remove(queued,1)() end
check(logs[#logs]:find('AUTOTEST_RESULT test-fail FAIL inspect',1,true)~=nil)
io.open=original_open
_G.print=oldprint
print(checks..' host Lua checks passed; UE4SS/game calls are NOT tested')
