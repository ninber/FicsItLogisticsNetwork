-- Stationary native fuel service. Never changes routes, inventories or possession.
local Bridge=require('Bridge')
local Core=require('Core')
local M={}
function M.new(api)
    local self={pending={},queue={},receipts={},retry={}}
    local function route(v)return Bridge.values(api.call(v,'GetVehicleRoute'),Core.guid)end
    local function count(inv,item)return api.call(inv,'GetNumItems',item)end
    function self:ready(home,station,vehicle)
        if self.pending[home]then return false end
        local b=api.call(station,'GetStation')
        if not api.call(b,'HasPower') or api.call(b,'IsProductionPaused')then return false end
        local r=route(vehicle)
        if #r~=1 or Core.guid_key(r[1])~=home or not api.call(vehicle,'IsAutopilotEnabled')then return false end
        local owner=api.call(vehicle,'GetOwnerVehicle')
        if not api.valid(owner)or api.call(owner,'HasActiveDriver')then return false end
        local fuel=api.call(b,'GetFuelInventory');local out={};api.call(fuel,'GetInventoryStacks',out,false)
        local item
        for _,s in ipairs(Bridge.values(out))do
            if s.NumItems>0 then item=Bridge.value(Bridge.value(s.Item).ItemClass);break end
        end
        local supplied=api.valid(item)
        local target=api.call(owner,'GetFuelInventory')
        if not supplied then
            local stacks={};api.call(target,'GetInventoryStacks',stacks,false)
            for _,s in ipairs(Bridge.values(stacks))do
                if s.NumItems>0 then item=Bridge.value(Bridge.value(s.Item).ItemClass);break end
            end
        end
        if not api.valid(item)then return false end
        local before=count(target,item)
        if before>=api.call(target,'GetSlotSize',0,item)then return true end
        if not supplied or (self.retry[home]or 0)>api.now()then return false end
        -- Do not replace another fuel type as an idle-service side effect.
        if before==0 and not api.call(target,'IsEmpty')then return false end
        local audit=Bridge.home_audit(station,api.call,api.valid,api.find)
        assert(audit.eligible,audit.reason)
        local p,q={X=0,Y=0,Z=0},{Pitch=0,Yaw=0,Roll=0}
        api.call(owner,'GetVehicleLocationAndRotation',p,q)
        local at=api.call(api.call(b,'GetDockingPathNode'),'K2_GetActorLocation')
        local d=(p.X-at.X)^2+(p.Y-at.Y)^2+(p.Z-at.Z)^2
        if d>=1000^2 then return false end
        local docked=api.call(b,'GetDockedActor')
        if api.valid(docked) and not Bridge.same_object(docked,owner,api.valid)then return false end
        if not api.valid(docked)then assert(api.call(b,'DockActor',owner)==true,'native stationary refueling rejected')end
        assert(Bridge.same_object(api.call(b,'GetDockedActor'),owner,api.valid),'native stationary docking readback failed')
        assert(api.call(vehicle,'IsAutopilotEnabled') and Core.same_route(route(vehicle),r),'native fuel service changed route/autopilot')
        self.pending[home]={vehicle=vehicle,owner=owner,building=b,item=item,before=before,started=api.now(),owned=true}
        self.queue[#self.queue+1]=home
        api.log('DEPOT_IDLE_REFUEL_STARTED home='..home..' route_unchanged=true controls_unchanged=true')
        return false
    end
    function self:tick()
        local home=table.remove(self.queue,1);if not home then return end
        local p=self.pending[home];if not p then return end
        local ok,done=pcall(function()
            if not api.call(p.vehicle,'IsAutopilotEnabled')or not api.call(p.building,'HasPower')or api.call(p.building,'IsProductionPaused')then return true end
            local r=route(p.vehicle)
            if #r~=1 or Core.guid_key(r[1])~=home then return true end
            local owner=api.call(p.vehicle,'GetOwnerVehicle')
            if not api.valid(owner)or api.call(owner,'HasActiveDriver')then return true end
            local after=count(api.call(owner,'GetFuelInventory'),p.item)
            local owned=Bridge.same_object(api.call(p.building,'GetDockedActor'),owner,api.valid)
            if after>p.before and(owned or p.owned)then
                self.receipts[home]={vehicle=p.vehicle:GetFullName(),before=p.before,after=after}
                api.log('DEPOT_NATIVE_FUEL_RECEIVED home='..home..' before='..p.before..' after='..after..' state=Idle')
                return true
            end
            p.owned=owned
            if api.now()-p.started>=45 then api.log('DEPOT_IDLE_REFUEL_WAIT home='..home..' native_transfer_not_observed=true');return true end
            return false
        end)
        if not ok then api.log('DEPOT_IDLE_REFUEL_ERROR home='..home..' reason='..tostring(done));done=true end
        if done then self.pending[home]=nil;self.retry[home]=api.now()+15 else self.queue[#self.queue+1]=home end
    end
    function self:clear()self.pending={};self.queue={};self.receipts={};self.retry={}end
    return self
end
return M
