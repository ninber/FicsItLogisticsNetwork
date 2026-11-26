local Audit=require('CargoAudit')
local Bridge=require('Bridge')
local Core=require('Core')
local Graph=require('RoadGraph')
local Policy=require('StockPolicy')
local M={}
function M.add(api,catalog,data)
    if (not api.available or api.available(data.guid)) and not api.valid(api.call(data.building,'GetDockedActor')) and
        not data.depot and data.configured and data.kinds<=1 and data.resource and data.capacity>0 then
        if data.role=='provider' and data.station_quantity>0 then catalog.providers[#catalog.providers+1]=data
        elseif data.role=='requester' and data.station_quantity==0 then catalog.requesters[#catalog.requesters+1]=data end
    end
end
function M.sample(api,station)return Audit.station(api,station)end
function M.collect(api)
    local catalog={providers={},requesters={}}
    for _,station in pairs(api.stations())do M.add(api,catalog,M.sample(api,station))end
    return catalog
end
function M.better(a,b)
    if not b then return true end
    if (a.cargo_delivery==true)~=(b.cargo_delivery==true)then return a.cargo_delivery==true end
    if a.distance~=b.distance then return a.distance<b.distance end
    return a.home<b.home
end
function M.select(api,home,catalog)
    catalog=catalog or M.collect(api)
    local providers,requesters=catalog.providers,catalog.requesters
    local vehicle
    for _,v in pairs(api.home_vehicle and {api.home_vehicle(home)} or api.vehicles())do
        local route=Bridge.values(api.call(v,'GetVehicleRoute'),Core.guid)
        if #route==1 and Core.guid_key(route[1])==home then assert(not vehicle,'multiple trucks claim home');vehicle=v end
    end
    assert(vehicle,'depot-only truck not found')
    api.authority(vehicle)
    assert(api.call(vehicle,'IsAutopilotEnabled'),'enable autopilot on a depot-only route to enroll')
    local owner=api.call(vehicle,'GetOwnerVehicle');assert(api.valid(owner),'truck actor missing')
    assert(not api.call(owner,'HasActiveDriver'),'player-driven truck is unavailable')
    local cargo=api.call(owner,'GetStorageInventory');local carried
    if not api.call(cargo,'IsEmpty')then
        carried=Audit.inventory(api,cargo)
        assert(carried.kinds==1 and carried.resource and carried.quantity>0,'retained cargo must contain exactly one item')
    end
    local preset=api.call(owner,'GetVehiclePathPreset')
    local home_guid,home_building
    for _,station in pairs(api.home_station and {api.home_station(home)} or api.stations())do
        local g=Core.guid(api.call(station,'GetPathNodeGUID'))
        if Core.guid_key(g)==home then
            local b=api.call(station,'GetStation')
            assert(api.call(b,'HasPower') and not api.call(b,'IsProductionPaused'),'home is unpowered or in standby')
            home_guid=g;home_building=b;break
        end
    end
    assert(home_guid,'home node missing')
    if not api.call(owner,'HasFuel')then
        local fuel=api.call(home_building,'GetFuelInventory')
        assert(api.valid(fuel) and not api.call(fuel,'IsEmpty'),'vehicle and depot have no fuel')
    end
    local function same(a,b)return Bridge.same_object(a,b,api.valid)end
    local snapshots={}
    local function distances_to(network,id)
        if api.distances then return api.distances(network,preset,id)end
        local network_key=network:GetFullName()
        if not snapshots[network_key]then snapshots[network_key]=Graph.snapshot(api,network,preset)end
        return snapshots[network_key]:distances(id)
    end
    local candidates={}
    local function report(requester,record)
        if api.report_request then pcall(api.report_request,home,requester.guid,record)end
    end
    local home_network=api.call(api.context(),'FindNetworkByPathNodeGuid',home_guid)
    if not api.valid(home_network)then error('home network missing')end
    local home_distances=distances_to(home_network,home)
    if carried then
        for _,requester in ipairs(requesters)do
            if not Policy.storage(requester.label)and requester.item==carried.item and requester.station_quantity==0 and requester.station_capacity>=carried.quantity and
                (not api.available or api.available(requester.guid))then
                local target=Core.guid(api.call(requester.identifier,'GetPathNodeGUID'))
                local network=api.call(api.context(),'FindNetworkByPathNodeGuid',target)
                if api.valid(network)and same(network,home_network)then
                    local delivery=distances_to(network,requester.guid)[home];local returning=home_distances[requester.guid]
                    if not delivery or not returning then report(requester,{item=requester.item,reason='unreachable_legs',need=carried.quantity})end
                    if delivery and returning then candidates[#candidates+1]={requester=requester,target=target,network=network,distance=delivery+returning,delivery=delivery,returning=returning}end
                end
            end
        end
        table.sort(candidates,function(a,b)if a.distance~=b.distance then return a.distance<b.distance end;return a.requester.guid<b.requester.guid end)
        for _,selected in ipairs(candidates)do
            if api.call(selected.network,'FindVehiclePath',home_guid,selected.target,preset,{})and
                api.call(selected.network,'FindVehiclePath',selected.target,home_guid,preset,{})then
                report(selected.requester,{reason='retained_cargo_selected',need=carried.quantity,distance=selected.distance})
                api.log('CARGO_REUSE_SELECTED home='..home..' requester='..selected.requester.guid..' item='..carried.item..' amount='..carried.quantity..' road_cm='..math.floor(selected.distance))
                return {home=home,kind='cargo',cargo_delivery=true,requester=selected.requester.guid,item=carried.item,amount=carried.quantity,
                    truckload=carried.capacity,distance=selected.distance,pickup_distance=0,delivery_distance=selected.delivery,return_distance=selected.returning}
            end
        end
        error('retained cargo has no empty matching requester with enough capacity and a reachable round trip')
    end
    for _,requester in ipairs(requesters)do
        local evidence={item=requester.item,reason='different_network',matching=0,full=0,candidates=0,storage_blocked=0,failures={}}
        local target=Core.guid(api.call(requester.identifier,'GetPathNodeGUID'))
        local network=api.call(api.context(),'FindNetworkByPathNodeGuid',target)
        if api.valid(network) and same(network,home_network) then
            local distances=distances_to(network,requester.guid)
            local truckload=Audit.inventory(api,cargo,requester.resource).capacity
            evidence.need=truckload;evidence.reason='no_matching_provider'
            for _,provider in ipairs(providers)do
                if provider.item==requester.item then
                    evidence.matching=evidence.matching+1
                    if not Policy.pair_allowed(provider.label,requester.label)then evidence.storage_blocked=evidence.storage_blocked+1 end
                end
                if (not api.available or(api.available(provider.guid) and api.available(requester.guid))) and provider.item==requester.item and Policy.pair_allowed(provider.label,requester.label)and
                    requester.station_capacity>=truckload and
                    Policy.evaluate({enabled=true},'provider',provider.quantity,provider.capacity,truckload) then
                    evidence.full=evidence.full+1
                    local delivery=distances[provider.guid]
                    local returning=home_distances[requester.guid]
                    if not delivery then
                        local provider_network=api.call(api.context(),'FindNetworkByPathNodeGuid',Core.guid(api.call(provider.identifier,'GetPathNodeGUID')))
                        evidence.failures[#evidence.failures+1]={provider=provider.guid,leg='provider_to_requester',reason=not api.valid(provider_network)and 'provider_network_missing'or(not same(provider_network,network)and 'provider_other_network'or'no_vehicle_path')}
                    end
                    if not returning then evidence.failures[#evidence.failures+1]={provider=provider.guid,leg='requester_to_home',reason='no_vehicle_path'}end
                    if delivery and returning then
                        local pickup=distances_to(network,provider.guid)[home]
                        if pickup then evidence.candidates=evidence.candidates+1;candidates[#candidates+1]={provider=provider,requester=requester,network=network,target=target,
                            distance=pickup+delivery+returning,pickup=pickup,delivery=delivery,returning=returning,truckload=truckload}
                        else evidence.failures[#evidence.failures+1]={provider=provider.guid,leg='home_to_provider',reason='no_vehicle_path'}end
                    end
                end
            end
            evidence.reason=evidence.candidates>0 and 'route_candidate_available' or(evidence.full>0 and 'unreachable_legs' or(evidence.matching>0 and 'provider_short_or_ineligible' or 'no_matching_provider'))
        end
        if evidence.matching>0 and evidence.storage_blocked==evidence.matching then evidence.reason='storage_pair_blocked'end
        report(requester,evidence)
    end
    table.sort(candidates,function(a,b)
        if a.distance~=b.distance then return a.distance<b.distance end
        if a.requester.guid~=b.requester.guid then return a.requester.guid<b.requester.guid end
        return a.provider.guid<b.provider.guid
    end)
    for _,selected in ipairs(candidates)do
        local provider,requester=selected.provider,selected.requester
        local p=Core.guid(api.call(provider.identifier,'GetPathNodeGUID'))
        if api.call(selected.network,'FindVehiclePath',home_guid,p,preset,{}) and
            api.call(selected.network,'FindVehiclePath',p,selected.target,preset,{}) and
            api.call(selected.network,'FindVehiclePath',selected.target,home_guid,preset,{}) then
            api.log('CARGO_SELECTED home='..home..' provider='..provider.guid..' requester='..requester.guid..
                ' item='..requester.item..' supply='..provider.quantity..' requested_stock='..requester.quantity..
                ' capacity='..requester.capacity..' road_cm='..math.floor(selected.distance)..' truckload='..selected.truckload..
                ' pickup_cm='..math.floor(selected.pickup)..' delivery_cm='..math.floor(selected.delivery)..' return_cm='..math.floor(selected.returning))
            report(requester,{reason='selected',provider=provider.guid,need=selected.truckload,distance=selected.distance})
            return {home=home,provider=provider.guid,requester=requester.guid,item=requester.item,
                truckload=selected.truckload,supply=provider.quantity,stock=requester.quantity,capacity=requester.capacity,
                distance=selected.distance,pickup_distance=selected.pickup,delivery_distance=selected.delivery,return_distance=selected.returning}
        end
        report(requester,{reason='native_path_rejected',provider=provider.guid,need=selected.truckload})
    end
    error('no explicitly configured active requester with a stocked, reachable matching provider')
end
return M
