local Bridge=require('Bridge')
local M={}
local function purchased(api,manager)
    local out={};api.call(manager,'GetAllPurchasedSchematics',out)
    local keys={}
    for _,entry in ipairs(Bridge.values(out))do keys[#keys+1]=entry:GetFullName()end
    table.sort(keys);return table.concat(keys,'\n'),#keys
end
function M.inspect(api,manager)
    assert(api.valid(manager) and api.call(manager,'HasAuthority'),'host schematic manager required')
    local active=api.call(manager,'GetActiveSchematic')
    local signature,count=purchased(api,manager)
    local info={active=active,signature=signature,count=count,purchased=false}
    if api.valid(active)then
        info.path=active:GetFullName()
        info.purchased=api.call(manager,'IsSchematicPurchased',active,nil)
        local library=api.find('/Script/FactoryGame.Default__FGSchematic')
        info.name=Bridge.text(api.call(library,'GetSchematicDisplayName',active))
        info.tier=api.call(library,'GetTechTier',active)
        info.repeatable=api.call(library,'IsRepeatPurchasesAllowed',active)
    end
    api.log('HUB_RESEARCH active='..tostring(info.path)..' name='..tostring(info.name)..' tier='..tostring(info.tier)..' purchased='..tostring(info.purchased)..' count='..count)
    return info
end
function M.repair(api,manager)
    local before=M.inspect(api,manager)
    if not api.valid(before.active) or not before.purchased or before.repeatable then
        api.log('HUB_RESEARCH_UNCHANGED reason=no-stale-nonrepeatable-purchased-selection');return false
    end
    api.call(manager,'SetActiveSchematic',nil)
    assert(not api.valid(api.call(manager,'GetActiveSchematic')),'native API did not clear active milestone')
    local signature,count=purchased(api,manager)
    assert(signature==before.signature and count==before.count,'purchased milestone list changed')
    api.log('HUB_RESEARCH_REPAIRED active=none purchased_unchanged=true count='..count..' previous='..before.path)
    return true
end
return M
