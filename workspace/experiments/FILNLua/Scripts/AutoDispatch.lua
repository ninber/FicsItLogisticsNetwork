-- SPDX-License-Identifier: GPL-3.0-only
-- One station sample per pass. Busy trucks do not trigger inventory searches.
local Core=require('Core')
local Bridge=require('Bridge')
local Select=require('CargoDispatch')
local RoadCache=require('RoadCache')
local M={}
function M.new(api)
    local self={cursor=1,request_checks={},reasons={},enabled=false,journal=api.journal,homes={},stations={},rank_nodes={},
        cargo_reuse_enabled=true,storage_tag_rule_enabled=true,next_registry=0,next_search=0,paths=RoadCache.new(),samples=0,searches=0,refreshes=0}
    local now=api.now or os.time
    if api.journal then self.orphans=require('OrphanRoutes').new(api)end
    api.report_request=function(home,requester,record)
        self.request_checks[home]=self.request_checks[home]or {};record.at=now()
        self.request_checks[home][requester]=record
    end
    api.capacity_cache={}
    api.recovery_vehicles=function()return self.registry_vehicles end
    api.rank_nodes=function()return self.rank_nodes end
    api.rank_revision=function()return self.rank_revision or 0 end
    api.distances=function(network,preset,requester)return self.paths:distances(api,network,preset,requester)end
    api.home_vehicle=function(home)assert(not(self.duplicate_homes and self.duplicate_homes[home]),'multiple trucks claim home');return self.vehicles_by_home and self.vehicles_by_home[home]end
    api.home_station=function(home)return self.stations_by_home and self.stations_by_home[home]end
    local function refresh(runtime)
        api.refresh();self.refreshes=self.refreshes+1
        for home,checks in pairs(self.request_checks)do
            for requester,record in pairs(checks)do if now()-record.at>60 then checks[requester]=nil end end
            if not next(checks)then self.request_checks[home]=nil end
        end
        local observed={};local claims={};self.vehicles_by_home={};self.stations_by_home={};self.duplicate_homes={}
        for _,v in pairs(api.vehicles())do
            local ok,why=pcall(function()
                local route=Bridge.values(api.call(v,'GetVehicleRoute'),Core.guid)
                for _,g in ipairs(route)do Core.guid_key(g)end
                observed[#observed+1]={vehicle=v,route=route}
                if #route==1 then local k=Core.guid_key(route[1]);claims[k]=true;if self.vehicles_by_home[k]then self.duplicate_homes[k]=true end;self.vehicles_by_home[k]=v end
            end)
            if not ok then api.log('AUTO_REGISTRY_VEHICLE_SKIPPED reason='..tostring(why))end
        end
        for home,job in pairs(runtime.jobs)do claims[home]=true;self.vehicles_by_home[home]=job.vehicle or self.vehicles_by_home[home]end
        local recipe=api.find('/KDataForge/Gen/FILNRestStop.Gen_FILNRestStop_Recipe_RestStop')
        self.homes={};self.stations={};self.rank_nodes={}
        for _,station in pairs(api.stations())do
            local guid=Core.guid_key(Core.guid(api.call(station,'GetPathNodeGUID')))
            self.rank_nodes[#self.rank_nodes+1]=guid;self.stations[#self.stations+1]=station;self.stations_by_home[guid]=station
            if claims[guid]then
                local building=api.call(station,'GetStation')
                if api.valid(building) and Bridge.same_object(Bridge.value(building.mBuiltWithRecipe),recipe,api.valid)and api.call(building,'HasPower') and not api.call(building,'IsProductionPaused')then self.homes[#self.homes+1]=guid end
            end
        end
        table.sort(self.rank_nodes)
        local signature=table.concat(self.rank_nodes,'|')
        if signature~=self.rank_signature then self.rank_signature=signature;self.rank_revision=(self.rank_revision or 0)+1 end
        self.registry_vehicles=observed
        if self.orphans then self.orphans:refresh(observed,self.stations_by_home)end
        table.sort(self.homes);table.sort(self.stations,function(a,b)return a:GetFullName()<b:GetFullName()end)
        self.next_registry=now()+30;api.capacity_cache={}
        -- Keep the last completed catalog usable while the next one is sampled.
        -- Refresh is called only between rounds/scans, so their cursors remain coherent.
    end
    function self:probe(home,requester)
        refresh(api.runtime())
        local target,vehicle
        for _,station in pairs(api.stations())do
            local g=Core.guid(api.call(station,'GetPathNodeGUID'))
            if Core.guid_key(g)==requester then target=g end
        end
        for _,v in pairs(api.vehicles())do
            local route=Bridge.values(api.call(v,'GetVehicleRoute'),Core.guid)
            if (#route==1 and Core.guid_key(route[1])==home)or(#route==3 and Core.guid_key(route[3])==home)or(#route==4 and Core.guid_key(route[1])==home and Core.guid_key(route[4])==home)then vehicle=v;break end
        end
        assert(target and vehicle,'cache probe needs existing requester and enrolled home truck')
        local network=api.call(api.context(),'FindNetworkByPathNodeGuid',target)
        assert(api.valid(network),'requester network missing')
        local preset=api.call(api.call(vehicle,'GetOwnerVehicle'),'GetVehiclePathPreset')
        local before=self.paths.builds
        local first=self.paths:distances(api,network,preset,requester)
        local second=self.paths:distances(api,network,preset,requester)
        assert(first==second and self.paths.builds-before<=1,'native cache reuse failed')
        api.log('AUTO_CACHE_PROBE_PASS builds='..(self.paths.builds-before)..' hits='..self.paths.hits..' cached_home_distance='..tostring(first[home]))
    end
    function self:step()
        if self.recovered and now()>=(self.next_journal_recovery or math.huge)then self.recovered=false end
        if not self.recovered then
            if api.journal and next(api.journal.entries)then
                if not self.journal_refreshed then api.refresh();self.journal_refreshed=true end
                self.recovered=api.journal:recover(api,1)
            else self.recovered=true end
            if self.recovered then self.next_journal_recovery=now()+30;self.journal_refreshed=nil end
        end
        local runtime=api.runtime()
        if not self.enabled and not runtime.fuel_service then return end
        if now()>=self.next_registry and not self.scan and not self.round then refresh(runtime)end
        if self.enabled and self.orphans and runtime.restore_persisted then self.orphans:step()end
        -- Refueling belongs to the rest stop, even while new cargo dispatch is paused.
        if runtime.fuel_service and #self.homes>0 then
            self.fuel_cursor=(self.fuel_cursor or 0)%#self.homes+1
            local h=self.homes[self.fuel_cursor];local previous=runtime.jobs[h]
            if not previous or previous.state=='Idle'then
                local v=self.vehicles_by_home[h]
                if api.valid(v)then
                    if runtime.ensure_home then
                        local ok,why=pcall(runtime.ensure_home,runtime,h,self.stations_by_home[h],v)
                        if not ok then api.log('DEPOT_RETURN_PENDING home='..h..' reason='..tostring(why))end
                    end
                    local ok,why=pcall(runtime.fuel_service.ready,runtime.fuel_service,h,self.stations_by_home[h],v)
                    if not ok then api.log('DEPOT_FUEL_SERVICE_PENDING home='..h..' reason='..tostring(why))end
                end
            end
        end
        if not self.enabled then return end
        local home
        for _=1,#self.homes do
            self.cursor=(self.cursor-1)%#self.homes+1;local candidate=self.homes[self.cursor];self.cursor=self.cursor+1
            local previous=runtime.jobs[candidate]
            if previous and previous.state=='Idle' and api.journal and api.journal.entries[candidate]then api.journal:remove(candidate)end
            if previous and previous.state=='Detached' then
                local vehicle=previous.vehicle
                if api.valid(vehicle)then
                    local route=Bridge.values(api.call(vehicle,'GetVehicleRoute'),Core.guid)
                    if #route==1 and Core.guid_key(route[1])==candidate and api.call(vehicle,'IsAutopilotEnabled')then runtime.jobs[candidate]=nil;previous=nil end
                end
            end
            if (not api.home_available or api.home_available(candidate)) and (not previous or previous.state=='Idle') and not(api.journal and api.journal.entries[candidate])then home=candidate;break end
        end
        if not home then self.scan=nil;self.round=nil;return end
        if not self.catalog or self.scan or now()>=self.catalog.expires then
            if not self.scan then self.scan={index=1,started=now(),catalog={providers={},requesters={}}}end
            local station=self.stations[self.scan.index]
            if station then
                local ok,why=pcall(function()Select.add(api,self.scan.catalog,Select.sample(api,station))end);self.samples=self.samples+1
                if not ok then api.log('AUTO_SAMPLE_SKIPPED '..tostring(why))end
                self.scan.index=self.scan.index+1
                -- One sample/pass; an existing catalog can still serve ranking below.
                if not self.catalog then return end
            else
                self.catalog=self.scan.catalog;self.catalog.built_at=now();self.catalog.expires=now()+10;self.scan=nil
                table.sort(self.catalog.requesters,function(a,b)return a.guid<b.guid end)
            end
        end
        if now()<self.next_search then return end
        if not self.round then
            self.rank_rounds=(self.rank_rounds or 0)+1;self.round={index=1,best=nil,compared=0,id=self.rank_rounds}
            api.log('FLEET_RANK_BEGIN round='..self.round.id)
        end
        local candidate=self.homes[self.round.index]
        if candidate then
            self.round.index=self.round.index+1
            local previous=runtime.jobs[candidate]
            if (not api.home_available or api.home_available(candidate)) and (not previous or previous.state=='Idle') and not(api.journal and api.journal.entries[candidate])then
                self.searches=self.searches+1
                local ok,selection=pcall(function()
                    if runtime.fuel_service then
                        assert(runtime.fuel_service:ready(candidate,self.stations_by_home[candidate],self.vehicles_by_home[candidate]),'native depot fuel service pending')
                    end
                    return Select.select(api,candidate,self.catalog)
                end)
                if ok then
                    self.round.compared=self.round.compared+1
                    api.log('FLEET_CANDIDATE round='..self.round.id..' home='..candidate..' total_cm='..string.format('%.3f',selection.distance)..' provider='..tostring(selection.provider)..' requester='..selection.requester)
                    local best=self.round.best
                    if Select.better(selection,best)then self.round.best=selection end
                else self.reasons[candidate]=tostring(selection)end
            end
            return -- At most one vehicle comparison in a game-thread pass.
        end
        local selection=self.round.best;local ranked=self.round;self.round=nil;self.next_search=now()+(selection and 0 or 5)
        if not selection then return end
        home=selection.home
        api.log('FLEET_CHOICE round='..ranked.id..' home='..home..' total_cm='..string.format('%.3f',selection.distance)..' compared='..ranked.compared)
        local ok,result=pcall(function()
            -- Refresh the winning intent before claiming endpoints; stock/power may have changed.
            assert(not api.home_available or api.home_available(home),'home is excluded')
            local checked=Select.select(api,home,self.catalog)
            assert(checked.provider==selection.provider and checked.requester==selection.requester and checked.cargo_delivery==selection.cargo_delivery and checked.amount==selection.amount,'fleet intent changed during ranking')
            local resource=api.find(selection.item);assert(api.valid(resource),'item unavailable')
            if api.journal then
                if selection.cargo_delivery then api.journal:put_cargo(home,selection.requester)
                else api.journal:put(home,selection.provider,selection.requester)end
            end
            local job
            if selection.cargo_delivery then job=runtime:begin_cargo(home,selection.requester,resource,selection.amount)
            else job=runtime:begin(home,selection.provider,selection.requester,resource)end
            api.log('AUTO_DISPATCH_STARTED home='..home..' provider='..tostring(selection.provider)..' requester='..selection.requester..' truckload='..selection.truckload..' cargo_reuse='..tostring(selection.cargo_delivery==true)..' total_cm='..math.floor(selection.distance))
            return job
        end)
        if not ok and api.journal and not runtime.jobs[home]then api.journal:remove(home)end
        if ok then self.dispatches=(self.dispatches or 0)+1;self.last_choice={home=home,kind=selection.cargo_delivery and 'cargo'or'pickup',amount=selection.amount,provider=selection.provider,requester=selection.requester,total_cm=selection.distance,at=now()}end
        if not ok then self.dispatch_failures=(self.dispatch_failures or 0)+1;self.next_search=now()+5 end
        local reason=ok and 'dispatched' or tostring(result)
        if self.reasons[home]~=reason then self.reasons[home]=reason;if not ok then api.log('AUTO_DISPATCH_WAIT home='..home..' reason='..reason)end end
    end
    return self
end
return M
