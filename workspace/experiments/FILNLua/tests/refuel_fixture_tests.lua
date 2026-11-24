local root=assert(arg[1]);package.path=root..'/?.lua;'..package.path
local Fixture=require('RefuelFixture');local n=0;local function check(v)assert(v);n=n+1 end
local source,destination=100,0;local driver=false;local writes=0;local coal={}
local s={GetNumItems=function()return source end,RemoveFromIndex=function(_,index,amount,target)check(index==0 and target~=nil);source=source-amount;writes=writes+1 end}
local d={IsEmpty=function()return destination==0 end,GetNumItems=function()return destination end,AddStack=function(_,stack,partial)check(stack.Item.ItemClass==coal and not partial);destination=destination+stack.NumItems;writes=writes+1;return stack.NumItems end}
local plan={{home='home',owner={HasActiveDriver=function()return driver end,GetFuelInventory=function()return s end},building={GetFuelInventory=function()return d end}}}
local api={snapshot=false,valid=function(o)return o~=nil end,call=function(o,m,...)return o[m](o,...)end,log=function()end}
check(not pcall(Fixture.prepare,api,plan,coal));check(writes==0)
api.snapshot=true;driver=true;check(not pcall(Fixture.prepare,api,plan,coal));check(writes==0)
driver=false;check(Fixture.prepare(api,plan,coal)==1);check(source==1 and destination==99 and source+destination==100)
check(not pcall(Fixture.prepare,api,plan,coal));check(writes==2)
print(n..' conserved-fuel fixture checks passed; native transfer not tested')
