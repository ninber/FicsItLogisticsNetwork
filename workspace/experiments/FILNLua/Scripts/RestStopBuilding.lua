local Bridge=require('Bridge')
local M={}
local prefix='/KDataForge/Gen/FILNRestStop.Gen_FILNRestStop_'
function M.ensure(api,manager)
    local call,valid,find=api.call,api.valid,api.find
    assert(valid(manager) and call(manager,'HasAuthority'),'authoritative schematic manager required')
    local recipe,schematic=find(prefix..'Recipe_RestStop'),find(prefix..'Schematic_RestStop')
    local actor=find('/Game/FactoryGame/Buildable/Factory/TruckStation/Build_TruckStation.Build_TruckStation_C')
    assert(valid(actor) and valid(recipe) and valid(schematic),'rest-stop pack unavailable')
    local descriptor=find('/KDataForge/Gen/FILNRestStop.Default__Gen_FILNRestStop_Desc_RestStop')
    assert(valid(descriptor) and valid(descriptor.mCategory),'rest-stop build category unavailable')
    assert(#Bridge.values(descriptor.mSubCategories)>0,'rest-stop build subcategory unavailable')
    if not call(manager,'IsSchematicPurchased',schematic,nil) then
        call(manager,'GiveAccessToSchematic',schematic,nil,0)
    end
    assert(call(manager,'IsSchematicPurchased',schematic,nil),'rest-stop unlock failed')
    local recipes=call(find('/Script/FactoryGame.Default__FGRecipeManager'),'Get',manager)
    assert(call(recipes,'IsRecipeAvailable',recipe),'rest-stop recipe unavailable')
    assert(call(recipes,'IsBuildingAvailable',actor),'rest-stop building unavailable')
    local gun=find('/Game/FactoryGame/Equipment/BuildGun/BP_BuildGun.BP_BuildGun_C')
    assert(valid(gun),'build gun class unavailable')
    local out={};call(recipes,'GetAvailableRecipesForProducer',gun,out)
    local listed=false
    for _,entry in ipairs(Bridge.values(out)) do
        if Bridge.same_object(entry,recipe,valid) then listed=true;break end
    end
    assert(listed,'rest-stop missing from Build Gun recipe list')
    return true
end
function M.watch(api)
    local candidate,attempts,pending,done=nil,0,false,false
    local function observe(object)
        candidate=Bridge.value(object); attempts=0;done=false
    end
    NotifyOnNewObject('/Script/FactoryGame.FGSchematicManager',observe)
    -- One startup enumeration for late injection; subsequent worlds use construction notifications.
    ExecuteInGameThread(function()
        for _,manager in ipairs(FindAllOf('FGSchematicManager') or {})do
            if api.valid(manager) and api.call(manager,'HasAuthority') then observe(manager);break end
        end
    end)
    local game_loop=LoopInGameThreadWithDelay or LoopAsync
    game_loop(1000,function()
        if pending or done or not candidate or attempts>=60 then return false end
        pending=true
        ExecuteInGameThread(function()
            attempts=attempts+1
            local ok,why=pcall(M.ensure,api,candidate)
            pending=false;done=ok
            if ok then api.log('REST_STOP_READY unlocked=true buildgun_listed=true; player places building and roads')
            elseif attempts==1 or attempts==60 then api.log('rest-stop setup pending: '..tostring(why)) end
        end)
        return false
    end)
end
return M
