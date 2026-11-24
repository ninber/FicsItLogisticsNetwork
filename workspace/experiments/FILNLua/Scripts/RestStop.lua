-- SPDX-License-Identifier: GPL-3.0-only
-- Stop/release controller. Building registration and arrival hooks are separate adapters.
local Core=require('Core')
local M={}
local function copy(route)
    local out={}; for i,g in ipairs(route) do out[i]=Core.guid(g) end; return out
end
function M.new(adapter)
    -- Adapter calls must run on the authoritative game thread. No timers release a hold.
    for _,name in ipairs({'arrived','set_enabled','enabled','route','set_route','validate'}) do
        assert(type(adapter[name])=='function','missing adapter '..name)
    end
    local self={bays={},owners={},generation=0}
    function self:register(bay,vehicle)
        assert(type(bay)=='string' and bay~='' and type(vehicle)=='string' and vehicle~='', 'missing identity')
        assert(not self.bays[bay], 'bay already registered')
        assert(not self.owners[vehicle], 'vehicle already has a bay')
        self.bays[bay]={vehicle=vehicle,state='Away'}; self.owners[vehicle]=bay
    end
    local function stop(record)
        -- Mark held before crossing the bridge; an exception must never grant departure.
        record.state='Holding'; record.ticket=nil
        local ok,why=pcall(function()
            adapter.set_enabled(record.vehicle,false)
            assert(adapter.enabled(record.vehicle)==false,'rest-stop disable readback failed')
        end)
        record.state=ok and 'Held' or 'HoldFailed'
        if not ok then error(why) end
    end
    function self:arrive(bay,vehicle)
        local record=assert(self.bays[bay],'unknown bay')
        assert(record.vehicle==vehicle,'vehicle does not own bay')
        assert(adapter.arrived(bay,vehicle)==true,'arrival not confirmed')
        stop(record)
        record.route=copy(adapter.route(vehicle))
        self.generation=self.generation+1
        record.ticket=self.generation
        return record.ticket
    end
    function self:dispatch(bay,ticket,route)
        local record=assert(self.bays[bay],'unknown bay')
        assert(record.state=='Held' and ticket~=nil and record.ticket==ticket,'stale or missing hold ticket')
        -- Consume once even if dispatch fails; a failed operation needs explicit reacquisition.
        record.ticket=nil
        local ok,why=pcall(function()
            adapter.set_enabled(record.vehicle,false)
            assert(adapter.enabled(record.vehicle)==false,'vehicle no longer held')
            assert(adapter.arrived(bay,record.vehicle)==true,'vehicle left its bay')
            local intended=copy(route)
            assert(#intended>=2,'route needs multiple destinations')
            assert(not Core.same_route(record.route,intended),'dispatcher must replace previous path')
            assert(adapter.validate(bay,record.vehicle,intended)==true,'dispatch route rejected')
            adapter.set_route(record.vehicle,intended)
            assert(Core.same_route(copy(adapter.route(record.vehicle)),intended),'dispatch readback mismatch')
            adapter.set_enabled(record.vehicle,true)
            assert(adapter.enabled(record.vehicle)==true,'departure enable readback failed')
            record.state='Away'
        end)
        if not ok then
            local stopped,stop_error=pcall(stop,record)
            if not stopped then error(tostring(why)..'; HOLD FAILED: '..tostring(stop_error)) end
            error(why)
        end
    end
    return self
end
return M
