local root=arg[1];package.path=root..'/?.lua;'..package.path
local M=require('WorldReadiness');local now=0;local searches=0;local candidates={};local world={}
local function obj(authority)return {alive=true,HasAuthority=function()return authority end,GetWorld=function()return world end}end
local api={now=function()return now end,valid=function(o)return o~=nil and o.alive~=false end,call=function(o,m)return o[m](o)end,find=function()searches=searches+1;return candidates end}
local r=M.new(api);assert(r:ready()==nil and searches==1)
for i=1,100 do assert(r:ready()==nil)end;assert(searches==1)
local good=obj(true);candidates={obj(false),good};now=2;assert(r:ready()==good and searches==2)
assert(r:ready()==good and searches==2);good.alive=false;now=4;assert(r:ready()==nil)
good.alive=true;candidates={good,obj(true)};now=6;assert(r:ready()==nil)
candidates={good};r:reset();assert(r:ready()==good)
local malformed={alive=true};candidates={malformed,good};r:reset();assert(r:ready()==good)
api.find=function()error('world not initialized')end;r:reset();assert(r:ready()==nil)
print('World readiness checks passed: menu/load backoff, authority, cache invalidation, ambiguity and probe failures')
