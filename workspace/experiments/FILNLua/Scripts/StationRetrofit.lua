-- A configuration retrofit: never replaces actors or rewrites road identities.
local Audit=require('CargoAudit')
local Bridge=require('Bridge')
local Core=require('Core')
local M={}
local function path(object)return object:GetFullName():match('^%S+ (.+)$')or object:GetFullName()end
function M.preview(api,identifier,item)
    assert(api.valid(item),'explicit item descriptor required')
    api.authority(identifier)
    local building=api.call(identifier,'GetStation');assert(api.valid(building),'station unavailable')
    assert(api.call(building,'GetDockingStationResourceForm')==1,'solid stations only')
    assert(not api.valid(api.call(building,'GetDockedActor')),'station is occupied')
    local guid=Core.guid_key(Core.guid(api.call(identifier,'GetPathNodeGUID')))
    assert(not api.busy(guid),'station has an active FILN job')
    local inventory=api.call(building,'GetInventory')
    local contents=Audit.inventory(api,inventory,item)
    assert(contents.kinds==0 or(contents.kinds==1 and contents.items[path(item)]),'clear incompatible cargo before changing filter; no items removed')
    local filters={}
    for index=0,contents.slots-1 do
        local old=api.call(inventory,'GetAllowedItemOnIndex',index)
        filters[#filters+1]={class=api.valid(old)and old or nil}
    end
    return {identifier=identifier,building=building,inventory=inventory,filters=filters,item=item,guid=guid,
        actor=building:GetFullName(),name=Bridge.text(api.call(identifier,'GetStationName')),
        load=api.call(building,'GetIsInLoadMode'),quantity=contents.quantity}
end
function M.restore(api,receipt)
    local errors={}
    for i,old in ipairs(receipt.filters)do
        local ok,why=pcall(api.call,receipt.inventory,'SetAllowedItemOnIndex',i-1,old.class)
        if not ok then errors[#errors+1]=tostring(why)end
    end
    assert(#errors==0,'filter rollback failed: '..table.concat(errors,'; '))
end
function M.apply(api,identifier,item)
    local receipt=M.preview(api,identifier,item)
    local ok,why=pcall(function()
        for i=1,#receipt.filters do api.call(receipt.inventory,'SetAllowedItemOnIndex',i-1,item)end
        for i=1,#receipt.filters do
            assert(Bridge.same_object(api.call(receipt.inventory,'GetAllowedItemOnIndex',i-1),item,api.valid),'filter readback failed')
        end
        assert(receipt.building:GetFullName()==receipt.actor,'actor identity changed')
        assert(Core.guid_key(Core.guid(api.call(identifier,'GetPathNodeGUID')))==receipt.guid,'road identity changed')
        assert(Bridge.text(api.call(identifier,'GetStationName'))==receipt.name,'station name changed')
        assert(api.call(receipt.building,'GetIsInLoadMode')==receipt.load,'role switch changed')
    end)
    if not ok then
        local restored,restore_error=pcall(M.restore,api,receipt)
        error(tostring(why)..(restored and ''or('; '..tostring(restore_error))))
    end
    return receipt
end
return M
