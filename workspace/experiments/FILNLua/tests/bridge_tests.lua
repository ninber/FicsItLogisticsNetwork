local root=assert(arg[1]); package.path=root..'/?.lua;'..package.path
local Bridge=require('Bridge')
local checks=0
local function check(v) assert(v);checks=checks+1 end
local function obj(name)
 return {IsValid=function()return true end,GetFullName=function()return name end}
end
local cargo,fuel=obj('cargo'),obj('fuel')
local mode,empty,form,connected,inventory=true,true,1,false,cargo
local component=obj('component')
component.GetInventory=function()return inventory end
component.IsConnected=function()return connected end
local building=obj('building')
building.GetInventory=function()return cargo end
building.GetFuelInventory=function()return fuel end
building.GetIsInLoadMode=function()return mode end
building.GetDockingStationResourceForm=function()return form end
building.K2_GetComponentsByClass=function()return {component} end
cargo.IsEmpty=function()return empty end
local station=obj('station');station.GetStation=function()return building end
station.GetPathNodeGUID=function()return {A=1,B=0,C=0,D=0} end
local function call(o,n,...)return o[n](o,...)end
local function valid(o)return o and o:IsValid()end
local function audit()return Bridge.home_audit(station,call,valid,function()return obj('connection-class')end)end
check(audit().eligible)
connected=true
check(not audit().eligible)
inventory=fuel
check(audit().eligible)
inventory=nil
check(not audit().eligible)
building.mFuelConnections={component}
check(audit().eligible and audit().fuel_connections==1)
inventory=cargo;check(not audit().eligible)
building.mFuelConnections={};inventory=nil
check(not audit().eligible)
inventory=cargo; connected=false; mode=false
check(not audit().eligible)
mode=true;empty=false
check(not audit().eligible)
empty=true;form=2
check(not audit().eligible)
form=1
local resolve=function()error('missing reflected class')end
check(not Bridge.home_audit(station,call,valid,resolve).eligible)
-- Bridge array values may wrap parameters even when the array itself is a Lua table.
local x=Bridge.values({{get=function()return {A=-1,B=0,C=0,D=0}end}},require('Core').guid)
check(x[1].A==4294967295)
print(checks..' bridge/home checks passed')
