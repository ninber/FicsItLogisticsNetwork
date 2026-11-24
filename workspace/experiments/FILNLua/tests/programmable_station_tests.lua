local root=assert(arg[1]);package.path=root..'/?.lua;'..package.path
local P=require('ProgrammableStation');local n=0
local function check(v)assert(v);n=n+1 end
local function item(name)return {IsValid=function()return true end,GetFullName=function()return name end}end
local a,b=item('ingot'),item('rod');local name='Build_ProgrammableTruckStation_C'
local station={FilterItem={},GetClass=function()return {GetFullName=function()return 'BlueprintGeneratedClass /TruckStationSettings/Build.'..name end}end}
local api={valid=function(o)return o and o.IsValid and o:IsValid()end}
check(P.item(api,station)==nil)
station.FilterItem={a};check(P.item(api,station)==a)
station.FilterItem={a,a};check(P.item(api,station)==a)
station.FilterItem={a,b};check(P.item(api,station)==nil)
station.FilterItem={a};name='Build_TruckStation_C';check(P.item(api,station)==nil)
name='Build_ProgrammableTruckStation_C';station.FilterItem={ForEach=function(_,fn)fn(0,{get=function()return b end})end};check(P.item(api,station)==b)
print(n..' programmable station in-game filter checks passed; native UI not tested')
