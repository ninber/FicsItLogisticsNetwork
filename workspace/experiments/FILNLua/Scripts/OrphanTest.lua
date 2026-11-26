-- Called only by prepared, autosave-disabled disposable route-bridge tests.
local B=require('Bridge');local C=require('Core');local Orphans=require('OrphanRoutes')
local M={}
local function key(g)return C.guid_key(C.guid(g))end
function M.new(api)
    local self={}
    local function route(v)return B.values(api.call(v,'GetVehicleRoute'),C.guid)end
    function self:start(home)
        self.home=home;self.baseline={};local rows={};local by={}
        for _,s in pairs(api.stations())do by[key(api.call(s,'GetPathNodeGUID'))]=s end
        for _,v in pairs(api.vehicles())do
            local r=route(v);rows[#rows+1]={vehicle=v,route=r}
            if #r==4 and key(r[1])==home and key(r[4])==home then
                assert(not self.vehicle,'multiple test vehicles claim home');self.vehicle=v;self.provider=key(r[2])
            else self.baseline[#self.baseline+1]={vehicle=v,route=r,autopilot=api.call(v,'IsAutopilotEnabled')}end
        end
        assert(self.vehicle,'pinned orphan route missing')
        assert(api.call(self.vehicle,'IsAutopilotEnabled'),'test truck opted out')
        assert(not api.call(api.call(self.vehicle,'GetOwnerVehicle'),'HasActiveDriver'),'test truck player-driven')
        assert(not api.runtime().jobs[home],'fixture already has observer')
        local testapi={};for k,v in pairs(api)do testapi[k]=v end
        testapi.allowed_home=function(h)return h==home end
        local reconciler=Orphans.new(testapi);reconciler:refresh(rows,by)
        for _=1,#reconciler.queue do reconciler:step()end
        local result=assert(reconciler.records[self.vehicle:GetFullName()],'orphan not recognized')
        assert(result.status=='adopted','native adoption failed: '..result.reason)
        api.log('ORPHAN_TEST_ADOPTED home='..home..' observer=true other_vehicles_preserved='..#self.baseline)
    end
    function self:check()
        assert(self.vehicle,'start test first')
        for _,before in ipairs(self.baseline)do
            assert(C.same_route(route(before.vehicle),before.route),'non-test vehicle route changed: '..before.vehicle:GetFullName())
            assert(api.call(before.vehicle,'IsAutopilotEnabled')==before.autopilot,'non-test vehicle autopilot changed')
        end
        local runtime=api.runtime();local job=assert(runtime.jobs[self.home],'observer lost')
        assert(job.vehicle_key==self.vehicle:GetFullName()and job.state~='Detached','test ownership lost')
        assert(runtime.owners[job.vehicle_key]==self.home,'vehicle owner registry lost')
        local owner=api.call(self.vehicle,'GetOwnerVehicle')
        local amount=api.call(api.call(owner,'GetStorageInventory'),'GetNumItems',job.resource)
        local r=route(self.vehicle)
        if job.recovery_route then
            for _,g in ipairs(r)do assert(key(g)~=self.provider,'recovery revisits pickup provider')end
        end
        if job.state=='Idle' then
            assert(amount==0 and #r==1 and key(r[1])==self.home,'false completed recovery')
            api.log('ORPHAN_TEST_RETURNED home='..self.home..' empty=true observer=true other_vehicles_preserved=true');return true
        end
        api.log('ORPHAN_TEST_STATE home='..self.home..' state='..job.state..' recovery='..tostring(job.recovery and job.recovery.state)..' cargo='..amount..' observer=true other_vehicles_preserved=true')
        return false
    end
    return self
end
return M
