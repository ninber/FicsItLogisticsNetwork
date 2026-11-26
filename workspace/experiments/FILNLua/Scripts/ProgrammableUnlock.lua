local Bridge=require('Bridge')
local M={}
local prefix='/TruckStationSettings/'
local function asset(api,name)
    local path=prefix..name..'.'..name..'_C'
    local result=api.find(path)
    if not api.valid(result) and api.load then
        result=api.load(path)
        if not api.valid(result) then
            api.load(prefix..name..'.'..name)
            result=api.find(path)
        end
    end
    assert(api.valid(result),'Smarter Truck Stations asset unavailable: '..path)
    return result
end
function M.ensure(api,manager)
    assert(api.valid(manager) and api.call(manager,'HasAuthority'),'host schematic manager required')
    local schematic=asset(api,'Schematic_SmarterTrucks')
    local recipe=asset(api,'Recipe_ProgrammableTruckStation')
    local actor=asset(api,'Build_ProgrammableTruckStation')
    local descriptor=asset(api,'Desc_ProgrammableTruckStation')
    local recipe_cdo=api.find(prefix..'Recipe_ProgrammableTruckStation.Default__Recipe_ProgrammableTruckStation_C')
    assert(api.valid(recipe_cdo),'programmable recipe defaults unavailable')
    local products=Bridge.values(recipe_cdo.mProduct)
    assert(#products==1 and Bridge.same_object(products[1].ItemClass,descriptor,api.valid),'unexpected programmable recipe product')
    if products[1].Amount==0 then
        products[1].Amount=1
        api.log('PROGRAMMABLE_RECIPE_REPAIRED output=1 previous=0')
    end
    local verified=Bridge.values(recipe_cdo.mProduct)
    assert(#verified==1 and verified[1].Amount==1,'programmable recipe must produce one building')
    local recipes=api.call(api.find('/Script/FactoryGame.Default__FGRecipeManager'),'Get',manager)
    local purchased=api.call(manager,'IsSchematicPurchased',schematic,nil)
    api.log('PROGRAMMABLE_DIAG schematic_purchased='..tostring(purchased)..' recipe_available='..tostring(api.call(recipes,'IsRecipeAvailable',recipe)))
    if not purchased then api.call(manager,'GiveAccessToSchematic',schematic,nil,0)end
    assert(api.call(manager,'IsSchematicPurchased',schematic,nil),'programmable schematic unlock failed')
    assert(api.call(recipes,'IsRecipeAvailable',recipe),'programmable recipe unlock failed')
    assert(api.call(recipes,'IsBuildingAvailable',actor),'programmable building unavailable')
    local gun=api.find('/Game/FactoryGame/Equipment/BuildGun/BP_BuildGun.BP_BuildGun_C')
    local out={};api.call(recipes,'GetAvailableRecipesForProducer',gun,out)
    local listed=false
    for _,entry in ipairs(Bridge.values(out))do
        if Bridge.same_object(entry,recipe,api.valid)then listed=true;break end
    end
    assert(listed,'programmable recipe not listed for Build Gun')
    local cdo=api.find(prefix..'Desc_ProgrammableTruckStation.Default__Desc_ProgrammableTruckStation_C')
    assert(api.valid(cdo) and api.valid(cdo.mCategory),'programmable descriptor has no build category')
    assert(#Bridge.values(cdo.mSubCategories)>0,'programmable descriptor has no build subcategory')
    local filn=api.find('/KDataForge/Gen/FILNStations.Gen_FILNStations_Schematic_LogisticsStation')
    if api.valid(filn) then
        if not api.call(manager,'IsSchematicPurchased',filn,nil) then api.call(manager,'GiveAccessToSchematic',filn,nil,0)end
        local filn_recipe=api.find('/KDataForge/Gen/FILNStations.Gen_FILNStations_Recipe_LogisticsStation')
        assert(api.valid(filn_recipe) and api.call(recipes,'IsRecipeAvailable',filn_recipe),'FILN station recipe unavailable')
        api.log('FILN_STATION_READY recipe_available=true actor=programmable native_filter_widget=true')
    end
    api.log('PROGRAMMABLE_READY purchased=true recipe=true building=true buildgun_listed=true category='..cdo.mCategory:GetFullName())
    return true
end
function M.watch(api)
    local candidate,attempts,done,pending=nil,0,false,false
    local function observe(object)candidate=Bridge.value(object);attempts=0;done=false end
    NotifyOnNewObject('/Script/FactoryGame.FGSchematicManager',observe)
    ExecuteInGameThread(function()
        for _,manager in ipairs(FindAllOf('FGSchematicManager')or {})do
            if api.valid(manager) and api.call(manager,'HasAuthority')then observe(manager);break end
        end
    end)
    local game_loop=LoopInGameThreadWithDelay or LoopAsync
    game_loop(1000,function()
        if pending or done or not candidate or attempts>=60 then return false end
        pending=true
        ExecuteInGameThread(function()
            attempts=attempts+1
            local ok,why=pcall(M.ensure,api,candidate)
            done=ok;pending=false
            if not ok and (attempts==1 or attempts==60)then api.log('PROGRAMMABLE_PENDING '..tostring(why))end
        end)
        return false
    end)
end
return M
