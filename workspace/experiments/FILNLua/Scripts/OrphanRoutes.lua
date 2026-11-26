-- Recover only exact FILN-shaped native schedules. One candidate per game-thread pass.
local C=require('Core');local B=require('Bridge');local Audit=require('CargoAudit')
local M={}
local function key(g)return C.guid_key(C.guid(g))end
local function shape(route)
    if #route~=4 then return end
    local h,p,r,last=key(route[1]),key(route[2]),key(route[3]),key(route[4])
    if h==last and h~=p and h~=r and p~=r then return h,p,r end
end
function M.new(api)
    local self={queue={},index=1,records={},retry={},duplicates={}}
    local now=api.now or os.time
    local function report(candidate,status,reason)
        local old=self.records[candidate.key]
        local record={home=candidate.home,provider=candidate.provider,requester=candidate.requester,
            status=status,reason=tostring(reason),at=now(),next_retry=self.retry[candidate.key]}
        self.records[candidate.key]=record
        if not old or old.status~=status or old.reason~=record.reason or now()>=(old.logged_at or 0)+30 then
            api.log('ORPHAN_ROUTE vehicle='..candidate.key..' home='..candidate.home..' status='..status..' reason='..record.reason)
            record.logged_at=now()
        else record.logged_at=old.logged_at end
    end
    function self:refresh(vehicles,stations)
        local next_key=self.queue[self.index]and self.queue[self.index].key
        self.queue={};self.index=1;self.stations=stations;self.duplicates={}
        local claims,seen={},{}
        for _,entry in ipairs(vehicles)do
            local candidate
            local ok,problem=pcall(function()
                local route=entry.route;local h,p,r=shape(route)
                if #route==1 then h=key(route[1])end
                if h then claims[h]=(claims[h]or 0)+1 end
                if p and r then
                    candidate={key=entry.vehicle:GetFullName(),vehicle=entry.vehicle,home=h,provider=p,requester=r,route=route}
                    local b=stations[h]and api.call(stations[h],'GetStation')
                    local recipe=api.find('/KDataForge/Gen/FILNRestStop.Gen_FILNRestStop_Recipe_RestStop')
                    if api.valid(b)and B.same_object(B.value(b.mBuiltWithRecipe),recipe,api.valid)then
                        seen[candidate.key]=true;self.queue[#self.queue+1]=candidate
                    end
                end
            end)
            if not ok then
                if candidate then seen[candidate.key]=true;report(candidate,'blocked','registry inspection: '..tostring(problem))
                else api.log('ORPHAN_DISCOVERY_SKIPPED reason='..tostring(problem))end
            end
        end
        for h,count in pairs(claims)do if count>1 then self.duplicates[h]=true end end
        for name in pairs(self.records)do if not seen[name]then self.records[name]=nil;self.retry[name]=nil end end
        table.sort(self.queue,function(a,b)return a.key<b.key end)
        -- A registry refresh must not restart a large sweep at its first truck forever.
        if next_key then
            for i,candidate in ipairs(self.queue)do if candidate.key>=next_key then self.index=i;break end end
        end
    end
    function self:step()
        local candidate=self.queue[self.index];if not candidate then return end
        self.index=self.index+1
        if api.allowed_home and not api.allowed_home(candidate.home)then return end
        local v=candidate.vehicle;local runtime=api.runtime();local previous=runtime.jobs[candidate.home]
        if previous then
            if previous.state=='Detached'then report(candidate,'opted-out','detached ownership retained; depot-only route required to re-enroll')
            elseif previous.vehicle_key==candidate.key then report(candidate,'managed',previous.state)
            else report(candidate,'blocked','home already owned by another job')end
            return
        end
        if now()<(self.retry[candidate.key]or 0)then return end
        local outcome,status,why=pcall(function()
            assert(not self.duplicates[candidate.home],'multiple vehicles claim this depot')
            assert(C.same_route(B.values(api.call(v,'GetVehicleRoute'),C.guid),candidate.route),'route changed since registry scan')
            if not api.call(v,'IsAutopilotEnabled')then return 'opted-out','autopilot is off; controls unchanged' end
            local owner=api.call(v,'GetOwnerVehicle');assert(api.valid(owner),'owner unavailable')
            if api.call(owner,'HasActiveDriver')then return 'deferred','player is driving; controls unchanged' end
            local home=self.stations[candidate.home]
            local b=api.call(home,'GetStation')
            assert(api.call(b,'HasPower')and not api.call(b,'IsProductionPaused'),'home disabled')
            for _,id in ipairs({candidate.requester})do
                assert(not runtime.reservations[id]or runtime.reservations[id]==candidate.home,'endpoint reserved by another delivery: '..id)
            end
            local provider=api.call(assert(self.stations[candidate.provider],'provider missing'),'GetStation')
            local recipe=api.find('/KDataForge/Gen/FILNRestStop.Gen_FILNRestStop_Recipe_RestStop')
            local requester=Audit.station(api,assert(self.stations[candidate.requester],'requester missing'))
            assert(not B.same_object(B.value(provider.mBuiltWithRecipe),recipe,api.valid)and api.call(provider,'GetIsInLoadMode'),'first cargo stop is not LOAD')
            assert(not requester.depot and requester.role=='requester'and requester.configured and requester.resource,'UNLOAD requester needs explicit item filter')
            local cargo=Audit.inventory(api,api.call(owner,'GetStorageInventory'),requester.resource)
            assert(cargo.kinds==0 or(cargo.kinds==1 and cargo.items[requester.item]),'carried cargo conflicts with requester filter')
            api.authority(v)
            -- Recheck opt-out after all potentially failing discovery work.
            assert(api.call(v,'IsAutopilotEnabled')and not api.call(owner,'HasActiveDriver'),'player control changed; deferred')
            local old=api.journal.entries[candidate.home]
            api.journal:put(candidate.home,candidate.provider,candidate.requester)
            if old and(old.provider~=candidate.provider or old.requester~=candidate.requester)then
                api.log('ORPHAN_JOURNAL_RECONCILED home='..candidate.home..' old_provider='..tostring(old.provider)..' old_requester='..old.requester..
                    ' provider='..candidate.provider..' requester='..candidate.requester..' evidence=current-native-route')
            end
            local ok,result=pcall(runtime.restore_persisted,runtime,candidate.home,candidate.provider,candidate.requester,v)
            if not ok and not runtime.jobs[candidate.home]then
                if old then
                    if old.kind=='cargo'then api.journal:put_cargo(candidate.home,old.requester)else api.journal:put(candidate.home,old.provider,old.requester)end
                else api.journal:remove(candidate.home)end
                error(result)
            end
            local job=assert(runtime.jobs[candidate.home],'restore did not attach an observer')
            assert(job.vehicle_key==candidate.key,'restore attached another vehicle')
            return 'adopted','observed recovery attached; actual cargo='..cargo.quantity..(not ok and ('; restore warning='..tostring(result))or '')
        end)
        if not outcome then
            self.retry[candidate.key]=now()+30;report(candidate,'blocked',status)
        else
            self.retry[candidate.key]=nil
            report(candidate,status,why)
        end
    end
    return self
end
return M
