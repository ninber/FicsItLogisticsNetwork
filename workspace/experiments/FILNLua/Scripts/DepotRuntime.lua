-- SPDX-License-Identifier: GPL-3.0-only
local Core=require('Core')
local Bridge=require('Bridge')
local RestStop=require('RestStop')
local CargoAudit=require('CargoAudit')
local StockPolicy=require('StockPolicy')
local Recovery=require('RecoveryFSM')
local M={}
function M.new(api)
    local valid=api.valid
    local function call(object,method,...)
        local ok,result=pcall(api.call,object,method,...)
        if not ok then error(method..': '..tostring(result))end
        return result
    end
    local self={jobs={},owners={},active={},cursor=1,reservations={},pending=false}
    self.fuel_service=require('DepotFuelService').new({call=call,valid=valid,find=api.find,now=api.now,log=api.log})
    local function same(a,b)return Bridge.same_object(a,b,valid)end
    local function route(v)return Bridge.values(call(v,'GetVehicleRoute'),Core.guid)end
    local function key(g)return Core.guid_key(Core.guid(g))end
    local function guid(s)return Core.guid(call(s,'GetPathNodeGUID'))end
    local function position(owner)
        local p,r={X=0,Y=0,Z=0},{Pitch=0,Yaw=0,Roll=0}
        call(owner,'GetVehicleLocationAndRotation',p,r)
        return {X=p.X,Y=p.Y,Z=p.Z}
    end
    local function distance(a,b)return math.sqrt((a.X-b.X)^2+(a.Y-b.Y)^2+(a.Z-b.Z)^2)end
    local function fuel_count(owner)
        local inventory=call(owner,'GetFuelInventory')
        if not valid(inventory)then return 0 end
        local stacks={};call(inventory,'GetInventoryStacks',stacks,false)
        local count=0;for _,stack in ipairs(Bridge.values(stacks))do count=count+stack.NumItems end
        return count
    end
    local function watch_fuel(job,home_docked)
        if home_docked or job.last_home_fuel_watch then
            local count=fuel_count(job.owner)
            if job.last_fuel_count and count>job.last_fuel_count then
                job.native_fuel_received=true
                api.log('DEPOT_NATIVE_FUEL_RECEIVED home='..job.home..' before='..job.last_fuel_count..' after='..count..' state='..job.state)
            end
            job.last_fuel_count=count
        end
        job.last_home_fuel_watch=home_docked
    end
    local function docked(job,index)
        return same(call(job.buildings[index],'GetDockedActor'),job.owner)
    end
    local function audit(identifier)
        local result=Bridge.home_audit(identifier,call,valid,api.find)
        assert(result.eligible,result.reason)
        assert(call(result.building,'HasPower') and not call(result.building,'IsProductionPaused'),'home is unpowered or in standby')
        -- User-supplied native fuel inputs are allowed; cargo connections remain forbidden.
        -- Refueling uses the native station cycle and player-supplied fuel.
        return result.building
    end
    function self:resolve(home_key,first_key,second_key,resource)
        assert(home_key~=first_key and first_key~=second_key and home_key~=second_key,'trip requires three distinct bays')
        local recipe=api.find('/KDataForge/Gen/FILNRestStop.Gen_FILNRestStop_Recipe_RestStop')
        assert(valid(recipe),'custom rest-stop recipe unavailable')
        local by_guid={}
        for _,s in pairs(api.stations())do
            local b=call(s,'GetStation')
            if valid(b)then by_guid[key(guid(s))]=s end
        end
        local nodes={home_key,first_key,second_key};local buildings,identifiers={},{}
        for i,k in ipairs(nodes)do
            identifiers[i]=assert(by_guid[k],'fixture station missing: '..k)
            local b=call(identifiers[i],'GetStation')
            if i==1 or not resource then
                assert(same(Bridge.value(b.mBuiltWithRecipe),recipe),'home/test bay is not a FILN rest stop')
                buildings[i]=audit(identifiers[i])
            else
                assert(not same(Bridge.value(b.mBuiltWithRecipe),recipe),'parking bay cannot handle cargo')
                buildings[i]=b
            end
        end
        local found
        for _,v in pairs(api.vehicles())do
            local r=route(v)
            if #r==1 and key(r[1])==home_key then assert(not found,'multiple trucks claim depot');found=v end
        end
        assert(found,'no truck with depot-only route')
        api.authority(found)
        local owner=call(found,'GetOwnerVehicle');assert(valid(owner),'truck actor unavailable')
        return {home=home_key,vehicle=found,vehicle_key=found:GetFullName(),owner=owner,buildings=buildings,identifiers=identifiers,
            home_route={guid(identifiers[1])},native_trip={guid(identifiers[1]),guid(identifiers[2]),guid(identifiers[3]),guid(identifiers[1])},trip={guid(identifiers[2]),guid(identifiers[3]),guid(identifiers[1])},
            resource=resource,loaded=0,unloaded=0,state='Prepared',visited=0,departed=false,events={},initial=position(owner)}
    end
    function self:probe(home,first,second,resource)
        local job=self:resolve(home,first,second,resource)
        local node=call(job.buildings[1],'GetDockingPathNode')
        local location=call(node,'K2_GetActorLocation')
        api.log('DEPOT_PROBE vehicle='..job.vehicle:GetFullName()..' docking='..tostring(call(job.vehicle,'IsCurrentlyDocking'))..
            ' docked='..tostring(docked(job,1))..' distance_cm='..math.floor(distance(job.initial,location))..
            ' error='..tostring(call(job.vehicle,'GetAutopilotErrorStatus')))
        return job
    end
    function self:prepare(home,first,second,resource)
        local job=self:resolve(home,first,second,resource)
        -- Preparation is read-only. An occupied vehicle must already have native autopilot enabled.
        if call(job.owner,'HasActiveDriver')then assert(call(job.vehicle,'IsAutopilotEnabled'),'player disabled autopilot')end
    end
    local function validate(job,intended)
        assert(Core.same_route(intended,job.native_trip),'dispatcher trip intent changed')
        audit(job.identifiers[1])
        if job.resource then
            assert(StockPolicy.pair_allowed(Bridge.text(call(job.identifiers[2],'GetStationName')),Bridge.text(call(job.identifiers[3],'GetStationName'))),'[S] storage-to-storage delivery forbidden')
            local id=job.resource:GetFullName():match('^%S+ (.+)$')
            for index=2,3 do
                assert(not valid(call(job.buildings[index],'GetDockedActor')),'cargo station is occupied')
                local data=CargoAudit.station({call=call,valid=valid,find=api.find},job.identifiers[index],job.resource)
                assert(data.kinds<=1 and (data.kinds==0 or data.items[id]),'cargo station contains another resource')
                assert((index==2 and data.role=='provider')or(index==3 and data.role=='requester'),'station load/unload mode changed')
                assert(data.configured and data.item==id,'station in-game filter differs from delivery item')
                local truckload=CargoAudit.inventory({call=call,valid=valid},call(job.owner,'GetStorageInventory'),job.resource).capacity
                job.truckload=truckload
                assert(StockPolicy.evaluate({},data.role,data.quantity,data.capacity,truckload),'station no longer satisfies truckload delivery policy')
                if index==3 then assert(data.station_capacity>=truckload,'requester cannot hold a truckload')end
            end
        else for i=2,3 do audit(job.identifiers[i])end end
        local preset=call(job.owner,'GetVehiclePathPreset');assert(valid(preset),'vehicle path preset missing')
        local sequence=job.native_trip
        for i=1,#sequence-1 do
            local network=call(api.context(),'FindNetworkByPathNodeGuid',sequence[i])
            assert(valid(network) and same(network,call(api.context(),'FindNetworkByPathNodeGuid',sequence[i+1])),'disconnected trip leg '..i)
            assert(call(network,'FindVehiclePath',sequence[i],sequence[i+1],preset,{})==true,'unreachable trip leg '..i)
        end
        return true
    end
    function self:detach(job,why)
        job.state='Detached';job.error=nil
        for index=1,2 do
            local node=job.trip[index];local id=node and key(node);if id and self.reservations[id]==job.home then self.reservations[id]=nil end
        end
        self.owners[job.vehicle_key]=nil
        if api.on_detach then local ok,why=pcall(api.on_detach,job.home);if not ok then api.log('DETACH_JOURNAL_PENDING '..tostring(why))end end
        api.log('DEPOT_DETACHED home='..job.home..' reason='..why..' route_unchanged=true controls_unchanged=true')
    end
    local function trace(job,event,reason)
        local function read(o,m,...)
            local ok,value=pcall(call,o,m,...);return ok and tostring(value) or 'unavailable'
        end
        local p=position(job.owner)
        local names={[0]='None',[1]='StationUnreachable',[2]='NotOnPath',[3]='TooFewStations',[4]='NoFuel',[5]='Deadlocked'}
        api.log('DEPOT_FSM native_status='..tostring(names[tonumber(read(job.vehicle,'GetAutopilotErrorStatus'))])..' event='..event..' home='..job.home..' vehicle='..job.vehicle_key..
            ' state='..job.state..' recovery='..tostring(job.recovery and job.recovery.state)..
            ' attempt='..tostring(job.recovery and job.recovery.attempts or 0)..
            ' autopilot='..read(job.vehicle,'IsAutopilotEnabled')..' driver='..read(job.owner,'HasActiveDriver')..
            ' native_error='..read(job.vehicle,'GetAutopilotErrorStatus')..' target='..read(job.vehicle,'GetCurrentTargetWaypointIndex')..
            ' truck_fuel='..fuel_count(job.owner)..' loaded='..job.loaded..' unloaded='..job.unloaded..
            ' position='..math.floor(p.X)..','..math.floor(p.Y)..','..math.floor(p.Z)..' reason='..tostring(reason))
    end
    local function stop(job,why)
        if job.state~='Recovery' then
            job.resume_state=job.state;job.recovery=Recovery.new(api.now())
            job.resume_native_fault=tostring(why):find('active trip autopilot error',1,true)~=nil
            job.state='Recovery';job.recovery_position=position(job.owner);job.progress_at=api.now()
        end
        job.error=tostring(why)
        trace(job,'RECOVERY_ENTER',why)
        -- Keep route, autopilot and possession intact. Native faults can clear after repair.
    end
    local function release(job)
        for _,node in ipairs(job.trip)do
            local id=key(node);if self.reservations[id]==job.home then self.reservations[id]=nil end
        end
        if api.on_detach then api.on_detach(job.home)end
    end
    local function write_route(job,intended)
        -- Preserve both sides of our own failed write; do not call it a player route edit.
        job.internal_route_before=route(job.vehicle);job.internal_route_intent=intended
        call(job.vehicle,'SetVehicleRoute',intended)
        assert(Core.same_route(route(job.vehicle),intended),'internal route readback failed')
    end
    local function clear_write(job)job.internal_route_before=nil;job.internal_route_intent=nil end
    local function recovery_step(job)
        local now=api.now();local p=position(job.owner)
        if distance(p,job.recovery_position or p)>100 then job.recovery_position=p;job.progress_at=now end
        local here=docked(job,1) or distance(p,call(call(job.buildings[1],'GetDockingPathNode'),'K2_GetActorLocation'))<1000
        local inventory=call(job.owner,'GetStorageInventory')
        local empty=call(inventory,'IsEmpty')
        if job.resource then
            local quantity=call(inventory,'GetNumItems',job.resource)
            job.loaded=math.max(job.loaded,quantity)
            job.unloaded=math.max(job.unloaded,job.loaded-quantity)
        end
        local current=route(job.vehicle)
        if job.internal_route_intent and Core.same_route(current,job.internal_route_intent)then
            job.recovery_route=job.internal_route_intent;job.resume_native_fault=false;clear_write(job)
        end
        local expected=job.recovery_route or job.native_trip
        local internal_before=job.internal_route_before and Core.same_route(current,job.internal_route_before)
        local state,reason=Recovery.step(job.recovery,{now=now,autopilot=call(job.vehicle,'IsAutopilotEnabled'),
            driver=call(job.owner,'HasActiveDriver'),route_changed=not Core.same_route(current,expected) and not internal_before and not(job.resume_state=='Prepared' and Core.same_route(current,job.home_route)),
            home=here,empty=empty,fuel=call(job.owner,'HasFuel'),error=call(job.vehicle,'GetAutopilotErrorStatus'),
            returning=job.recovery_route~=nil,resumable=job.resume_state=='Trip'and job.resume_native_fault==true and job.visited==0 and empty,
            stalled=now-(job.progress_at or now)>60 and not call(job.vehicle,'IsCurrentlyDocking')})
        if state~=job.recovery.state or now>=(job.next_trace or 0)then
            job.recovery.state=state;job.next_trace=now+30;trace(job,'RECOVERY_STEP',reason)
        end
        if state=='Detached'then self:detach(job,reason);return end
        if state=='HomeReady'then
            if not Core.same_route(current,job.home_route)then write_route(job,job.home_route)end
            job.recovery_route=job.home_route;clear_write(job)
            if now<(job.cleanup_retry or 0)then return end
            local released,problem=pcall(release,job)
            if not released then
                job.cleanup_retry=now+30;job.recovery.next_retry=job.cleanup_retry
                job.recovery.last_error='journal cleanup: '..tostring(problem)
                trace(job,'RECOVERY_PERSISTENCE_WAIT',job.recovery.last_error);return
            end
            job.cleanup_retry=nil;job.recovery.last_error=nil
            job.state='Idle';job.error=nil;job.return_position=p
            trace(job,'RECOVERED_IDLE','cargo-empty leases-released fuel-gate-required');return
        end
        if state=='Resume'then
            job.state=job.resume_state;job.started=now;job.error=nil
            trace(job,'RECOVERY_RESUMED',reason);return
        end
        if state=='Returning' and empty and job.recovery_route and key(job.recovery_route[1])~=job.home and key(job.recovery_route[2])~=job.home then
            job.resume_state='Return';job.recovery.next_retry=now
            state='Replan'
        end
        if state~='Replan'then return end
        local ok,why=pcall(function()
            audit(job.identifiers[1])
            local destination=empty and job.home_route[1] or(job.requester_guid or job.trip[2])
            assert(destination,'carried cargo has no known requester; refusing to guess')
            local anchor
            if empty then
                -- A second distinct rest stop makes native autopilot accept the return route.
                -- Never use a cargo provider as an idle-return anchor.
                for _,station in pairs(api.stations())do
                    local id=guid(station)
                    if key(id)~=job.home then
                        local candidate=pcall(audit,station)
                        local network=call(api.context(),'FindNetworkByPathNodeGuid',destination)
                        if candidate and valid(network) and call(network,'FindVehiclePath',destination,id,call(job.owner,'GetVehiclePathPreset'),{})then anchor=id;break end
                    end
                end
                assert(anchor,'no reachable rest-stop anchor for native return')
            else anchor=job.home_route[1]end
            local from=Core.guid(call(job.vehicle,'GetCurrentToPathNodeGUID'))
            assert(key(from)~=string.rep('0',32),'truck is off the road; physical placement repair required')
            local network=call(api.context(),'FindNetworkByPathNodeGuid',from)
            assert(valid(network) and call(network,'FindVehiclePath',from,destination,call(job.owner,'GetVehiclePathPreset'),{}),'destination remains unreachable from current segment')
            assert(not call(job.owner,'HasActiveDriver') and call(job.vehicle,'IsAutopilotEnabled'),'player control changed before recovery write')
            local intended={destination,anchor}
            job.resume_native_fault=false
            write_route(job,intended)
            job.recovery_route=intended;clear_write(job)
            assert(call(job.vehicle,'IsAutopilotEnabled'),'native recovery autopilot rejected')
            job.progress_at=now;job.recovery_position=p
            trace(job,'RECOVERY_REPLAN','destination='..key(destination)..' anchor='..key(anchor))
        end)
        if not ok then job.recovery.last_error=tostring(why);trace(job,'RECOVERY_RETRY_FAILED',why)else job.recovery.last_error=nil end
    end
    local function finish(job,source,player_present)
        assert(job.visited==2 and job.departed and (docked(job,1)or(job.service_since and distance(position(job.owner),job.service_position)<1000)),'return not confirmed')
        if job.resource then
            assert(job.loaded>0 and job.loaded<=job.truckload,'invalid observed cargo amount')
            if job.loaded<job.truckload then api.log('DEPOT_PARTIAL_DELIVERY home='..job.home..' planned='..job.truckload..' actual='..job.loaded..' reason=pickup-stock-changed')end
            assert(call(call(job.owner,'GetStorageInventory'),'IsEmpty'),'returned truck still carries cargo')
            job.unloaded=job.loaded
        end
        write_route(job,job.home_route);clear_write(job)
        assert(call(job.vehicle,'IsAutopilotEnabled'),'idle membership flag restore failed')
        job.return_position=position(job.owner);job.hold_since=api.now();job.state='Holding';job.stop_source=source
        api.log('DEPOT_RETURN home='..job.home..' source='..source..' route=home-only autopilot=true')
    end
    function self:observe(job,source)
        api.authority(job.vehicle)
        -- The physical owner can change as proxy/significance/docking state changes.
        job.owner=call(job.vehicle,'GetOwnerVehicle')
        assert(valid(job.owner),'current truck actor unavailable')
        local player_present=call(job.owner,'HasActiveDriver')
        local autopilot=call(job.vehicle,'IsAutopilotEnabled')
        if (job.state=='Trip' or job.state=='Servicing' or job.state=='Suspended' or job.state=='Holding' or job.state=='Idle') and not autopilot then self:detach(job,'autopilot-disabled');return end
        if job.state=='Recovery' then recovery_step(job);return end
        if job.state=='Suspended' and not player_present then
            if job.suspended_state=='Prepared' then stop(job,job.error);return end
            job.state=job.suspended_state;job.error=nil
        end
        for i,identifier in ipairs(job.identifiers)do
            job.buildings[i]=call(identifier,'GetStation')
            assert(valid(job.buildings[i]),'station actor unavailable')
        end
        if job.state=='Trip' then
            if job.visited==0 then watch_fuel(job,docked(job,1))end
            if job.resource then
                assert(call(job.buildings[2],'GetIsInLoadMode') and not call(job.buildings[3],'GetIsInLoadMode'),'cargo station mode changed')
                local quantity=call(call(job.owner,'GetStorageInventory'),'GetNumItems',job.resource)
                if job.visited>=1 and quantity>job.loaded then
                    job.loaded=quantity;api.log('CARGO_LOAD observed='..quantity..' home='..job.home)
                end
                if job.visited>=2 then job.unloaded=math.max(job.unloaded,job.loaded-quantity)end
                if job.visited>=1 and job.loaded>0 and not docked(job,2) and not job.provider_released then
                    local provider=key(job.trip[1])
                    if self.reservations[provider]==job.home then self.reservations[provider]=nil end
                    job.provider_released=true
                    api.log('CARGO_PROVIDER_RELEASED home='..job.home..' provider='..provider..' loaded='..job.loaded)
                end
            end
            if not Core.same_route(route(job.vehicle),job.native_trip)then self:detach(job,'player-route-changed');return end
            local p=position(job.owner)
            if not job.progress_position or distance(p,job.progress_position)>100 then job.progress_position=p;job.progress_time=api.now()end
            assert(call(job.vehicle,'IsCurrentlyDocking')or api.now()-(job.progress_time or api.now())<60,'no road progress for 60 seconds')
            local status=call(job.vehicle,'GetAutopilotErrorStatus')
            if status==0 then job.enable_deadline=nil end
            if job.fuel_deadline then
                if call(job.owner,'HasFuel')then job.fuel_deadline=nil;api.log('DEPOT_FUEL_READY home='..job.home)
                else assert(api.now()<job.fuel_deadline,'initial depot refueling timed out')end
            end
            assert(status==0 or (status==3 and job.enable_deadline and api.now()<job.enable_deadline) or ((status==3 or status==4)and job.fuel_deadline and not job.departed and api.now()<job.fuel_deadline) or(status==4 and job.visited==0 and docked(job,1)and job.initial_dock_deadline and api.now()<job.initial_dock_deadline),'active trip autopilot error '..tostring(status))
            assert(api.now()-job.started<(api.journey_timeout or 1800),'journey timed out')
            if not docked(job,1) and distance(position(job.owner),job.initial)>300 then job.departed=true end
            if docked(job,2) then
                assert(job.visited<2,'unexpected second loop at first stop')
                if job.visited==0 then job.visited=1;api.log('DEPOT_VISIT stop=1 home='..job.home)end
            elseif docked(job,3) then
                assert(job.visited>=1,'missed or out-of-order first stop')
                if job.visited==1 then job.visited=2;api.log('DEPOT_VISIT stop=2 home='..job.home)end
            elseif docked(job,1) and job.departed and job.visited==2 then
                local fuel=call(job.buildings[1],'GetFuelInventory')
                if valid(fuel) and not call(fuel,'IsEmpty')then
                    job.last_fuel_count=fuel_count(job.owner);job.last_home_fuel_watch=true
                    job.state='Servicing';job.service_since=api.now();job.service_position=position(job.owner)
                    job.service_docking=call(job.vehicle,'IsCurrentlyDocking')
                    job.service_progress=tonumber(job.buildings[1].mLoadUnloadCycleProgress)or 0
                    api.log('DEPOT_REFUEL_SERVICE home='..job.home..' source=native-docking')
                else finish(job,source,player_present)end
            end
        elseif job.state=='Servicing' then
            watch_fuel(job,docked(job,1))
            assert(Core.same_route(route(job.vehicle),job.native_trip),'servicing route changed')
            assert(api.now()-job.service_since<45,'native depot refueling did not complete')
            assert(distance(position(job.owner),job.service_position)<1000,'vehicle left depot during fuel service')
            local docking=call(job.vehicle,'IsCurrentlyDocking')
            local progress=tonumber(job.buildings[1].mLoadUnloadCycleProgress)or 0
            if(job.service_docking and not docking)or(job.service_progress>0 and progress<job.service_progress-.5)then
                finish(job,source,player_present)
            else
                job.service_docking=job.service_docking or docking;job.service_progress=progress
            end
        elseif job.state=='Holding' or job.state=='Idle' then
            assert(call(job.vehicle,'IsAutopilotEnabled'),'idle membership lost')
            assert(Core.same_route(route(job.vehicle),job.home_route),'idle route changed')
            assert(distance(position(job.owner),job.return_position)<100,'truck moved while held')
            if job.state=='Holding' and api.now()-job.hold_since>=12 then
                for _,identifier in ipairs({job.identifiers[2],job.identifiers[3]})do
                    local endpoint=key(guid(identifier))
                    if self.reservations[endpoint]==job.home then self.reservations[endpoint]=nil end
                end
                job.state='Idle';api.log('DEPOT_CYCLE_PASS home='..job.home..' cycle='..job.cycle..' visited=2 returned=true idle_route=1 held_seconds=12 source='..job.stop_source..' loaded='..job.loaded..' unloaded='..job.unloaded)
            end
        end
    end
    function self:tick()
        self.fuel_service:tick()
        local budget=math.min(16,#self.active)
        for _=1,budget do
            if #self.active==0 then break end
            self.cursor=(self.cursor-1)%#self.active+1
            local job=self.active[self.cursor]
            local before=job.state
            local ok,why=pcall(self.observe,self,job,'poll')
            if not ok then
                local entered,problem=pcall(stop,job,why)
                if not entered then api.log('DEPOT_RECOVERY_OBSERVER_ERROR home='..job.home..' reason='..tostring(problem))end
            end
            if before~=job.state or api.now()>=(job.next_snapshot or 0)then
                job.next_snapshot=api.now()+30
                local logged,problem=pcall(trace,job,before~=job.state and 'TRANSITION' or 'HEARTBEAT','from='..before..' observer='..tostring(why))
                if not logged then api.log('DEPOT_DIAGNOSTIC_UNAVAILABLE home='..job.home..' reason='..tostring(problem))end
            end
            if job.state=='Detached' then table.remove(self.active,self.cursor)else self.cursor=self.cursor+1 end
        end
    end
    function self:begin(home,first,second,resource)
        local previous=self.jobs[home]
        assert(not previous or previous.state=='Idle','bay already assigned or recovering')
        local job=self:probe(home,first,second,resource)
        local needs_refuel=not call(job.owner,'HasFuel')
        if needs_refuel then
            local fuel=call(job.buildings[1],'GetFuelInventory')
            assert(valid(fuel) and not call(fuel,'IsEmpty'),'vehicle and depot have no fuel')
            assert(not call(job.owner,'HasActiveDriver'),'initial refueling requires normal player exit')
        end
        local player_present=call(job.owner,'HasActiveDriver')
        if player_present then assert(call(job.vehicle,'IsAutopilotEnabled'),'player disabled autopilot')end
        local home_fuel=call(job.buildings[1],'GetFuelInventory')
        local service_home=not player_present and valid(home_fuel) and not call(home_fuel,'IsEmpty')
        if resource then assert(call(call(job.owner,'GetStorageInventory'),'IsEmpty'),'truck must start empty')end
        assert(not self.owners[job.vehicle_key] or self.owners[job.vehicle_key]==home,'truck already enrolled')
        assert(not self.jobs[first] and not self.jobs[second],'another truck owns a trip stop')
        assert(not self.reservations[first] and not self.reservations[second],'trip stop already reserved')
        local error_status=call(job.vehicle,'GetAutopilotErrorStatus')
        assert(error_status==0 or error_status==3 or(needs_refuel and error_status==4),'idle vehicle has unexpected error')
        local at_home=docked(job,1)
        if not at_home then
            local node=call(job.buildings[1],'GetDockingPathNode')
            assert(distance(job.initial,call(node,'K2_GetActorLocation'))<1000,'truck is not at its assigned bay')
        end
        validate(job,job.native_trip)
        local receipt=self.fuel_service.receipts[home]
        if receipt and receipt.vehicle==job.vehicle_key then job.native_fuel_received=true end
        job.original_route=route(job.vehicle);job.original_autopilot=call(job.vehicle,'IsAutopilotEnabled')
        job.cycle=previous and previous.cycle+1 or 1
        if previous then
            for i=#self.active,1,-1 do if self.active[i]==previous then table.remove(self.active,i)end end
        end
        self.reservations[first]=home;self.reservations[second]=home
        self.jobs[home]=job;self.owners[job.vehicle_key]=home
        local controller=RestStop.new({
            arrived=function()return at_home end,
            set_enabled=function(_,value)call(job.vehicle,'SetAutopilotEnabled',value)end,
            enabled=function()return call(job.vehicle,'IsAutopilotEnabled')end,
            route=function()return route(job.vehicle)end,
            set_route=function(_,value)call(job.vehicle,'SetVehicleRoute',value)end,
            validate=function(_,_,intended)return validate(job,intended)end,
        })
        local ok,why=pcall(function()
            if player_present or needs_refuel or service_home then
                -- Preserve native driving/possession state for a player riding with autopilot.
                assert(call(job.vehicle,'IsAutopilotEnabled'),'player disabled autopilot')
                call(job.vehicle,'SetVehicleRoute',job.native_trip)
                assert(Core.same_route(route(job.vehicle),job.native_trip),'occupied dispatch route readback failed')
                assert(call(job.vehicle,'IsAutopilotEnabled'),'occupied dispatch changed autopilot')
                if service_home then
                    job.last_fuel_count=fuel_count(job.owner);job.last_home_fuel_watch=true
                    if not docked(job,1)then
                        assert(not valid(call(job.buildings[1],'GetDockedActor')),'home docking slot occupied')
                        assert(not call(job.owner,'HasActiveDriver'),'player entered before native docking')
                        assert(call(job.buildings[1],'DockActor',job.owner)==true,'native home docking rejected')
                        assert(docked(job,1),'native home docking readback failed')
                    end
                    job.initial_dock_deadline=api.now()+45
                    api.log('DEPOT_INITIAL_DOCK home='..home..' native=true no_possession_writes=true')
                end
            else
                controller:register(home,job.vehicle_key)
                -- A single owned home route + verified bay proximity can enroll an undocked idle truck.
                at_home=true
                local ticket=controller:arrive(home,job.vehicle_key)
                controller:dispatch(home,ticket,job.native_trip)
            end
            local status=call(job.vehicle,'GetAutopilotErrorStatus')
            if (player_present or service_home) and status==3 then job.enable_deadline=api.now()+5 end
            if needs_refuel then job.fuel_deadline=api.now()+45 end
            assert(status==0 or job.enable_deadline or((status==3 or status==4)and job.fuel_deadline),'trip failed to enable')
            job.state='Trip';job.started=api.now();self.active[#self.active+1]=job
        end)
        if not ok then
            stop(job,why)
            -- Keep deferred occupied failures observable so autopilot-off can release leases.
            if job.state=='Suspended' or job.state=='Recovery' then self.active[#self.active+1]=job end
            error(why)
        end
        api.log('DEPOT_DISPATCH home='..home..' stops=4 home_first_last=true route_verified=true')
        return job
    end
    local function attach_cargo(home,requester,vehicle,resource,expected,restoring)
        local previous=self.jobs[home]
        assert(not previous or previous.state=='Idle','depot already assigned or recovering')
        assert(home~=requester,'depot cannot request cargo')
        api.authority(vehicle)
        local owner=call(vehicle,'GetOwnerVehicle');assert(valid(owner),'cargo truck unavailable')
        assert(call(vehicle,'IsAutopilotEnabled')and not call(owner,'HasActiveDriver'),'truck opted out or player-driven')
        local ids={}
        for _,station in pairs(api.stations())do ids[key(guid(station))]=station end
        local home_id=assert(ids[home],'cargo home missing');local target=assert(ids[requester],'cargo requester missing')
        local b=audit(home_id);local data=CargoAudit.station({call=call,valid=valid,find=api.find},target)
        assert(not data.depot and data.role=='requester'and data.configured and data.resource,'explicit UNLOAD item filter required')
        if resource then assert(same(resource,data.resource),'cargo requester filter changed')else resource=data.resource end
        local inventory=CargoAudit.inventory({call=call,valid=valid},call(owner,'GetStorageInventory'),resource)
        assert(inventory.kinds==0 or(inventory.kinds==1 and inventory.items[data.item]),'cargo does not match requester filter')
        local quantity=inventory.quantity;local native=route(vehicle)
        if not restoring then
            assert(not StockPolicy.storage(data.label),'unknown-source retained cargo cannot target [S] storage')
            assert(#native==1 and key(native[1])==home,'cargo-priority truck needs depot-only route')
            assert(quantity>0 and quantity==expected,'retained cargo changed before dispatch')
            assert(call(data.building,'HasPower')and not call(data.building,'IsProductionPaused'),'cargo requester disabled')
            assert(data.station_quantity==0 and data.station_capacity>=quantity,'requester no longer empty or cannot hold cargo')
            assert(not valid(call(data.building,'GetDockedActor')),'requester occupied')
            assert(call(owner,'HasFuel'),'cargo-priority truck has no fuel')
            assert(distance(position(owner),call(call(b,'GetDockingPathNode'),'K2_GetActorLocation'))<1000,'cargo-priority truck is not at its depot')
        else
            local valid_route=#native==1 and key(native[1])==home or
                (#native==3 and key(native[1])==home and key(native[2])==requester and key(native[3])==home)or
                (#native==2 and ((key(native[1])==requester and key(native[2])==home)or(key(native[1])==home and key(native[2])==requester)))
            if not valid_route and #native==2 and key(native[1])==home then
                local anchor=ids[key(native[2])]
                valid_route=anchor and pcall(audit,anchor)or false
            end
            assert(valid_route,'native schedule does not match persisted cargo intent')
        end
        assert(not self.owners[vehicle:GetFullName()]or self.owners[vehicle:GetFullName()]==home,'cargo truck already owned')
        assert(not self.reservations[requester]or self.reservations[requester]==home,'requester reserved by another job')
        local p=position(owner)
        local job={kind='cargo',home=home,vehicle=vehicle,vehicle_key=vehicle:GetFullName(),owner=owner,resource=resource,
            requester_guid=guid(target),identifiers={home_id,target},buildings={b,data.building},trip={guid(target),guid(home_id)},
            native_trip=native,home_route={guid(home_id)},state='Recovery',resume_state='CargoDelivery',recovery=Recovery.new(api.now()),
            recovery_position=p,progress_at=api.now(),initial=p,loaded=quantity,unloaded=0,truckload=quantity,vehicle_capacity=inventory.capacity,started=api.now(),
            cycle=previous and(previous.cycle or 0)+1 or 1,visited=0,departed=true,events={}}
        if previous then for i=#self.active,1,-1 do if self.active[i]==previous then table.remove(self.active,i)end end end
        self.reservations[requester]=home;self.jobs[home]=job;self.owners[job.vehicle_key]=home;self.active[#self.active+1]=job
        trace(job,restoring and 'CARGO_REUSE_RESTORED'or'CARGO_REUSE_ASSIGNED','amount='..quantity..' requester='..requester..' provider=none')
        return job
    end
    function self:begin_cargo(home,requester,resource,expected)
        local vehicle
        for _,v in pairs(api.vehicles())do
            local r=route(v)
            if #r==1 and key(r[1])==home then assert(not vehicle,'multiple trucks claim cargo home');vehicle=v end
        end
        assert(vehicle,'cargo-priority depot truck missing')
        return attach_cargo(home,requester,vehicle,resource,expected,false)
    end
    function self:restore_cargo(home,requester,vehicle)
        if self.jobs[home]then return end
        return attach_cargo(home,requester,vehicle,nil,nil,true)
    end
    function self:restore_persisted(home,first,second,vehicle)
        if self.jobs[home]then return end
        api.authority(vehicle)
        local r=route(vehicle);assert(call(vehicle,'IsAutopilotEnabled'),'player opted out of persisted trip')
        assert(not call(call(vehicle,'GetOwnerVehicle'),'HasActiveDriver'),'player is driving persisted trip')
        local identifiers,buildings={},{}
        for _,station in pairs(api.stations())do
            local id=key(guid(station))
            for index,wanted in ipairs({home,first,second})do if id==wanted then identifiers[index]=station;buildings[index]=call(station,'GetStation')end end
        end
        for index=1,3 do assert(identifiers[index],'persisted endpoint missing: '..index)end
        audit(identifiers[1])
        local owner=call(vehicle,'GetOwnerVehicle');assert(valid(owner),'persisted vehicle unavailable')
        local cargo=CargoAudit.station({call=call,valid=valid,find=api.find},identifiers[3])
        assert(cargo.configured and cargo.role=='requester' and cargo.resource,'persisted requester filter unavailable')
        assert(call(buildings[2],'GetIsInLoadMode'),'persisted provider is no longer LOAD')
        local carried=CargoAudit.inventory({call=call,valid=valid},call(owner,'GetStorageInventory'),cargo.resource)
        assert(carried.kinds==0 or(carried.kinds==1 and carried.items[cargo.item]),'persisted cargo differs from requester filter')
        local quantity=call(call(owner,'GetStorageInventory'),'GetNumItems',cargo.resource)
        local job={home=home,vehicle=vehicle,vehicle_key=vehicle:GetFullName(),owner=owner,
            identifiers=identifiers,buildings=buildings,trip={guid(identifiers[2]),guid(identifiers[3]),guid(identifiers[1])},
            home_route={guid(identifiers[1])},native_trip=r,resource=cargo.resource,loaded=quantity,unloaded=0,
            cycle=0,state='Restored',visited=0,departed=true,events={},initial=position(owner)}
        -- Restored jobs deliver existing cargo or return empty; they never pick up again.
        assert(not self.reservations[second]or self.reservations[second]==home,'persisted endpoint conflict')
        self.reservations[second]=home;job.provider_released=true
        self.jobs[home]=job;self.owners[job.vehicle_key]=home;self.active[#self.active+1]=job
        stop(job,'session restored; reconcile actual cargo and return home before new work')
        trace(job,'RESTORED_RECOVERY','no forced stop; actual cargo='..quantity)
    end
    function self:ensure_home(home,identifier,vehicle)
        local previous=self.jobs[home]
        if previous and previous.state~='Idle'then return end
        if not call(vehicle,'IsAutopilotEnabled')then return end
        local r=route(vehicle);if #r~=1 or key(r[1])~=home then return end
        local owner=call(vehicle,'GetOwnerVehicle');if not valid(owner)or call(owner,'HasActiveDriver')then return end
        local b=call(identifier,'GetStation');local p=position(owner)
        if same(call(b,'GetDockedActor'),owner)or distance(p,call(call(b,'GetDockingPathNode'),'K2_GetActorLocation'))<1000 then return end
        assert(call(call(owner,'GetStorageInventory'),'IsEmpty'),'orphan truck carries cargo; original destination required')
        local job=previous or {home=home,vehicle=vehicle,vehicle_key=vehicle:GetFullName(),owner=owner,
            identifiers={identifier},buildings={b},trip={},home_route=r,native_trip=r,loaded=0,unloaded=0,cycle=0,state='Idle'}
        job.owner=owner;job.native_trip=r;self.jobs[home]=job;self.owners[job.vehicle_key]=home
        stop(job,'idle truck away from assigned bay')
        local present=false;for _,active in ipairs(self.active)do if active==job then present=true end end
        if not present then self.active[#self.active+1]=job end
    end
    function self:status(home)
        local job=assert(self.jobs[home],'bay not enrolled')
        return job
    end
    function self:guard(home,first,second)
        local job=self:status(home);assert(job.state=='Idle','guard requires held idle truck')
        local ok=pcall(self.begin,self,home,home,second)
        assert(not ok,'invalid replacement assignment was accepted')
        assert(call(job.vehicle,'IsAutopilotEnabled') and Core.same_route(route(job.vehicle),job.home_route),'rejected assignment released truck')
        api.log('DEPOT_GUARD_PASS rejected=true held=true route=home-only')
    end
    function self:break_test_road(home)
        local job=self:status(home);assert(job.state=='Idle','road fault requires held idle truck')
        local node=call(job.buildings[2],'GetDockingPathNode')
        local arriving=Bridge.values(call(node,'GetArrivingConnections'))
        assert(#arriving==1,'fault test requires exactly one incoming segment')
        call(arriving[1],'K2_DestroyActor')
        api.log('DEPOT_ROAD_FAULT snapshot_only=true incoming_segment_removed=true')
    end
    function self:blocked_check(home,first,second)
        local job=self:status(home);assert(job.state=='Idle','blocked test requires held idle truck')
        local ok,why=pcall(self.begin,self,home,first,second)
        assert(not ok,'disconnected route unexpectedly accepted')
        assert(tostring(why):find('unreachable',1,true) or tostring(why):find('disconnected',1,true),'unexpected block reason: '..tostring(why))
        assert(call(job.vehicle,'IsAutopilotEnabled') and Core.same_route(route(job.vehicle),job.home_route),'blocked route released truck')
        api.log('DEPOT_BLOCKED_PASS rejected=true held=true route=home-only reason='..tostring(why))
    end
    function self:cancel(home)
        local job=assert(self.jobs[home],'bay not enrolled');stop(job,'cancelled')
    end
    function self:watch()
        if self.watching then return end;self.watching=true
        api.log('DEPOT_HOOK disabled=true reason=unverified-interface-hook; bounded polling active')
        local game_loop=LoopInGameThreadWithDelay or LoopAsync
        game_loop(250,function()
            if self.pending or(#self.active==0 and not next(self.fuel_service.pending))then return false end
            self.pending=true
            ExecuteInGameThread(function()
                local ok,why=pcall(self.tick,self);self.pending=false
                if not ok then api.log('DEPOT_WATCHDOG_FAILED '..tostring(why))end
            end)
            return false
        end)
    end
    return self
end
return M
