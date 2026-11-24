-- SPDX-License-Identifier: GPL-3.0-only
-- Demand-driven read-only diagnostics: one station, vehicle or capacity calculation per pass.
local B=require('Bridge');local C=require('Core');local Audit=require('CargoAudit');local Json=require('Json')
local M={}
local function key(g)return C.guid_key(C.guid(g))end
local function path(o)return o:GetFullName():match('^%S+ (.+)$')or o:GetFullName()end
function M.new(api)
    local self={sequence=0,force_until=0,next_start=0,capacity_cache={}}
    local now=api.now or os.time
    local function inventory(inv,resource)
        local data=Audit.inventory({call=api.call,valid=api.valid,capacity_cache=self.capacity_cache},inv,resource)
        local contents=Json.array();for item,entry in pairs(data.items)do contents[#contents+1]={item=item,amount=entry.count}end
        table.sort(contents,function(a,b)return a.item<b.item end)
        local quantity=0;for _,entry in ipairs(contents)do quantity=quantity+entry.amount end
        return {items=contents,amount=quantity,capacity=data.capacity,slots=data.slots,kinds=data.kinds,item=data.item},data.resource
    end
    local function position(actor)
        local p=api.call(actor,'K2_GetActorLocation');return {x=math.floor(p.X/100),y=math.floor(p.Y/100),z=math.floor(p.Z/100)}
    end
    local function active()
        if now()<self.force_until then return true end
        local f=io.open(api.demand_path,'rb');if not f then return false end
        local stamp=tonumber(f:read(64));f:close();return stamp and now()-stamp<30 and stamp<=now()+5
    end
    function self:request()self.force_until=now()+60;self.next_start=0 end
    function self:reset()self.cycle=nil;self.next_start=0;self.capacity_cache={}end
    local function start()
        api.refresh()
        assert(api.valid(api.context()),'world unavailable')
        local runtime=api.runtime();local jobs={}
        for home,job in pairs(runtime.jobs)do jobs[job.vehicle_key]={home=home,job=job}end
        local queue={};local total_vehicles=0;local total_stations=0
        for id,v in pairs(api.vehicles())do queue[#queue+1]={kind='vehicle',id=id,object=v};total_vehicles=total_vehicles+1 end
        for id,s in pairs(api.stations())do queue[#queue+1]={kind='station',id=id,object=s};total_stations=total_stations+1 end
        table.sort(queue,function(a,b)if a.kind~=b.kind then return a.kind=='vehicle'end;return a.id<b.id end)
        local world=api.call(api.context(),'GetWorld')
        self.cycle={queue=queue,index=1,jobs=jobs,world=world,resources={},vehicle_refs={},types={},
            stations=Json.array(),vehicles=Json.array(),capacities=Json.array(),errors=Json.array(),
            started=now(),total_vehicles=total_vehicles,total_stations=total_stations}
        self.capacity_cache={}
    end
    local function vehicle_sample(c,task)
        local v=task.object;local owner=api.call(v,'GetOwnerVehicle');assert(api.valid(owner),'vehicle owner unavailable')
        local route=Json.array();for _,g in ipairs(B.values(api.call(v,'GetVehicleRoute'),C.guid))do route[#route+1]=key(g)end
        local cargo=inventory(api.call(owner,'GetStorageInventory'))
        local fuel,fuel_resource=inventory(api.call(owner,'GetFuelInventory'))
        local record={id=task.id,key=v:GetFullName(),owner_key=owner:GetFullName(),name=B.text(api.call(v,'GetVehicleName')),
            class=path(api.call(v,'GetVehicleClass')),route=route,autopilot=api.call(v,'IsAutopilotEnabled'),
            driver=api.call(owner,'HasActiveDriver'),has_fuel=api.call(owner,'HasFuel'),native_error=api.call(v,'GetAutopilotErrorStatus'),
            docking=api.call(v,'IsCurrentlyDocking'),target=api.call(v,'GetCurrentTargetWaypointIndex'),cargo=cargo,fuel=fuel,sampled_at=now()}
        local p,r={X=0,Y=0,Z=0},{};api.call(owner,'GetVehicleLocationAndRotation',p,r)
        record.position={x=math.floor(p.X/100),y=math.floor(p.Y/100),z=math.floor(p.Z/100)}
        local owned=c.jobs[record.key]
        if owned then
            local j=owned.job;record.home=owned.home
            record.job={kind=j.kind or 'pickup',state=j.state,home=owned.home,provider=j.kind~='cargo'and j.trip[1]and key(j.trip[1])or nil,
                requester=j.requester_guid and key(j.requester_guid)or(j.trip[2]and key(j.trip[2])),
                provider_released=j.provider_released==true,item=j.resource and path(j.resource),loaded=j.loaded,unloaded=j.unloaded,truckload=j.truckload,
                visited=j.visited,started=j.started,cycle=j.cycle,error=j.error,recovery=j.recovery and j.recovery.state,
                recovery_details=j.recovery and {attempts=j.recovery.attempts,next_retry=j.recovery.next_retry,since=j.recovery.since,last_error=j.recovery.last_error}}
        end
        c.vehicles[#c.vehicles+1]=record;c.vehicle_refs[record.key]={object=v,owner=owner,inventory=api.call(owner,'GetStorageInventory'),fuel_resource=fuel_resource,record=record}
    end
    local function station_sample(c,task)
        local data=Audit.station({call=api.call,valid=api.valid,find=api.find,capacity_cache=self.capacity_cache},task.object)
        local contents=Json.array();for item,entry in pairs(data.items)do contents[#contents+1]={item=item,amount=entry.count}end
        table.sort(contents,function(a,b)return a.item<b.item end)
        if data.resource then c.resources[data.item]=data.resource end
        local occupant=api.call(data.building,'GetDockedActor')
        local record={id=task.id,guid=data.guid,name=data.label,role=data.role,configured=data.configured,is_depot=data.depot,
            item=data.item,amount=data.quantity,capacity=data.capacity,contents=contents,item_source=data.item_source,
            powered=api.call(data.building,'HasPower'),standby=api.call(data.building,'IsProductionPaused'),
            occupied=api.valid(occupant)==true,position=position(data.building),sampled_at=now()}
        local road_ok,road=pcall(function()
            local node=api.call(data.building,'GetDockingPathNode')
            assert(api.valid(node),'docking path node missing')
            return {incoming=#B.values(api.call(node,'GetArrivingConnections')),outgoing=#B.values(api.call(node,'GetLeavingConnections'))}
        end)
        if road_ok then record.road=road end
        local hint
        for _,ref in pairs(c.vehicle_refs)do
            local r=ref.record.route
            if (#r==1 and r[1]==data.guid)or(ref.record.home==data.guid)then hint=ref.fuel_resource;break end
        end
        record.fuel=inventory(api.call(data.building,'GetFuelInventory'),hint)
        c.stations[#c.stations+1]=record
    end
    local function capacity_tasks(c)
        local by={};for _,s in ipairs(c.stations)do by[s.guid]=s end
        for _,v in ipairs(c.vehicles)do
            if not v.home and #v.route==1 and by[v.route[1]]and by[v.route[1]].is_depot then v.home=v.route[1]end
            local home=v.home and by[v.home]
            v.managed=home~=nil and home.is_depot or false
            v.home_powered=home~=nil and home.powered and not home.standby or false
            if v.managed and v.home_powered and v.autopilot then
                local typekey=v.class..'|'..v.cargo.slots
                if not c.types[typekey]then
                    local typeinfo={key=typekey,class=v.class,slots=v.cargo.slots,sampled_vehicle=v.id,by_item={}}
                    c.types[typekey]={info=typeinfo,ref=c.vehicle_refs[v.key]};c.capacities[#c.capacities+1]=typeinfo
                    for item,resource in pairs(c.resources)do c.queue[#c.queue+1]={kind='capacity',typekey=typekey,item=item,resource=resource}end
                end
            end
        end
        c.capacity_tasks_added=true
    end
    local function finish(c)
        local scheduler=api.scheduler();local runtime=api.runtime();local reservations={}
        for station,home in pairs(runtime.reservations)do reservations[station]=home end
        self.sequence=self.sequence+1
        local snapshot={schema_version=1,sequence=self.sequence,exported_at=now(),collection_started_at=c.started,
            collection_seconds=now()-c.started,world=c.world:GetFullName(),stations=c.stations,vehicles=c.vehicles,
            capacity_samples=c.capacities,errors=c.errors,dispatcher={registration_supported=true,enabled=scheduler.enabled,storage_tag_rule_enabled=scheduler.storage_tag_rule_enabled==true,cargo_reuse_enabled=scheduler.cargo_reuse_enabled==true,reservations=reservations,
                reasons=scheduler.reasons,orphans=scheduler.orphans and scheduler.orphans.records or {},request_checks=scheduler.request_checks or {},last_choice=scheduler.last_choice,
                pipeline={catalog_age=scheduler.catalog and now()-(scheduler.catalog.built_at or now()),
                    sampling=scheduler.scan~=nil,sampled=scheduler.scan and scheduler.scan.index-1 or 0,total_stations=#scheduler.stations,
                    ranking=scheduler.round~=nil,ranked=scheduler.round and scheduler.round.index-1 or 0,total_homes=#scheduler.homes,
                    next_search_in=math.max(0,(scheduler.next_search or 0)-now())},
                stats={rank_invalidations=scheduler.paths.rank_invalidations or 0,dispatches=scheduler.dispatches or 0,dispatch_failures=scheduler.dispatch_failures or 0,samples=scheduler.samples,searches=scheduler.searches,graph_builds=scheduler.paths.builds,graph_hits=scheduler.paths.hits}}}
        Json.write(api.output_path,snapshot);self.last_snapshot=snapshot;self.cycle=nil;self.next_start=now()+(api.interval or 5)
    end
    function self:step()
        -- Host systemd watches this file. No shell, process launch or browser API in Lua.
        if api.lifecycle_path and now()>=(self.next_lifecycle or 0)then
            local f=assert(io.open(api.lifecycle_path,'wb'),'cannot write dashboard mod heartbeat')
            assert(f:write(tostring(now())));assert(f:close());self.next_lifecycle=now()+5
        end
        if api.ready and not api.ready()then self.cycle=nil;self.next_start=now()+2;return end
        if not self.cycle then
            if now()<self.next_start or not active()then return end
            start()
        end
        local c=self.cycle
        assert(api.valid(c.world)and api.valid(api.context())and B.same_object(c.world,api.call(api.context(),'GetWorld'),api.valid),'world changed during telemetry scan')
        local task=c.queue[c.index]
        if not task then
            if not c.capacity_tasks_added then capacity_tasks(c);task=c.queue[c.index]end
            if not task then finish(c);return end
        end
        local ok,why=pcall(function()
            if task.kind=='vehicle'then vehicle_sample(c,task)
            elseif task.kind=='station'then station_sample(c,task)
            else
                local t=c.types[task.typekey]
                local info=Audit.inventory({call=api.call,valid=api.valid,capacity_cache=self.capacity_cache},t.ref.inventory,task.resource)
                t.info.by_item[task.item]=info.capacity
            end
        end)
        if not ok then c.errors[#c.errors+1]={kind=task.kind,id=task.id or task.item,error=tostring(why)}end
        c.index=c.index+1
        if now()~=(self.progress_at or 0)then
            self.progress_at=now();Json.write(api.progress_path,{started_at=c.started,completed=c.index-1,total=#c.queue,exported_at=now()})
        end
    end
    function self:watch()
        if self.watching then return end;self.watching=true
        local loop=LoopInGameThreadWithDelay or LoopAsync
        loop(api.pass_ms or 100,function()
            if self.pending then return false end;self.pending=true
            ExecuteInGameThread(function()
                local ok,why=pcall(self.step,self);self.pending=false
                if not ok then
                    self.cycle=nil;self.next_start=now()+5
                    if now()>=(self.next_error or 0)then api.log('DASHBOARD_EXPORT_WAIT reason='..tostring(why));self.next_error=now()+30 end
                end
            end)
            return false
        end)
    end
    return self
end
return M
