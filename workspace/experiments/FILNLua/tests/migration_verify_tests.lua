local root=assert(arg[1]);package.path=root..'/?.lua;'..package.path
local M=require('MigrationVerify');local Core=require('Core');local n=0;local function check(v)assert(v);n=n+1 end
local function obj(name)return {GetFullName=function()return name end,IsValid=function()return true end}end
local guid={A=1,B=2,C=3,D=4};local recipe=obj('recipe');local item=obj('Class /Game/Item.Item_C')
local node=obj('Class /Game/Map.node');node.GetPathNodeGUID=function()return guid end;node.GetArrivingConnections=function()return {{}}end;node.GetLeavingConnections=function()return {{}}end
local inv=obj('Inventory /Game/Map.inventory');local b=obj('station');b.mBuiltWithRecipe=recipe;b.FilterItem={item};b.GetClass=function()return obj('Class /Mod/Build.Build_ProgrammableTruckStation_C')end;b.GetIsInLoadMode=function()return false end;b.GetDockingPathNode=function()return node end;b.GetInventory=function()return inv end
local id={GetStation=function()return b end};local api={call=function(o,m,...)return o[m](o,...)end,valid=function(o)return o and o.IsValid and o:IsValid()end,find=function()return recipe end}
local entry={guid=Core.guid_key(guid),mode='UNLOAD',filters='/Game/Item.Item_C',inventory='Map.inventory',node='Map.node',incoming=1,outgoing=1}
check(M.verify(api,entry,id));entry.incoming=0;check(not pcall(M.verify,api,entry,id));entry.incoming=1
entry.mode='LOAD';check(not pcall(M.verify,api,entry,id));entry.mode='UNLOAD'
b.FilterItem={};check(not pcall(M.verify,api,entry,id));b.FilterItem={item}
entry.inventory='other';check(not pcall(M.verify,api,entry,id))
print(n..' migration verifier checks passed: class/filter/identity/role/road failures block release')
