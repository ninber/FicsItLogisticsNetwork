local root=assert(arg[1]);package.path=root..'/?.lua;'..package.path
local Retrofit=require('StationRetrofit')
local count=0;local function check(v)assert(v);count=count+1 end
local function obj(n)return {IsValid=function()return true end,GetFullName=function()return 'Class '..n end}end
local item,other=obj('/Item'),obj('/Other');local filters={};local occupied=false;local foreign=false;local failure=false;local busy=false
local inventory=obj('inventory');inventory.GetSizeLinear=function()return 2 end
inventory.GetInventoryStacks=function(_,out)if foreign then out[1]={Item={ItemClass=other},NumItems=1}end end
inventory.GetAllowedItemOnIndex=function(_,i)return filters[i+1]end
inventory.GetSlotSize=function()return 100 end
inventory.SetAllowedItemOnIndex=function(_,i,v)if failure and i==1 then failure=false;error('injected setter failure')end;filters[i+1]=v end
local building=obj('station');building.GetDockingStationResourceForm=function()return 1 end
building.GetDockedActor=function()return occupied and obj('truck')or nil end
building.GetInventory=function()return inventory end;building.GetIsInLoadMode=function()return false end
local id=obj('identifier');id.GetStation=function()return building end
id.GetPathNodeGUID=function()return {A=1,B=0,C=0,D=0}end;id.GetStationName=function()return 'Preserved station' end
local api={call=function(o,m,...)return o[m](o,...)end,valid=function(o)return o and o:IsValid()end,authority=function()end,busy=function()return busy end}
occupied=true;check(not pcall(Retrofit.apply,api,id,item));occupied=false
busy=true;check(not pcall(Retrofit.apply,api,id,item));busy=false
foreign=true;check(not pcall(Retrofit.apply,api,id,item));check(not filters[1] and not filters[2]);foreign=false
failure=true;check(not pcall(Retrofit.apply,api,id,item));check(not filters[1] and not filters[2])
local receipt=Retrofit.apply(api,id,item);check(filters[1]==item and filters[2]==item)
check(receipt.actor=='Class station' and receipt.name=='Preserved station' and receipt.load==false)
Retrofit.restore(api,receipt);check(not filters[1] and not filters[2])
print(count..' station retrofit checks passed; native filter enforcement/save reload NOT tested')
