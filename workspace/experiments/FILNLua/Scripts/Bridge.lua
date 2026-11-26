local Core=require('Core')
local B={}
function B.value(value)
    for _=1,3 do
        if type(value)=='table' and type(rawget(value,'get'))=='function' then value=value:get()
        elseif type(value)=='userdata' then
            local ok,kind=pcall(function()return value:type()end)
            if ok and (kind=='RemoteUnrealParam' or kind=='LocalUnrealParam') then value=value:get() else break end
        else break end
    end
    return value
end
function B.values(array,transform)
    local out={}
    local function append(value)
        value=B.value(value)
        out[#out+1]=transform and transform(value) or value
    end
    assert(array~=nil,'game function returned no array')
    if type(array)=='table' and type(array.ForEach)~='function' then
        for _,value in ipairs(array)do append(value)end
    else array:ForEach(function(_,element)append(element)end) end
    return out
end
function B.same_object(a,b,valid)
    return valid(a) and valid(b) and a:GetFullName()==b:GetFullName()
end
function B.text(value)
    if type(value)=='string' then return value end
    local ok,text=pcall(function()return value:ToString()end)
    return ok and tostring(text) or '<unavailable>'
end
function B.home_audit(identifier,call,valid,find_class)
    local info={eligible=false,cargo_connections=0,fuel_connections=0,unknown_connections=0}
    local ok,why=pcall(function()
        local station=call(identifier,'GetStation');assert(valid(station),'station building unavailable')
        info.building=station
        info.load=call(station,'GetIsInLoadMode')==true
        info.solid=call(station,'GetDockingStationResourceForm')==1
        local cargo=call(station,'GetInventory');assert(valid(cargo),'cargo inventory unavailable')
        local fuel=call(station,'GetFuelInventory')
        info.empty=call(cargo,'IsEmpty')==true
        local class=find_class('/Script/FactoryGame.FGFactoryConnectionComponent')
        assert(valid(class),'factory connection component class unavailable')
        local components=B.values(call(station,'K2_GetComponentsByClass',class))
        local fuel_ports={}
        -- Fuel inputs can pull directly and legitimately have no connection inventory.
        local known,ports=pcall(function()return B.values(station.mFuelConnections)end)
        if known then for _,port in ipairs(ports)do if valid(port)then fuel_ports[port:GetFullName()]=true end end end
        for _,component in ipairs(components)do
            assert(valid(component),'invalid station connector')
            if call(component,'IsConnected') then
                local inventory=call(component,'GetInventory')
                if B.same_object(inventory,cargo,valid) then info.cargo_connections=info.cargo_connections+1
                elseif B.same_object(inventory,fuel,valid) or fuel_ports[component:GetFullName()] then info.fuel_connections=info.fuel_connections+1
                else info.unknown_connections=info.unknown_connections+1 end
            end
        end
        info.path=Core.guid(call(identifier,'GetPathNodeGUID'))
        if not info.solid then info.reason='home must be a solid-cargo station'
        elseif not info.load then info.reason='home must be LOAD'
        elseif not info.empty then info.reason='home cargo must be empty'
        elseif info.cargo_connections>0 then info.reason='home has connected cargo conveyors'
        elseif info.unknown_connections>0 then info.reason='connected connector inventory is unknown'
        else info.eligible=true;info.reason='eligible disconnected empty LOAD home' end
    end)
    if not ok then info.reason='home audit unavailable: '..tostring(why) end
    return info
end
return B
