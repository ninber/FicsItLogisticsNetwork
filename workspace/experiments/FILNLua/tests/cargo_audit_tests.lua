local root=assert(arg[1]);package.path=root..'/?.lua;'..package.path
local Audit=require('CargoAudit')
local checks=0;local function check(v)assert(v);checks=checks+1 end
local function obj(n)return {IsValid=function()return true end,GetFullName=function()return 'Class '..n end}end
local a,b=obj('/Game/ItemA'),obj('/Game/ItemB');local stacks={};local filters={};local sizes={100,50}
local inv=obj('inventory');inv.GetInventoryStacks=function(_,out)for i,s in ipairs(stacks)do out[i]=s end end
inv.GetSizeLinear=function()return 2 end
inv.GetAllowedItemOnIndex=function(_,i)return filters[i+1]end
inv.GetSlotSize=function(_,i)return sizes[i+1]end
local api={call=function(o,m,...)return o[m](o,...)end,valid=function(o)return o and o:IsValid()end}
local function sample()return Audit.inventory(api,inv)end
local s=sample();check(not s.resource and s.quantity==0);check(s.capacity==0)
stacks={{get=function()return {Item={ItemClass=a},NumItems=12}end},{Item={ItemClass=a},NumItems=7}}
s=sample();check(s.item=='/Game/ItemA');check(s.quantity==19 and s.capacity==150)
stacks[#stacks+1]={Item={ItemClass=b},NumItems=1};s=sample();check(s.kinds==2 and not s.resource)
stacks={};filters={a,a};s=sample();check(s.item=='/Game/ItemA' and s.quantity==0);check(s.capacity==150)
filters={a,b};s=sample();check(s.capacity==100)
stacks={{Item={ItemClass=a},NumItems=-3}};check(not pcall(sample))
print(checks..' cargo inventory checks passed; native inventory APIs NOT tested')
