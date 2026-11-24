-- SPDX-License-Identifier: GPL-3.0-only
-- Explicit disposable-fixture preparation; never a production fuel source.
local M={}
function M.seed(api,vehicles,coal)
    assert(api.snapshot==true,'prepared disposable snapshot required')
    assert(api.valid(coal),'coal descriptor missing')
    local plan={}
    for home,vehicle in pairs(vehicles)do
        api.authority(vehicle)
        local owner=api.call(vehicle,'GetOwnerVehicle')
        assert(api.valid(owner) and not api.call(owner,'HasActiveDriver'),'fuel fixture needs unoccupied vehicle')
        local fuel=api.call(owner,'GetFuelInventory');assert(api.valid(fuel),'vehicle fuel inventory unavailable')
        if api.call(fuel,'IsEmpty')then plan[#plan+1]={home=home,fuel=fuel}end
    end
    for _,row in ipairs(plan)do
        local added=api.call(row.fuel,'AddStack',{Item={ItemClass=coal},NumItems=100},false)
        assert(added==100 and api.call(row.fuel,'GetNumItems',coal)==100,'starter fuel addition/readback failed')
        api.log('FLEET_TEST_FUEL_SEEDED home='..row.home..' amount=100 snapshot_only=true')
    end
    return #plan
end
return M
