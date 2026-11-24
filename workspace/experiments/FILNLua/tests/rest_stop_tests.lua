local root=assert(arg[1]);package.path=root..'/?.lua;'..package.path
local RestStop=require('RestStop')
local function g(n)return {A=n,B=0,C=0,D=0}end
local checks=0
local function check(value)assert(value);checks=checks+1 end
local function fails(fn)check(not pcall(fn))end
local enabled={V1=true,V2=true};local routes={V1={g(1),g(2)},V2={g(3),g(4)}}
local present={B1='V1',B2='V2'};local valid=true;local reject_write=false
local r=RestStop.new({
 arrived=function(b,v)return present[b]==v end,
 set_enabled=function(v,value)enabled[v]=value end,
 enabled=function(v)return enabled[v]end,
 route=function(v)return routes[v]end,
 set_route=function(v,value)if reject_write then return end;routes[v]=value end,
 validate=function()return valid end,
})
r:register('B1','V1');r:register('B2','V2')
fails(function()r:register('B1','V3')end)
fails(function()r:register('B3','V1')end)
fails(function()r:arrive('B1','V2')end)
local ticket=r:arrive('B1','V1');check(not enabled.V1);check(enabled.V2)
check(r.bays.B1.state=='Held')
fails(function()r:dispatch('B1',nil,{g(2),g(1)})end)
fails(function()r:dispatch('B1',ticket+1,{g(2),g(1)})end)
fails(function()r:dispatch('B1',ticket,{g(1),g(2)})end);check(not enabled.V1)
fails(function()r:dispatch('B1',ticket,{g(2),g(1)})end)
ticket=r:arrive('B1','V1');valid=false
fails(function()r:dispatch('B1',ticket,{g(2),g(1)})end);check(not enabled.V1)
valid=true;ticket=r:arrive('B1','V1');reject_write=true
fails(function()r:dispatch('B1',ticket,{g(2),g(1)})end);check(not enabled.V1)
reject_write=false;ticket=r:arrive('B1','V1');present.B1=nil
fails(function()r:dispatch('B1',ticket,{g(2),g(1)})end);check(not enabled.V1)
present.B1='V1';ticket=r:arrive('B1','V1')
r:dispatch('B1',ticket,{g(2),g(1)});check(enabled.V1);check(r.bays.B1.state=='Away')
fails(function()r:dispatch('B1',ticket,{g(1),g(2)})end)
local t2=r:arrive('B2','V2');check(not enabled.V2);check(enabled.V1)
check(t2~=ticket)
local broken=RestStop.new({
 arrived=function()return true end,set_enabled=function()error('native stop rejected')end,
 enabled=function()return true end,route=function()return routes.V1 end,
 set_route=function()error('must not write')end,validate=function()return true end,
})
broken:register('B','V')
fails(function()broken:arrive('B','V')end)
check(broken.bays.B.state=='HoldFailed')
fails(function()broken:dispatch('B',1,{g(8),g(9)})end)
print(checks..' rest-stop controller checks passed; building/arrival integration NOT tested')
