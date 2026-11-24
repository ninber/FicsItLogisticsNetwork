local root=assert(arg[1]);package.path=root..'/?.lua;'..package.path
local Seed=require('TestFuel');local checks=0;local function check(v)assert(v);checks=checks+1 end
local writes=0;local driver=false;local amounts={0,9};local vehicles={};local coal={};local logs={}
for i=1,2 do local index=i;local fuel={}
 fuel.IsEmpty=function()return amounts[index]==0 end
 fuel.AddStack=function(_,stack,partial)check(stack.Item.ItemClass==coal and stack.NumItems==100 and partial==false);writes=writes+1;amounts[index]=100;return 100 end
 fuel.GetNumItems=function()return amounts[index]end
 local owner={GetFuelInventory=function()return fuel end,HasActiveDriver=function()return driver end}
 vehicles['home'..i]={GetOwnerVehicle=function()return owner end}
end
local api={snapshot=false,authority=function()end,valid=function(o)return o~=nil end,call=function(o,m,...)return o[m](o,...)end,log=function(s)logs[#logs+1]=s end}
check(not pcall(Seed.seed,api,vehicles,coal));check(writes==0)
api.snapshot=true;driver=true;check(not pcall(Seed.seed,api,vehicles,coal));check(writes==0)
driver=false;check(Seed.seed(api,vehicles,coal)==1);check(writes==1 and amounts[1]==100 and amounts[2]==9)
check(Seed.seed(api,vehicles,coal)==0 and writes==1);check(logs[1]:find('snapshot_only=true',1,true))
print(checks..' starter-fuel fixture checks passed; native AddStack not tested')
