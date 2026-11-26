local Bridge=require('Bridge')
local Core=require('Core')
local Programmable=require('ProgrammableStation')
local Policy=require('StockPolicy')
local M={}
local function path(object)return object:GetFullName():match('^%S+ (.+)$')or object:GetFullName()end
function M.inventory(api,inventory,configured)
    local call,valid=api.call,api.valid
    assert(valid(inventory),'inventory unavailable')
    local out={};call(inventory,'GetInventoryStacks',out,false)
    local items={};local kinds=0;local resource=configured
    for _,stack in ipairs(Bridge.values(out))do
        local item=Bridge.value(stack.Item);local class=Bridge.value(item.ItemClass)
        local n=stack.NumItems
        assert(type(n)=='number' and n>=0 and n==math.floor(n),'invalid stack amount')
        if n>0 then
            assert(valid(class),'nonempty stack has no descriptor')
            local id=path(class)
            if not items[id]then kinds=kinds+1;items[id]={class=class,count=0}end
            items[id].count=items[id].count+n
        end
    end
    if not resource and kinds==1 then for _,entry in pairs(items)do resource=entry.class end end
    if not resource and kinds==0 then
        local filter=call(inventory,'GetAllowedItemOnIndex',0)
        if valid(filter)then resource=filter end
    end
    local size=call(inventory,'GetSizeLinear')
    local quantity,capacity=0,0
    if resource then
        local id=path(resource);quantity=items[id]and items[id].count or 0
        local cache_key=inventory:GetFullName()..'|'..id..'|'..size
        local cached=api.capacity_cache and api.capacity_cache[cache_key]
        if cached then capacity=cached else
            for index=0,size-1 do
                local filter=call(inventory,'GetAllowedItemOnIndex',index)
                if not valid(filter)or Bridge.same_object(filter,resource,valid)then
                    capacity=capacity+call(inventory,'GetSlotSize',index,resource)
                end
            end
            if api.capacity_cache then api.capacity_cache[cache_key]=capacity end
        end
    end
    return {items=items,kinds=kinds,resource=resource,item=resource and path(resource)or nil,quantity=quantity,capacity=capacity,slots=size}
end
function M.station(api,identifier,configured)
    local b=api.call(identifier,'GetStation');assert(api.valid(b),'station unavailable')
    local inventory=api.call(b,'GetInventory')
    local guid=Core.guid_key(Core.guid(api.call(identifier,'GetPathNodeGUID')))
    local filter,filter_reason=Programmable.item(api,b)
    configured=filter
    local config=filter and {enabled=true,stock_source='station'} or nil
    local explicit=filter~=nil
    local data=M.inventory(api,inventory,configured)
    data.configured=explicit
    data.item_source=data.configured and 'in-game-filter' or 'observed-only'
    if configured and data.kinds>0 then
        assert(data.kinds==1 and data.items[data.item],'station contents conflict with configured single resource')
    end
    data.station_quantity=data.quantity;data.station_capacity=data.capacity
    data.eligible=false;data.reason=filter_reason
    if data.configured then data.stock_source='station' end
    data.identifier=identifier;data.building=b;data.inventory=inventory
    data.guid=guid
    data.role=api.call(b,'GetIsInLoadMode')and 'provider'or 'requester'
    -- LOAD supply is observational, not a learned request configuration.
    -- The delivery item is always selected by a programmable UNLOAD requester.
    if data.role=='provider' and not data.configured and data.kinds==1 and data.quantity>0 then
        data.configured=true;data.item_source='single-item-load-stock';data.stock_source='station'
        config={enabled=true}
    end
    data.label=Bridge.text(api.call(identifier,'GetStationName')):gsub('[\r\n]',' ')
    local recipe=api.find('/KDataForge/Gen/FILNRestStop.Gen_FILNRestStop_Recipe_RestStop')
    if data.configured then
        data.eligible,data.reason=Policy.evaluate(config,data.role,data.quantity,data.capacity)
    end
    data.depot=Bridge.same_object(Bridge.value(b.mBuiltWithRecipe),recipe,api.valid)
    return data
end
function M.report(api,stations)
    local records,errors={},0
    local ids={};for id in pairs(stations)do ids[#ids+1]=id end;table.sort(ids)
    for _,id in ipairs(ids)do
        local ok,data=pcall(M.station,api,stations[id])
        if ok then
            records[#records+1]=data
            api.log('CARGO_STATION id='..id..' guid='..data.guid..' role='..data.role..' depot='..tostring(data.depot)..
                ' configured='..tostring(data.configured)..' eligible='..tostring(data.eligible)..' item_source='..data.item_source..' kinds='..data.kinds..' item='..tostring(data.item)..' quantity='..data.quantity..' capacity='..data.capacity..' label='..data.label)
        else errors=errors+1;api.log('CARGO_AUDIT_ERROR '..id..' '..tostring(data))end
    end
    api.log('CARGO_AUDIT stations='..#records..' errors='..errors)
    assert(errors==0,'cargo stock audit has unavailable APIs/stations')
    return records
end
return M
