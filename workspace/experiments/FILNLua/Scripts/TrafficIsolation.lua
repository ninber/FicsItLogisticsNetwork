-- Test-only traffic pause. Never changes routes, cargo, actors or player possession.
local Core=require('Core')
local Bridge=require('Bridge')
local M={}
function M.pause(api,home)
    assert(api.snapshot==true,'prepared disposable snapshot required')
    local rows,selected={},nil
    for _,vehicle in pairs(api.vehicles())do
        api.authority(vehicle)
        local route=Bridge.values(api.call(vehicle,'GetVehicleRoute'),Core.guid)
        local row={vehicle=vehicle,route=route,enabled=api.call(vehicle,'IsAutopilotEnabled')}
        row.home=#route==1 and Core.guid_key(route[1])==home
        if row.home then assert(not selected,'multiple trucks claim home');selected=row end
        rows[#rows+1]=row
    end
    assert(selected and selected.enabled,'active depot-only truck required')
    -- Finish every preflight before the first write, including other occupied vehicles.
    for _,row in ipairs(rows)do
        if not row.home and row.enabled then
            local owner=api.call(row.vehicle,'GetOwnerVehicle')
            assert(api.valid(owner) and not api.call(owner,'HasActiveDriver'),'another active vehicle has a player driver')
        end
    end
    local paused=0
    for _,row in ipairs(rows)do
        if not row.home and row.enabled then
            api.call(row.vehicle,'SetAutopilotEnabled',false);paused=paused+1
        end
    end
    for _,row in ipairs(rows)do
        assert(api.call(row.vehicle,'IsAutopilotEnabled')==row.home,'traffic isolation autopilot readback failed')
        assert(Core.same_route(Bridge.values(api.call(row.vehicle,'GetVehicleRoute'),Core.guid),row.route),'traffic isolation changed route')
    end
    api.log('TRAFFIC_ISOLATION_PASS vehicles='..#rows..' paused='..paused..' active=1 routes_unchanged=true snapshot_only=true')
    return paused
end
return M
