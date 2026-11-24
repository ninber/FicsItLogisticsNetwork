-- SPDX-License-Identifier: GPL-3.0-only
-- Read the operator's in-game Smarter Truck Stations filter; never infer from stock.
local Bridge=require('Bridge')
local M={}
function M.item(api,building)
    local class=building:GetClass()
    if not class:GetFullName():match('%.Build_ProgrammableTruckStation_C$') then return nil,'requires programmable truck station' end
    local selected
    for _,value in ipairs(Bridge.values(building.FilterItem))do
        local item=Bridge.value(value)
        if api.valid(item)then
            if selected and not Bridge.same_object(selected,item,api.valid)then return nil,'select exactly one item in station filters' end
            selected=item
        end
    end
    return selected,selected and 'in-game station filter' or 'select an item in station filters'
end
return M
