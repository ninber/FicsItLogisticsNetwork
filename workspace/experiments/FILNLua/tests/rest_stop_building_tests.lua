local root=assert(arg[1]);package.path=root..'/?.lua;'..package.path
local Building=require('RestStopBuilding')
local checks=0
local function check(v)assert(v);checks=checks+1 end
local function obj(name)return {IsValid=function()return true end,GetFullName=function()return name end}end
local objects={};local function find(path)if not objects[path]then objects[path]=obj(path)end;return objects[path]end
local prefix='/KDataForge/Gen/FILNRestStop.Gen_FILNRestStop_'
local recipe=find(prefix..'Recipe_RestStop');local descriptor=find('/KDataForge/Gen/FILNRestStop.Default__Gen_FILNRestStop_Desc_RestStop')
descriptor.mCategory=obj('category');descriptor.mSubCategories={obj('subcat')}
local manager=obj('manager');local bought,grants,listed,authority=false,0,true,true
manager.HasAuthority=function()return authority end
manager.IsSchematicPurchased=function()return bought end
manager.GiveAccessToSchematic=function()bought=true;grants=grants+1 end
local recipes=obj('recipes');recipes.IsRecipeAvailable=function()return true end
recipes.IsBuildingAvailable=function()return true end
recipes.GetAvailableRecipesForProducer=function(_,_,out)if listed then out[1]=recipe end end
find('/Script/FactoryGame.Default__FGRecipeManager').Get=function()return recipes end
local api={find=find,valid=function(v)return v and v:IsValid()end,call=function(o,m,...)return o[m](o,...)end}
check(Building.ensure(api,manager));check(grants==1)
check(Building.ensure(api,manager));check(grants==1)
listed=false;check(not pcall(Building.ensure,api,manager));listed=true
descriptor.mCategory=nil;check(not pcall(Building.ensure,api,manager));descriptor.mCategory=obj('category')
descriptor.mSubCategories={};check(not pcall(Building.ensure,api,manager));descriptor.mSubCategories={obj('subcat')}
authority=false;check(not pcall(Building.ensure,api,manager));check(grants==1)
_G.NotifyOnNewObject=function()end
_G.ExecuteInGameThread=function(fn)fn()end
_G.FindAllOf=function()return {}end
local registered=0
_G.LoopInGameThreadWithDelay=function(delay,fn)check(delay==1000);check(fn()==false);registered=registered+1 end
Building.watch(api);check(registered==1)
print(checks..' rest-stop unlock/menu checks passed; UI rendering NOT tested')
