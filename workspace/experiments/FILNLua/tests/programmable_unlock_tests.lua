local root=assert(arg[1]);package.path=root..'/?.lua;'..package.path
local U=require('ProgrammableUnlock');local n=0;local function check(v)assert(v);n=n+1 end
local function obj(name)return {IsValid=function()return true end,GetFullName=function()return name end}end
local objects={};local prefix='/TruckStationSettings/'
for _,name in ipairs({'Schematic_SmarterTrucks','Recipe_ProgrammableTruckStation','Build_ProgrammableTruckStation','Desc_ProgrammableTruckStation'})do local path=prefix..name..'.'..name..'_C';objects[path]=obj(path)end
local recipe=objects[prefix..'Recipe_ProgrammableTruckStation.Recipe_ProgrammableTruckStation_C']
local descriptor=obj('descriptor');descriptor.mCategory=obj('logistics');descriptor.mSubCategories={obj('stations')}
objects[prefix..'Desc_ProgrammableTruckStation.Default__Desc_ProgrammableTruckStation_C']=descriptor
local product={ItemClass=objects[prefix..'Desc_ProgrammableTruckStation.Desc_ProgrammableTruckStation_C'],Amount=0}
objects[prefix..'Recipe_ProgrammableTruckStation.Default__Recipe_ProgrammableTruckStation_C']={IsValid=function()return true end,mProduct={product}}
local purchased,grants,listed,authority=false,0,true,true
local manager=obj('manager');manager.HasAuthority=function()return authority end;manager.IsSchematicPurchased=function()return purchased end;manager.GiveAccessToSchematic=function()purchased=true;grants=grants+1 end
local recipes=obj('recipes');recipes.IsRecipeAvailable=function()return purchased end;recipes.IsBuildingAvailable=function()return purchased end;recipes.GetAvailableRecipesForProducer=function(_,_,out)if listed then out[1]=recipe end end
objects['/Script/FactoryGame.Default__FGRecipeManager']={Get=function()return recipes end}
local api={valid=function(o)return o and o.IsValid and o:IsValid()end,find=function(path)return objects[path]end,call=function(o,m,...)return o[m](o,...)end,log=function()end}
check(U.ensure(api,manager));check(product.Amount==1);check(grants==1);check(U.ensure(api,manager));check(grants==1)
listed=false;check(not pcall(U.ensure,api,manager));listed=true
authority=false;check(not pcall(U.ensure,api,manager));authority=true
descriptor.mCategory=nil;check(not pcall(U.ensure,api,manager));descriptor.mCategory=obj('category')
local path=prefix..'Build_ProgrammableTruckStation.Build_ProgrammableTruckStation_C';local actor=objects[path];objects[path]=nil
check(not pcall(U.ensure,api,manager));api.load=function(p)objects[p]=actor;return actor end;check(U.ensure(api,manager))
local callback;_G.NotifyOnNewObject=function(_,fn)callback=fn end;_G.ExecuteInGameThread=function(fn)fn()end;_G.FindAllOf=function()return {manager}end;local loop;_G.LoopInGameThreadWithDelay=function(_,fn)loop=fn end
U.watch(api);check(loop()==false);local before=grants;check(loop()==false);check(grants==before);purchased=false;callback(manager);check(loop()==false);check(grants==before+1)
print(n..' programmable unlock checks passed; native availability/UI not tested')
