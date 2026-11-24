-- SPDX-License-Identifier: GPL-3.0-only
local Core = require('Core')
local Config = require('Config')
local Bridge = require('Bridge')
local test_run_ok,test_run=pcall(require,'AutoTestRun')
local test_prepared=false
local script_source=debug.getinfo(1,'S').source
local script_dir=script_source:sub(1,1)=='@' and script_source:sub(2):match('^(.*)[/\\][^/\\]+$')
local registry = Core.new()
local vehicles, stations, context = {}, {}, nil
local pending, fatal, cursor = false, false, 1
local migration_verifier
local fleet_test
local function log(message) print('[FILN-Lua] '..tostring(message)..'\n') end
local function valid(object)
    if object==nil then return false end
    local ok,result=pcall(function()return object:IsValid()end)
    return ok and result==true
end
local function call(object, method, ...)
    assert(valid(object), 'object unavailable for '..method)
    return object[method](object, ...)
end
local world_readiness=require('WorldReadiness').new({now=os.time,valid=valid,call=call,find=function()return FindAllOf('FGVehicleSubsystem')or {}end})
local function loaded_world()
    local ready=world_readiness:ready(context)
    if ready then context=ready;return true end
    return false
end
local values = Bridge.values
local function route(v) return values(call(v,'GetVehicleRoute'),Core.guid) end
local function vehicle(id) return assert(vehicles[id], 'unknown vehicle; use filn_lua.scan') end
local function station(id) return assert(stations[id], 'unknown station; use filn_lua.scan') end
local function building(id) return call(station(id),'GetStation') end
local function authority(v)
    assert(valid(context),'scan a loaded save first')
    assert(call(v,'HasAuthority') == true,'server/host authority required')
    assert(call(v,'GetWorld'):GetFullName()==call(context,'GetWorld'):GetFullName(),'world changed; restart game and scan again')
end
local function audit_home(id)
    return Bridge.home_audit(station(id),call,valid,StaticFindObject)
end
local function home_ok(id)
    local audit=audit_home(id)
    assert(audit.eligible,audit.reason)
    return audit.building
end
local function parked(v,home)
    return call(home,'GetDockedActor') == call(v,'GetOwnerVehicle') and call(v,'IsAutopilotEnabled')==false
end
local function path_guid(id) return Core.guid(call(station(id),'GetPathNodeGUID')) end
local function validate_legs(v,home,p,q)
    local preset=call(call(v,'GetOwnerVehicle'),'GetVehiclePathPreset')
    assert(valid(preset),'vehicle has no compatible path preset')
    local nodes={path_guid(home),path_guid(p),path_guid(q),path_guid(home)}
    for i=1,3 do
        local network=call(context,'FindNetworkByPathNodeGuid',nodes[i])
        assert(valid(network),'missing path network on leg '..i)
        local other=call(context,'FindNetworkByPathNodeGuid',nodes[i+1])
        assert(valid(other) and network:GetFullName()==other:GetFullName(),'disconnected leg '..i)
        local out={}
        assert(call(network,'FindVehiclePath',nodes[i],nodes[i+1],preset,out)==true,'unreachable directed leg '..i)
        -- Out-array marshalling is version-sensitive. True return validates reachability;
        -- no distance is inferred from its contents.
    end
    return {nodes[2],nodes[3],nodes[4]}
end
local function scan(quiet)
    assert(next(registry.pairs)==nil,'IDs are session-bound; cannot rescan after pairing. Restart game to discard this experiment session.')
    local candidates=(quiet and valid(context))and {context}or(FindAllOf('FGVehicleSubsystem') or {})
    local found={}
    for _,c in ipairs(candidates) do if valid(c) and call(c,'HasAuthority') then found[#found+1]=c end end
    assert(#found==1,'expected one authoritative vehicle subsystem in loaded save; found '..#found)
    context=found[1]
    vehicles,stations={},{}
    local vs=values(call(context,'GetAllVehicles'))
    local ss=values(call(context,'GetDockingStations'))
    table.sort(vs,function(a,b) return a:GetFullName()<b:GetFullName() end)
    table.sort(ss,function(a,b) return a:GetFullName()<b:GetFullName() end)
    for i,v in ipairs(vs) do
        local id='V'..i; vehicles[id]=v
        if not quiet then log(id..' '..v:GetFullName())end
    end
    for i,s in ipairs(ss) do
        local id='S'..i; stations[id]=s
        local ok,guid=pcall(function() return Core.guid_key(Core.guid(call(s,'GetPathNodeGUID'))) end)
        if not quiet then log(id..' '..s:GetFullName()..' guid='..(ok and guid or ('unavailable ('..tostring(guid)..')')))end
    end
    if not quiet then log('scan complete; IDs valid only for this game session. No vehicle was modified.')end
end
local function recover(id,why)
    registry:recover(id,tostring(why))
    pcall(function() call(vehicle(id),'SetAutopilotEnabled',false) end)
    log(id..' RECOVERY '..tostring(why)..'; reservations retained')
end
local function poll_one(id,job)
    local v=vehicle(id); authority(v)
    assert(valid(station(job.provider)) and valid(station(job.requester)), 'endpoint disappeared')
    local home=home_ok(job.home)
    assert(call(building(job.provider),'GetIsInLoadMode') and not call(building(job.requester),'GetIsInLoadMode'),'station modes changed')
    assert(Core.same_route(route(v),job.intent),'route edited or sanitized')
    local err=call(v,'GetAutopilotErrorStatus')
    assert(err==0,'autopilot error '..tostring(err))
    assert(os.time()-job.started<Config.JourneyTimeoutSeconds,'journey timeout')
    local owner=call(v,'GetOwnerVehicle')
    local at='Road'
    if call(building(job.provider),'GetDockedActor')==owner then at='Provider'
    elseif call(building(job.requester),'GetDockedActor')==owner then at='Requester'
    elseif call(home,'GetDockedActor')==owner then at='Home' end
    local next_state=Core.advance(job.state,at)
    assert(next_state~='Recovery','out-of-order docking; missed observations possible')
    if next_state=='HomeArrived' then
        call(v,'SetAutopilotEnabled',false)
        assert(parked(v,home),'home stop did not take effect')
        assert(call(call(owner,'GetStorageInventory'),'IsEmpty'),'returned with cargo: incomplete unload')
        registry:returned(id,true); log(id..' home stopped; Idle; transfers not measured')
    else
        assert(call(v,'IsAutopilotEnabled'),'autopilot stopped before home')
        if next_state~=job.state then log(id..' '..next_state..' (docking observation, not transfer confirmation)') end
        job.state=next_state
    end
end
local active = {}
local function poll()
    -- Pair/dispatch maintain this compact job list; never scan all objects/fleet here.
    local budget=math.min(Config.MaxObservedJobsPerPass,#active)
    for _=1,budget do
        if #active==0 then break end
        cursor=(cursor-1)%#active+1
        local id=active[cursor]; local job=registry.pairs[id]
        if job.state=='Idle' or job.state=='Recovery' then table.remove(active,cursor)
        else
            local ok,why=pcall(poll_one,id,job)
            if not ok then recover(id,why) end
            cursor=cursor+1
        end
    end
end
local commands={}
commands.ping=function() log('ping OK; Lua and game-thread callback are running') end
commands.scan=function()scan()end
commands.inspect=function(args)
    local v=vehicle(args[1]); local r=route(v)
    log(args[1]..' autopilot='..tostring(call(v,'IsAutopilotEnabled'))..' target='..tostring(call(v,'GetCurrentTargetWaypointIndex'))..' error='..tostring(call(v,'GetAutopilotErrorStatus')))
    for i,g in ipairs(r) do log('stop '..i..' '..Core.guid_key(g)) end
end
local function station_metadata(id)
    local identifier=station(id)
    local audit=audit_home(id)
    local label=Bridge.text(call(identifier,'GetStationName')):gsub('[\r\n]',' ')
    log('STATION '..id..' name='..label..' load='..tostring(audit.load)..' solid='..tostring(audit.solid)..
        ' empty='..tostring(audit.empty)..' cargo_connected='..audit.cargo_connections..
        ' fuel_connected='..audit.fuel_connections..' home='..tostring(audit.eligible)..' reason='..audit.reason)
    return audit
end
commands.station=function(args)station_metadata(args[1])end
commands.stations=function()
    local eligible=0
    local ids={};for id in pairs(stations)do ids[#ids+1]=id end
    table.sort(ids,function(a,b)return tonumber(a:sub(2))<tonumber(b:sub(2))end)
    for _,id in ipairs(ids)do if station_metadata(id).eligible then eligible=eligible+1 end end
    log('HOME_FIXTURES eligible='..eligible..' stations='..#ids)
end
commands.autotest_prepare=function()
    assert(test_run_ok and type(test_run)=='string','prepare requires generated automated test session')
    assert(valid(context),'scan first')
    local default=StaticFindObject('/Script/FactoryGame.Default__FGSaveSession')
    local session=call(default,'Get',context)
    assert(valid(session),'save session unavailable')
    call(session,'SetAutoSaveEnabled',false)
    assert(session.mAutosaveEnabled==false,'autosave-disable readback failed')
    test_prepared=true
    log('AUTOTEST_PREPARED autosave=false; manual route writes remain '..tostring(Config.AllowRouteWrites))
end
local function validate_existing_route(v,nodes)
    assert(#nodes>=2,'bridge proof needs a real multi-stop route')
    local owner=call(v,'GetOwnerVehicle')
    local preset=call(owner,'GetVehiclePathPreset');assert(valid(preset),'vehicle preset unavailable')
    local unique={}
    for i,node in ipairs(nodes)do
        unique[Core.guid_key(node)]=true
        local next_node=nodes[i%#nodes+1]
        local network=call(context,'FindNetworkByPathNodeGuid',node)
        assert(valid(network),'route network unavailable')
        local other=call(context,'FindNetworkByPathNodeGuid',next_node)
        assert(Bridge.same_object(network,other,valid),'route has disconnected nodes')
        local out={}
        assert(call(network,'FindVehiclePath',node,next_node,preset,out)==true,'existing route leg is unreachable')
    end
    local count=0;for _ in pairs(unique)do count=count+1 end
    assert(count>=2,'single-station routes are invalid')
end
commands.autotest_rest_stop_building=function()
    assert(test_run_ok and type(test_run)=='string' and test_prepared,'snapshot autosave preparation required')
    local manager=call(StaticFindObject('/Script/FactoryGame.Default__FGSchematicManager'),'Get',context)
    require('RestStopBuilding').ensure({call=call,valid=valid,find=StaticFindObject},manager)
    log('REST_STOP_BUILDING registered=true unlocked=true available=true; placement and hold NOT tested')
end
commands.hub_research=function()
    assert(valid(context),'scan in a loaded save first')
    local manager=call(StaticFindObject('/Script/FactoryGame.Default__FGSchematicManager'),'Get',context)
    require('HubResearch').repair({call=call,valid=valid,find=StaticFindObject,log=log},manager)
end
commands.hub_research_save=function()
    assert(valid(context),'scan in a loaded save first')
    local manager=call(StaticFindObject('/Script/FactoryGame.Default__FGSchematicManager'),'Get',context)
    assert(not valid(call(manager,'GetActiveSchematic')),'clear stale milestone before saving')
    local mode=call(StaticFindObject('/Script/Engine.Default__GameplayStatics'),'GetGameMode',context)
    assert(valid(mode) and call(mode,'HasAuthority'),'host game mode required')
    local name='City_FILN_HubRepaired_'..os.date('!%Y%m%d_%H%M%S')
    call(mode,'TriggerWorldSave',name)
    log('HUB_REPAIR_SAVE_REQUESTED name='..name)
end
commands.player_state=function()
    local count=0
    for _,pawn in ipairs(FindAllOf('FGCharacterPlayer')or {})do
        if valid(pawn) and call(pawn,'IsLocallyControlled')then
            count=count+1;local controller=call(StaticFindObject('/Script/Engine.Default__GameplayStatics'),'GetPlayerController',pawn,0)
            assert(valid(controller),'local controller unavailable')
            local controlled=call(controller,'GetControlledCharacter')
            assert(Bridge.same_object(controlled,pawn,valid),'controlled character mismatch')
            log('PLAYER_STATE local=true controller=true controlled_character=true driving='..tostring(call(pawn,'IsDrivingVehicle')))
        end
    end
    assert(count==1,'local player not ready')
end
commands.autotest_blueprints_preview=function()
    error('automated UI previews disabled by user preference; request manual visual/placement validation')
end
commands.blueprints_reload=function()
    package.loaded.BlueprintLibrary=nil;log('BLUEPRINT_HELPER_RELOADED')
end
commands.blueprints_organize=function()
    require('BlueprintLibrary').organize({call=call,valid=valid,find=StaticFindObject,log=log},assert(script_dir)..'/blueprint-categories.tsv')
end
commands.blueprints_verify=function()
    require('BlueprintLibrary').verify({call=call,valid=valid,find=StaticFindObject,log=log},assert(script_dir)..'/blueprint-categories.tsv')
end
commands.save_autosave=function()
    local session=call(StaticFindObject('/Script/FactoryGame.Default__FGSaveSession'),'Get',context)
    assert(valid(session),'native save session unavailable')
    call(session,'Autosave');log('FILN_NATIVE_AUTOSAVE_REQUESTED')
end
commands.save_city=function()
    assert(valid(context),'scan a loaded save first')
    if depot_runtime then
        for _,job in pairs(depot_runtime.jobs)do assert(job.state=='Idle' or job.state=='Detached' or job.state=='Recovery','wait for dispatcher trips to finish before checkpoint')end
    end
    local mode=call(StaticFindObject('/Script/Engine.Default__GameplayStatics'),'GetGameMode',context)
    assert(valid(mode) and call(mode,'HasAuthority'),'host game mode required')
    local name='City_FILN_Stations_'..os.date('!%Y%m%d_%H%M%S')
    call(mode,'TriggerWorldSave',name);log('FILN_CITY_SAVE_REQUESTED name='..name)
end
commands.programmable_unlock=function()
    assert(valid(context),'scan in a loaded save first')
    local manager=call(StaticFindObject('/Script/FactoryGame.Default__FGSchematicManager'),'Get',context)
    require('ProgrammableUnlock').ensure({call=call,valid=valid,find=StaticFindObject,load=LoadAsset,log=log},manager)
    local recipe=StaticFindObject('/TruckStationSettings/Recipe_ProgrammableTruckStation.Recipe_ProgrammableTruckStation_C')
    local found=false
    for _,gun in ipairs(FindAllOf('FGBuildGun')or {})do
        if valid(gun) then
            local out,custom={},{};call(gun,'GetAvailableRecipes',out,custom)
            for _,entry in ipairs(values(out))do
                if Bridge.same_object(entry,recipe,valid)then found=true;break end
            end
        end
    end
    assert(found,'programmable recipe rejected by actual Build Gun filtering')
    log('PROGRAMMABLE_BUILDGUN actual_listed=true output=1')
end
commands.autotest_filn_station=function()
    assert(test_run_ok and type(test_run)=='string' and test_prepared,'prepared snapshot required')
    local recipe=StaticFindObject('/KDataForge/Gen/FILNStations.Gen_FILNStations_Recipe_LogisticsStation')
    local expected=StaticFindObject('/TruckStationSettings/Build_ProgrammableTruckStation.Build_ProgrammableTruckStation_C')
    assert(valid(recipe) and valid(expected),'FILN station pack not available')
    local pawn
    for _,p in ipairs(FindAllOf('FGCharacterPlayer')or {})do if call(p,'IsLocallyControlled')then pawn=p;break end end
    assert(valid(pawn),'local player unavailable')
    local gun=call(pawn,'GetBuildGun');local out,custom={},{};call(gun,'GetAvailableRecipes',out,custom)
    local listed=false;for _,entry in ipairs(values(out))do if Bridge.same_object(entry,recipe,valid)then listed=true;break end end
    assert(listed,'FILN recipe missing from actual Build Gun')
    call(pawn,'HotKeyRecipe',recipe)
    local state=call(gun,'GetBuildGunStateFor',2);local holo=call(state,'GetHologram')
    assert(valid(holo) and Bridge.same_object(call(holo,'GetBuildClass'),expected,valid),'incorrect FILN station hologram')
    call(gun,'GotoMenuState')
    log('FILN_STATION_HOLOGRAM PASS actual_buildgun=true class=programmable')
end
commands.cargo_stations=function()
    assert(valid(context),'scan first')
    require('CargoAudit').report({call=call,valid=valid,find=StaticFindObject,log=log},stations)
end
commands.rest_stop_fixtures=function()
    assert(valid(context),'scan a loaded save first')
    local recipe=StaticFindObject('/KDataForge/Gen/FILNRestStop.Gen_FILNRestStop_Recipe_RestStop')
    assert(valid(recipe),'rest-stop recipe unavailable')
    local count=0
    for id,identifier in pairs(stations)do
        local actor=call(identifier,'GetStation')
        if Bridge.same_object(Bridge.value(actor.mBuiltWithRecipe),recipe,valid) then
            count=count+1
            local node=call(actor,'GetDockingPathNode')
            assert(valid(node),'rest-stop docking node unavailable')
            local guid=Core.guid_key(Core.guid(call(node,'GetPathNodeGUID')))
            local incoming=#values(call(node,'GetArrivingConnections'))
            local outgoing=#values(call(node,'GetLeavingConnections'))
            log('REST_STOP_FIXTURE '..id..' guid='..guid..' incoming='..incoming..' outgoing='..outgoing..' road_attached='..tostring(incoming>0 and outgoing>0))
        end
    end
    log('REST_STOP_FIXTURES count='..count..'; attachment is not a complete journey proof')
end
local depot_runtime,auto_dispatch,telemetry,depot_registration
local function depot()
    if not depot_runtime then
        depot_runtime=require('DepotRuntime').new({call=call,valid=valid,find=StaticFindObject,
            stations=function()return stations end,vehicles=function()return vehicles end,
            context=function()return context end,authority=authority,log=log,now=os.time,journey_timeout=Config.JourneyTimeoutSeconds,
            on_detach=function(home)if auto_dispatch and auto_dispatch.journal then auto_dispatch.journal:remove(home)end end})
        depot_runtime:watch()
    end
    return depot_runtime
end
local function automatic()
    if not auto_dispatch then
        auto_dispatch=require('AutoDispatch').new({call=call,valid=valid,find=StaticFindObject,log=log,authority=authority,
            now=os.time,refresh=function()scan(true)end,stations=function()return stations end,vehicles=function()return vehicles end,
            context=function()return context end,runtime=depot,
            journal=not(test_run_ok and type(test_run)=='string') and require('DispatchJournal').open(assert(script_dir)..'/dispatch-journal.tsv')or nil,
            available=function(guid)return not depot().reservations[guid]end,
            home_available=function(home)return not fleet_test or fleet_test.soak or not fleet_test.completed[home]end,
            enrolled=function(home)local job=depot().jobs[home];return job and job.state=='Idle' end,budget=1})
        auto_dispatch.enabled=Config.AllowAutoDispatch==true and not(test_run_ok and type(test_run)=='string')
    end
    return auto_dispatch
end
local function dashboard_telemetry()
    if not telemetry then
        telemetry=require('Telemetry').new({call=call,valid=valid,find=StaticFindObject,log=log,now=os.time,
            context=function()return context end,refresh=function()scan(true)end,
            stations=function()return stations end,vehicles=function()return vehicles end,
            runtime=depot,scheduler=automatic,lifecycle_path=assert(script_dir)..'/dashboard-mod.txt',
            demand_path=script_dir..'/dashboard-demand.txt',
            output_path=script_dir..'/telemetry.json',progress_path=script_dir..'/telemetry-progress.json',ready=loaded_world})
    end
    return telemetry
end
commands.dashboard=function()
    dashboard_telemetry():request()
    require('Dashboard').open({call=call,valid=valid,find=StaticFindObject,log=log,now=os.time},
        assert(script_dir)..'/dashboard-server.txt')
end
commands.dashboard_status=function()
    local t=dashboard_telemetry()
    log('DASHBOARD_STATUS sequence='..t.sequence..' collecting='..tostring(t.cycle~=nil))
end
commands.auto_start=function()
    assert(Config.AllowAutoDispatch==true,'automatic dispatch not enabled in Config.lua')
    if test_run_ok and type(test_run)=='string' then assert(test_prepared and Config.AutoTestAllowAutoDispatch==true,'snapshot automatic dispatch gate disabled')end
    automatic().enabled=true;log('AUTO_DISPATCH_ENABLED')
end
commands.auto_stop=function()
    automatic().enabled=false;log('AUTO_DISPATCH_DISABLED active trips retain return/hold observer')
end
commands.auto_cache_probe=function(args)
    assert(#args==2,'home and requester GUIDs required');automatic():probe(args[1],args[2])
end
local orphan_test
commands.autotest_orphan_adopt=function(args)
    assert(test_run_ok and type(test_run)=='string' and test_prepared,'prepared disposable snapshot required')
    assert(Config.AutoTestAllowRouteBridge==true,'route-bridge gate required')
    assert(#args==1 and #args[1]==32 and args[1]:match('^[0-9a-f]+$'),'pinned home GUID required')
    orphan_test=require('OrphanTest').new({call=call,valid=valid,find=StaticFindObject,authority=authority,log=log,now=os.time,
        stations=function()return stations end,vehicles=function()return vehicles end,runtime=depot,
        journal=require('DispatchJournal').open(assert(script_dir)..'/orphan-test-'..test_run..'.tsv')})
    orphan_test:start(args[1])
end
commands.autotest_orphan_state=function()
    assert(test_run_ok and type(test_run)=='string' and test_prepared and orphan_test,'prepared orphan test required')
    orphan_test:check()
end
commands.auto_status=function()
    local scheduler=automatic();log('AUTO_DISPATCH_STATUS enabled='..tostring(scheduler.enabled))
    log('AUTO_PERF samples='..scheduler.samples..' searches='..scheduler.searches..' registry_refreshes='..scheduler.refreshes..' graph_builds='..scheduler.paths.builds..' graph_hits='..scheduler.paths.hits)
    for home,job in pairs(depot().jobs)do log('AUTO_JOB home='..home..' state='..job.state..' loaded='..job.loaded..' unloaded='..job.unloaded..' error='..tostring(job.error)..' recovery='..tostring(job.recovery and job.recovery.state)..' attempts='..tostring(job.recovery and job.recovery.attempts)..' next_retry='..tostring(job.recovery and job.recovery.next_retry))end
    for home,reason in pairs(scheduler.reasons)do log('AUTO_DISPATCH_REASON home='..home..' reason='..reason)end
end
commands.requests=function()
    local scheduler=automatic();local runtime=depot()
    local rows=require('RequestStatus').rows(scheduler.catalog,runtime.jobs,runtime.reservations,function(request)
        local result
        for home,v in pairs(scheduler.vehicles_by_home or {})do
            local station=scheduler.stations_by_home[home]
            if station then
                local b=call(station,'GetStation')
                if call(b,'HasPower')and not call(b,'IsProductionPaused')and call(v,'IsAutopilotEnabled')then
                    local capacity=require('CargoAudit').inventory({call=call,valid=valid},call(call(v,'GetOwnerVehicle'),'GetStorageInventory'),request.resource).capacity
                    if not result or capacity<result then result=capacity end
                end
            end
        end
        return result
    end)
    local lines={'FILN requests ('..#rows..') - last completed station scan'}
    if not scheduler.catalog then lines[#lines+1]='Stock scan pending; enable dispatcher and try again shortly.'end
    for i,row in ipairs(rows)do
        local item=row.item and (row.item:match('Desc_([%w_]+)')or row.item)or 'unknown'
        log('REQUEST guid='..row.guid..' station='..row.label..' item='..item..' available='..tostring(row.stock)..' truckload='..tostring(row.truckload)..' reason='..row.reason..' supplier='..tostring(row.supplier))
        lines[#lines+1]=row.label..': '..item..' '..tostring(row.stock)..'/'..tostring(row.truckload or '?')..' - '..row.reason
    end
    local text=table.concat(lines,'\n')
    local library=StaticFindObject('/Script/Engine.Default__KismetTextLibrary')
    assert(valid(library),'text conversion unavailable; request details are in UE4SS.log')
    local controller
    for _,pawn in ipairs(FindAllOf('FGCharacterPlayer')or {})do
        if call(pawn,'IsLocallyControlled')then controller=call(pawn,'GetController');break end
    end
    assert(valid(controller),'local player controller unavailable')
    local pages={}
    for start=2,math.max(2,#lines),4 do
        local page={lines[1]}
        for index=start,math.min(start+3,#lines)do page[#page+1]=lines[index]end
        pages[#pages+1]=table.concat(page,'\n')
    end
    for _,page in ipairs(pages)do
        call(controller,'Client_SendChatMessage',{MessageText=call(library,'Conv_StringToText',page),MessageType=1,
            MessageSender=call(library,'Conv_StringToText','FILN'),MessageSenderColor={R=0.2,G=1,B=0.7,A=1},ServerTimeStamp=0,bIsLocalPlayerMessage=false})
    end
    local manager=call(StaticFindObject('/Script/FactoryGame.Default__FGChatManager'),'Get',context)
    local messages={};call(manager,'GetReceivedChatMessages',messages)
    for _,page in ipairs(pages)do
        local found=false
        for _,message in ipairs(values(messages))do if Bridge.text(message.MessageText)==page then found=true;break end end
        assert(found,'request message was not added to native chat history')
    end
    log('REQUEST_VIEW shown=true native_chat_verified=true rows='..#rows)

end
commands.recover_vehicle=function(args)
    assert(#args==1,'vehicle ID required; this command explicitly opts a repaired empty truck back in')
    local v=vehicle(args[1]);authority(v)
    local owner=call(v,'GetOwnerVehicle');assert(not call(owner,'HasActiveDriver'),'exit vehicle normally before recovery')
    assert(call(call(owner,'GetStorageInventory'),'IsEmpty'),'recovery enrollment requires empty cargo; preserve its known delivery instead')
    local r=route(v);assert(#r==1,'recovery enrollment requires assigned home-only route')
    local home=Core.guid_key(r[1]);local identifier
    for _,s in pairs(stations)do if Core.guid_key(Core.guid(call(s,'GetPathNodeGUID')))==home then identifier=s;break end end
    assert(identifier,'assigned home missing')
    local audit=Bridge.home_audit(identifier,call,valid,StaticFindObject);assert(audit.eligible,audit.reason)
    local runtime=depot();local previous=runtime.jobs[home]
    if previous then
        assert(previous.state=='Recovery' or previous.state=='Detached' or previous.state=='Idle','truck has active cargo trip')
        runtime:detach(previous,'explicit-recovery-enrollment')
        runtime.jobs[home]=nil
    end
    call(v,'SetAutopilotEnabled',true);assert(call(v,'IsAutopilotEnabled'),'native enrollment rejected')
    runtime:ensure_home(home,identifier,v)
    log('DEPOT_RECOVERY_ENROLLED vehicle='..args[1]..' home='..home..' explicit_optin=true')
end
local function depot_test_ready()
    assert(test_run_ok and type(test_run)=='string' and test_prepared,'prepared disposable snapshot required')
end
local function fuel_count(inventory)
    assert(valid(inventory),'fuel inventory unavailable')
    local stacks={};call(inventory,'GetInventoryStacks',stacks,false)
    local n=0;for _,stack in ipairs(values(stacks))do n=n+stack.NumItems end
    return n
end
commands.depot_fuel=function()
    scan(true)
    local recipe=StaticFindObject('/KDataForge/Gen/FILNRestStop.Gen_FILNRestStop_Recipe_RestStop')
    local homes={};local enabled=0;local disabled=0
    for id,s in pairs(stations)do
        local b=call(s,'GetStation')
        if valid(b) and Bridge.same_object(Bridge.value(b.mBuiltWithRecipe),recipe,valid)then
            local home=Core.guid_key(Core.guid(call(s,'GetPathNodeGUID')));homes[home]=true
            local active=call(b,'HasPower') and not call(b,'IsProductionPaused')
            if active then enabled=enabled+1 else disabled=disabled+1 end
            log('DEPOT_FUEL id='..id..' home='..home..' active='..tostring(active)..' fuel_items='..fuel_count(call(b,'GetFuelInventory'))..' cargo_empty='..tostring(call(call(b,'GetInventory'),'IsEmpty')))
        end
    end
    for id,v in pairs(vehicles)do
        local r=route(v);local home=#r==1 and Core.guid_key(r[1])or nil
        if home and homes[home]then log('DEPOT_TRUCK_FUEL id='..id..' home='..home..' fuel_items='..fuel_count(call(call(v,'GetOwnerVehicle'),'GetFuelInventory'))..' autopilot='..tostring(call(v,'IsAutopilotEnabled')))end
    end
    log('DEPOT_FUEL_SUMMARY active='..enabled..' disabled='..disabled)
end
commands.autotest_fleet_fuel_trace=function()
    depot_test_ready();assert(fleet_test,'fleet audit required')
    for home,v in pairs(fleet_test.vehicles)do
        log('FUEL_TRUCK home='..home..' items='..fuel_count(call(call(v,'GetOwnerVehicle'),'GetFuelInventory')))
    end
    for _,s in pairs(stations)do
        local home=Core.guid_key(Core.guid(call(s,'GetPathNodeGUID')))
        if fleet_test.homes[home]then
            local b=call(s,'GetStation')
            log('FUEL_DEPOT home='..home..' items='..fuel_count(call(b,'GetFuelInventory'))..' cargo_empty='..tostring(call(call(b,'GetInventory'),'IsEmpty')))
        end
    end
    for _,splitter in ipairs(FindAllOf('Build_ConveyorAttachmentSplitterSmart_C')or {})do
        if valid(splitter)then
            local ok,why=pcall(function()
                for _,rule in ipairs(values(call(splitter,'GetSortRules')))do
                    local item=Bridge.value(rule.ItemClass)
                    log('FUEL_SPLITTER actor='..splitter:GetFullName()..' output='..tostring(rule.OutputIndex)..' item='..(valid(item)and item:GetFullName()or 'none'))
                end
            end)
            if not ok then log('FUEL_SPLITTER_TRACE_UNAVAILABLE '..tostring(why))end
        end
    end
    log('FUEL_TRACE_PASS read_only=true')
end
local recovery_test
commands.autotest_recovery_fleet=function()
    depot_test_ready()
    assert(not recovery_test,'recovery test already initialized')
    scan(true);recovery_test={homes={},preserved={}}
    local recipe=StaticFindObject('/KDataForge/Gen/FILNRestStop.Gen_FILNRestStop_Recipe_RestStop')
    for id,v in pairs(vehicles)do
        local r=route(v);local identifier
        if #r==1 then
            for _,station in pairs(stations)do if Core.guid_key(Core.guid(call(station,'GetPathNodeGUID')))==Core.guid_key(r[1])then identifier=station;break end end
        end
        local b=identifier and call(identifier,'GetStation')
        if valid(b) and Bridge.same_object(Bridge.value(b.mBuiltWithRecipe),recipe,valid)and call(b,'HasPower')and not call(b,'IsProductionPaused')then
            recovery_test.homes[#recovery_test.homes+1]={id=id,vehicle=v,identifier=identifier,building=b,home=Core.guid_key(r[1])}
        else recovery_test.preserved[#recovery_test.preserved+1]={vehicle=v,route=r,enabled=call(v,'IsAutopilotEnabled')}end
    end
    assert(#recovery_test.homes==4,'expected four powered home-only trucks')
    for _,entry in ipairs(recovery_test.homes)do commands.recover_vehicle({entry.id})end
    log('RECOVERY_FLEET_STARTED count=4 cargo_dispatch_paused=true')
end
commands.autotest_recovery_status=function()
    depot_test_ready();assert(recovery_test,'initialize recovery fixture first')
    local ready=0
    for _,p in ipairs(recovery_test.preserved)do assert(Core.same_route(route(p.vehicle),p.route)and call(p.vehicle,'IsAutopilotEnabled')==p.enabled,'ordinary/disabled truck changed')end
    for _,entry in ipairs(recovery_test.homes)do
        local v=entry.vehicle;local owner=call(v,'GetOwnerVehicle');local p,q={X=0,Y=0,Z=0},{}
        call(owner,'GetVehicleLocationAndRotation',p,q)
        local at=call(call(entry.building,'GetDockingPathNode'),'K2_GetActorLocation')
        local cm=math.sqrt((p.X-at.X)^2+(p.Y-at.Y)^2+(p.Z-at.Z)^2)
        local r=route(v);local job=depot().jobs[entry.home]
        local full=depot().fuel_service:ready(entry.home,entry.identifier,v)
        local is_ready=cm<1000 and #r==1 and Core.guid_key(r[1])==entry.home and call(v,'IsAutopilotEnabled')and full and(not job or job.state=='Idle')
        if is_ready then ready=ready+1 end
        log('RECOVERY_NATIVE home='..entry.home..' id='..entry.id..' distance_cm='..math.floor(cm)..' truck_fuel='..fuel_count(call(owner,'GetFuelInventory'))..' full='..tostring(full)..' state='..tostring(job and job.state)..' recovery='..tostring(job and job.recovery and job.recovery.state)..' ready='..tostring(is_ready))
    end
    log('RECOVERY_FLEET_STATUS ready='..ready..' ordinary_preserved=true')
end
commands.autotest_fleet_audit=function(args)
    depot_test_ready();assert(Config.AutoTestAllowDepotJourney==true,'fleet test writes disabled')
    local recipe=StaticFindObject('/KDataForge/Gen/FILNRestStop.Gen_FILNRestStop_Recipe_RestStop')
    fleet_test={homes={},disabled={},completed={},preserved={},vehicles={},fuel={},soak=args[2]=='soak'}
    local depot_guids={};local enabled=0;local disabled=0
    for _,identifier in pairs(stations)do
        local b=call(identifier,'GetStation')
        if valid(b) and Bridge.same_object(Bridge.value(b.mBuiltWithRecipe),recipe,valid)then
            local home=Core.guid_key(Core.guid(call(identifier,'GetPathNodeGUID')))
            local powered=call(b,'HasPower');local paused=call(b,'IsProductionPaused')
            depot_guids[home]=true
            if powered and not paused then
                local audit=Bridge.home_audit(identifier,call,valid,StaticFindObject)
                assert(audit.eligible,'active depot '..home..': '..audit.reason)
                fleet_test.homes[home]=true;enabled=enabled+1
            else fleet_test.disabled[home]=true;disabled=disabled+1 end
            log('FLEET_HOME home='..home..' powered='..tostring(powered)..' paused='..tostring(paused))
        end
    end
    assert(enabled==tonumber(args[1]),'powered depot count differs: '..enabled)
    local claims={}
    for id,v in pairs(vehicles)do
        local r=route(v);local home=#r==1 and Core.guid_key(r[1])or nil
        if home and fleet_test.homes[home]then
            assert(not claims[home],'multiple trucks claim active depot');claims[home]=true;fleet_test.vehicles[home]=v
            assert(call(v,'IsAutopilotEnabled'),'enable autopilot for active depot '..home)
            local owner=call(v,'GetOwnerVehicle');assert(valid(owner) and not call(owner,'HasActiveDriver'),'fleet baseline requires naturally unoccupied vehicles')
            assert(call(call(owner,'GetStorageInventory'),'IsEmpty'),'fleet baseline requires empty trucks')
            log('FLEET_VEHICLE home='..home..' id='..id)
            local station_fuel
            for _,station in pairs(stations)do if Core.guid_key(Core.guid(call(station,'GetPathNodeGUID')))==home then station_fuel=call(call(station,'GetStation'),'GetFuelInventory');break end end
            log('FLEET_FUEL home='..home..' has_fuel='..tostring(call(owner,'HasFuel'))..' vehicle_empty='..tostring(call(call(owner,'GetFuelInventory'),'IsEmpty'))..' station_empty='..tostring(call(station_fuel,'IsEmpty')))
        else
            fleet_test.preserved[#fleet_test.preserved+1]={vehicle=v,route=r,enabled=call(v,'IsAutopilotEnabled')}
            log('FLEET_PRESERVED id='..id..' autopilot='..tostring(call(v,'IsAutopilotEnabled')))
        end
    end
    for home in pairs(fleet_test.homes)do assert(claims[home],'powered depot has no depot-only truck: '..home)end
    log('FLEET_AUDIT_PASS enabled='..enabled..' disabled='..disabled..' preserved='..#fleet_test.preserved)
end
commands.autotest_fleet_stationary_refuel_ready=function()
    depot_test_ready();assert(fleet_test,'fleet audit required')
    assert(not automatic().enabled,'cargo dispatcher must remain paused for stationary fuel proof')
    local count=0
    for home,v in pairs(fleet_test.vehicles)do
        assert(not depot().jobs[home],'cargo job started during stationary fuel proof')
        local receipt=depot().fuel_service.receipts[home]
        assert(receipt and receipt.vehicle==v:GetFullName() and receipt.after>receipt.before,'stationary native refueling not observed: '..home)
        local r=route(v);assert(#r==1 and Core.guid_key(r[1])==home and call(v,'IsAutopilotEnabled'),'stationary service changed route/autopilot')
        count=count+1
    end
    for _,p in ipairs(fleet_test.preserved)do assert(Core.same_route(route(p.vehicle),p.route) and call(p.vehicle,'IsAutopilotEnabled')==p.enabled,'preserved vehicle changed')end
    log('STATIONARY_REFUEL_PASS homes='..count..' dispatch_paused=true routes_unchanged=true ordinary_preserved=true')
end
commands.autotest_fleet_refuel_fixture=function()
    depot_test_ready();assert(Config.AutoTestAllowRefuelFixture==true and fleet_test,'refueling fixture disabled')
    local plan={}
    for _,s in pairs(stations)do
        local home=Core.guid_key(Core.guid(call(s,'GetPathNodeGUID')))
        if fleet_test.homes[home]then plan[#plan+1]={home=home,building=call(s,'GetStation'),owner=call(fleet_test.vehicles[home],'GetOwnerVehicle')}end
    end
    assert(#plan==4,'expected four explicit depot/vehicle pairs')
    require('RefuelFixture').prepare({snapshot=true,call=call,valid=valid,log=log},plan,
        StaticFindObject('/Game/FactoryGame/Resource/RawResources/Coal/Desc_Coal.Desc_Coal_C'))
    fleet_test.require_refueling=true
    log('REFUEL_FIXTURE_PASS pairs=4 fuel_conserved=true cargo_untouched=true')
end
commands.autotest_fleet_fix_fuel_rules=function(args)
    depot_test_ready();assert(Config.AutoTestAllowFuelRules==true and fleet_test,'fuel-rule fixture disabled')
    local plans={};local seen={};local splitters=FindAllOf('Build_ConveyorAttachmentSplitterSmart_C')or {}
    assert(#args==8,'four explicit home/splitter pairs required')
    for i=1,#args,2 do
        local home,suffix=args[i],args[i+1]
        assert(fleet_test.homes[home] and not seen[home],'fuel plan must match distinct active homes');seen[home]=true
        assert(suffix:match('^Build_ConveyorAttachmentSplitterSmart_C_%d+$'),'invalid splitter suffix')
        local found
        for _,s in ipairs(splitters)do if valid(s) and s:GetFullName():sub(-#suffix)==suffix then assert(not found,'ambiguous splitter');found=s end end
        plans[#plans+1]={home=home,actor=found,output=2,bypass=0}
    end
    local changed=require('FuelSupplyRules').apply({call=call,valid=valid,authority=authority,log=log},plans)
    log('FUEL_BRANCH_FIX_PASS changed='..changed..' snapshot_only=true')
end
commands.autotest_fleet_depot_fuel_ready=function(args)
    depot_test_ready();assert(fleet_test,'fleet audit required')
    local count=0
    for _,s in pairs(stations)do
        local home=Core.guid_key(Core.guid(call(s,'GetPathNodeGUID')))
        if fleet_test.homes[home]then
            local b=call(s,'GetStation');local n=fuel_count(call(b,'GetFuelInventory'))
            log('FUEL_DEPOT home='..home..' items='..n..' cargo_empty='..tostring(call(call(b,'GetInventory'),'IsEmpty')))
            assert(n>=(tonumber(args[1])or 1),'fuel belt has not supplied refueling inventory: '..home)
            assert(call(call(b,'GetInventory'),'IsEmpty'),'rest stop cargo inventory must remain empty');count=count+1
        end
    end
    assert(count==4,'expected four active fuel inventories');log('DEPOT_FUEL_READY count=4 cargo_empty=true')
end
commands.autotest_fleet_seed_fuel=function()
    depot_test_ready();assert(Config.AutoTestAllowStarterFuel==true and fleet_test,'starter fuel fixture disabled')
    require('TestFuel').seed({snapshot=true,call=call,valid=valid,authority=authority,log=log},fleet_test.vehicles,
        StaticFindObject('/Game/FactoryGame/Resource/RawResources/Coal/Desc_Coal.Desc_Coal_C'))
end
commands.autotest_fleet_fuel_ready=function()
    depot_test_ready();assert(fleet_test,'fleet audit required')
    for home,v in pairs(fleet_test.vehicles)do
        if not call(call(v,'GetOwnerVehicle'),'HasFuel')then
            local available=false
            for _,station in pairs(stations)do
                if Core.guid_key(Core.guid(call(station,'GetPathNodeGUID')))==home then
                    local fuel=call(call(station,'GetStation'),'GetFuelInventory')
                    available=valid(fuel) and not call(fuel,'IsEmpty');break
                end
            end
            assert(available,'vehicle and depot have no fuel: '..home)
        end
    end
    log('FLEET_FUEL_READY all=true')
end
commands.autotest_fleet_status=function()
    depot_test_ready();assert(fleet_test,'fleet audit required')
    local runtime=depot();local reserved={};local completed=0
    for _,p in ipairs(fleet_test.preserved)do
        assert(call(p.vehicle,'IsAutopilotEnabled')==p.enabled and Core.same_route(route(p.vehicle),p.route),'ordinary/disabled vehicle changed route or autopilot')
    end
    for home in pairs(fleet_test.disabled)do assert(not runtime.jobs[home],'disabled depot entered dispatcher')end
    for home in pairs(fleet_test.homes)do
        local job=runtime.jobs[home]
        local v=fleet_test.vehicles[home]
        local truck_fuel=fuel_count(call(call(v,'GetOwnerVehicle'),'GetFuelInventory'))
        local depot_fuel=0
        for _,s in pairs(stations)do if Core.guid_key(Core.guid(call(s,'GetPathNodeGUID')))==home then depot_fuel=fuel_count(call(call(s,'GetStation'),'GetFuelInventory'));break end end
        local fuel_state=truck_fuel..':'..depot_fuel
        if fleet_test.fuel[home]~=fuel_state then fleet_test.fuel[home]=fuel_state;log('FLEET_FUEL_CHANGED home='..home..' truck_items='..truck_fuel..' depot_items='..depot_fuel)end
        if job then
            log('FLEET_JOB home='..home..' state='..job.state..' loaded='..job.loaded..' unloaded='..job.unloaded)
            assert(job.state~='Recovery' and job.state~='Suspended' and job.state~='Detached',job.error or 'fleet truck detached')
            if job.state=='Trip' or job.state=='Servicing' or job.state=='Holding' then
                for i=1,2 do
                    local k=Core.guid_key(job.trip[i]);assert(not reserved[k],'fleet endpoint collision');reserved[k]=home
                    assert(runtime.reservations[k]==home,'fleet endpoint ownership lost')
                end
            elseif job.state=='Idle' then
                assert(job.loaded>0 and job.loaded==job.unloaded,'fleet cargo completion missing')
                assert(#route(job.vehicle)==1 and Core.guid_key(route(job.vehicle)[1])==home and call(job.vehicle,'IsAutopilotEnabled'),'fleet idle route/control mismatch')
                fleet_test.completed[home]=true
            end
        else log('FLEET_JOB home='..home..' state=Waiting loaded=0 unloaded=0')end
        if fleet_test.completed[home]then
            assert(not fleet_test.require_refueling or(job and job.native_fuel_received),'native refueling transfer not observed at home '..home)
            completed=completed+1
        end
    end
    log('FLEET_STATUS_PASS completed='..completed..' ordinary_preserved=true disabled_excluded=true reservations_unique=true')
    commands.auto_status()
end
commands.autotest_isolate_traffic=function(args)
    depot_test_ready();assert(Config.AutoTestAllowDepotJourney==true,'traffic isolation test writes disabled')
    require('TrafficIsolation').pause({snapshot=true,call=call,valid=valid,authority=authority,log=log,
        vehicles=function()return vehicles end},args[1])
end
commands.autotest_station_retrofit=function()
    depot_test_ready();assert(Config.AutoTestAllowDepotJourney==true,'retrofit test writes disabled')
    local item=StaticFindObject('/Game/FactoryGame/Resource/Parts/IronIngot/Desc_IronIngot.Desc_IronIngot_C')
    local other=StaticFindObject('/Game/FactoryGame/Resource/Parts/IronRod/Desc_IronRod.Desc_IronRod_C')
    assert(valid(item) and valid(other),'filter test descriptors unavailable')
    local recipe=StaticFindObject('/KDataForge/Gen/FILNRestStop.Gen_FILNRestStop_Recipe_RestStop')
    local chosen
    for _,identifier in pairs(stations)do
        local b=call(identifier,'GetStation')
        if valid(b) and not Bridge.same_object(Bridge.value(b.mBuiltWithRecipe),recipe,valid) and
            not call(b,'GetIsInLoadMode') and call(call(b,'GetInventory'),'IsEmpty') and not valid(call(b,'GetDockedActor')) then
            chosen=identifier;break
        end
    end
    assert(chosen,'no empty unoccupied vanilla requester for isolated retrofit test')
    local adapter={call=call,valid=valid,authority=authority,busy=function(guid)return depot_runtime and depot_runtime.reservations[guid]~=nil end}
    local retrofit=require('StationRetrofit')
    local receipt=retrofit.apply(adapter,chosen,item)
    local ok,why=pcall(function()
        for index=0,#receipt.filters-1 do
            assert(call(receipt.inventory,'IsItemAllowed',item,index),'configured item rejected')
            assert(not call(receipt.inventory,'IsItemAllowed',other,index),'wrong item allowed')
        end
        local library=StaticFindObject('/Script/FactoryGame.Default__FGInventoryLibrary')
        local allowed=call(library,'MakeInventoryStack',1,call(library,'MakeInventoryItem',item))
        local blocked=call(library,'MakeInventoryStack',1,call(library,'MakeInventoryItem',other))
        assert(call(receipt.inventory,'HasEnoughSpaceForStack',allowed),'matching stack cannot be inserted')
        assert(not call(receipt.inventory,'HasEnoughSpaceForStack',blocked),'wrong stack can be inserted')
        log('STATION_RETROFIT_APPLIED guid='..receipt.guid..' actor_same=true road_same=true name_same=true mode_same=true filter_verified=true')
    end)
    local restored,restore_error=pcall(retrofit.restore,adapter,receipt)
    assert(restored,'retrofit rollback failed: '..tostring(restore_error))
    assert(ok,why)
    log('STATION_RETROFIT_PASS filters=true insertion_check=true restored=true; actual docking and save/reload NOT tested')
end
commands.autotest_cargo_select=function(args)
    depot_test_ready()
    require('CargoDispatch').select({call=call,valid=valid,find=StaticFindObject,log=log,authority=authority,
        stations=function()return stations end,vehicles=function()return vehicles end,context=function()return context end},args[1])
end
commands.autotest_depot_probe=function(args)
    depot_test_ready();depot():probe(args[1],args[2],args[3],args[4] and StaticFindObject(args[4]) or nil)
end
commands.autotest_depot_prepare=function(args)
    depot_test_ready();assert(Config.AutoTestAllowDepotJourney==true,'depot journey writes disabled')
    depot():prepare(args[1],args[2],args[3],args[4] and StaticFindObject(args[4]) or nil)
end
commands.autotest_depot_begin=function(args)
    depot_test_ready();assert(Config.AutoTestAllowDepotJourney==true,'depot journey writes disabled')
    depot():begin(args[1],args[2],args[3],args[4] and StaticFindObject(args[4]) or nil)
end
commands.autotest_driver_optout=function(args)
    depot_test_ready();assert(Config.AutoTestAllowDepotJourney==true,'snapshot optout test disabled')
    local job=depot():status(args[1]);job.optout_test_route=route(job.vehicle)
    call(job.vehicle,'SetAutopilotEnabled',false)
    log('DRIVER_OPTOUT_REQUESTED native_autopilot=false')
end
commands.autotest_driver_optout_check=function(args)
    depot_test_ready();local runtime=depot();local job=runtime:status(args[1])
    assert(job.state=='Detached','vehicle still dispatcher-owned')
    assert(not call(job.vehicle,'IsAutopilotEnabled') and Core.same_route(route(job.vehicle),job.optout_test_route),'optout changed controls/route')
    assert(not runtime.owners[job.vehicle_key],'owner lease retained')
    for _,identifier in ipairs({job.identifiers[2],job.identifiers[3]})do assert(not runtime.reservations[Core.guid_key(Core.guid(call(identifier,'GetPathNodeGUID')))],'endpoint lease retained')end
    log('DRIVER_OPTOUT_PASS detached=true route_unchanged=true reservations_released=true')
end
commands.autotest_depot_status=function(args)
    depot_test_ready()
    local job=depot():status(args[1])
    log('DEPOT_STATE state='..job.state..' visited='..job.visited..' departed='..tostring(job.departed)..
        ' error='..tostring(job.error)..' hook_events='..#job.events..' loaded='..job.loaded..' unloaded='..job.unloaded)
    assert(job.state~='Recovery' and job.state~='Suspended',job.error)
end
commands.autotest_depot_guard=function(args)
    depot_test_ready();assert(Config.AutoTestAllowDepotJourney==true,'depot journey writes disabled')
    depot():guard(args[1],args[2],args[3])
end
commands.autotest_depot_break_road=function(args)
    depot_test_ready();assert(Config.AutoTestAllowDepotJourney==true,'depot journey writes disabled')
    depot():break_test_road(args[1])
end
commands.autotest_depot_blocked=function(args)
    depot_test_ready();assert(Config.AutoTestAllowDepotJourney==true,'depot journey writes disabled')
    depot():blocked_check(args[1],args[2],args[3])
end
commands.autotest_depot_cancel=function(args)
    depot_test_ready();depot():cancel(args[1])
end
commands.autotest_rest_stop_hologram=function()
    assert(test_run_ok and type(test_run)=='string' and test_prepared,'prepared disposable snapshot required')
    local recipe=StaticFindObject('/KDataForge/Gen/FILNRestStop.Gen_FILNRestStop_Recipe_RestStop')
    local expected=StaticFindObject('/Game/FactoryGame/Buildable/Factory/TruckStation/Build_TruckStation.Build_TruckStation_C')
    assert(valid(recipe) and valid(expected),'rest-stop assets unavailable')
    local gun,pawn
    for _,candidate in ipairs(FindAllOf('FGCharacterPlayer') or {})do
        if call(candidate,'IsLocallyControlled') then
            pawn=candidate;gun=call(pawn,'GetBuildGun');break
        end
    end
    assert(valid(gun),'local player build gun unavailable')
    log('REST_STOP_HOLOGRAM attempting native HotKeyRecipe equip/build path')
    call(pawn,'HotKeyRecipe',recipe)
    local state=call(gun,'GetBuildGunStateFor',2)
    local hologram=call(state,'GetHologram')
    assert(valid(hologram),'rest-stop hologram was not created')
    assert(Bridge.same_object(call(hologram,'GetBuildClass'),expected,valid),'unexpected hologram actor class')
    assert(Bridge.same_object(call(state,'GetDescriptor'),StaticFindObject('/KDataForge/Gen/FILNRestStop.Gen_FILNRestStop_Desc_RestStop'),valid),'wrong building descriptor')
    call(gun,'GotoMenuState')
    log('REST_STOP_HOLOGRAM PASS created=true build_class=true descriptor=true; actual construction and roads NOT tested')
end
commands.autotest_route_bridge=function(args)
    assert(test_run_ok and type(test_run)=='string' and test_prepared,'snapshot autosave preparation required')
    assert(Config.AutoTestAllowRouteBridge==true,'automatic bridge mutation is not enabled')
    local candidates={}
    if args[1] then candidates[1]=args[1]
    else for id in pairs(vehicles)do candidates[#candidates+1]=id end
        table.sort(candidates,function(a,b)return tonumber(a:sub(2))<tonumber(b:sub(2))end)
    end
    local chosen,original,v
    for _,id in ipairs(candidates)do
        local candidate=vehicle(id)
        local ok,nodes=pcall(function()
            authority(candidate)
            assert(call(candidate,'GetAutopilotErrorStatus')==0,'autopilot error')
            assert(not call(candidate,'IsCurrentlyDocking'),'vehicle is docking')
            local existing=route(candidate);validate_existing_route(candidate,existing);return existing
        end)
        if ok then chosen,original,v=id,nodes,candidate;break end
        log('BRIDGE_CANDIDATE '..id..' skipped: '..tostring(nodes))
    end
    assert(chosen,'no error-free, non-docking vehicle with reachable multi-stop route')
    local autopilot=call(v,'IsAutopilotEnabled')
    local original_target=call(v,'GetCurrentTargetWaypointIndex')
    local rotated={};for i=1,#original do rotated[i]=original[i%#original+1]end
    assert(not Core.same_route(original,rotated),'rotated route would not change route intent')
    local changed_target
    local write_ok,write_error=pcall(function()
        call(v,'SetAutopilotEnabled',false)
        assert(not call(v,'IsAutopilotEnabled'),'failed to stop selected vehicle')
        call(v,'SetVehicleRoute',rotated)
        assert(Core.same_route(route(v),rotated),'changed route readback failed')
        changed_target=call(v,'GetCurrentTargetWaypointIndex')
        log('BRIDGE_WRITE '..chosen..' changed=true stops='..#original..' target='..tostring(changed_target))
    end)
    -- Never leave an exception path without attempting both restoration steps.
    local restore_ok,restore_error=pcall(function()
        call(v,'SetAutopilotEnabled',false)
        call(v,'SetVehicleRoute',original)
        assert(Core.same_route(route(v),original),'original route restore readback failed')
        call(v,'SetAutopilotEnabled',autopilot)
        assert(call(v,'IsAutopilotEnabled')==autopilot,'original autopilot restore readback failed')
    end)
    assert(restore_ok,'BRIDGE RESTORE FAILED: '..tostring(restore_error))
    log('BRIDGE_RESTORE '..chosen..' route=true autopilot='..tostring(autopilot)..' original_target='..tostring(original_target)..
        ' restored_target='..tostring(call(v,'GetCurrentTargetWaypointIndex')))
    assert(write_ok,'bridge write failed (original route restored): '..tostring(write_error))
    log('BRIDGE_PROOF '..chosen..' PASS changed_route=true restored_route=true restored_autopilot=true; driving NOT tested')
end
commands.pair=function(args)
    local v,h=vehicle(args[1]),home_ok(args[2]); authority(v)
    path_guid(args[2]) -- disconnected/invalid home nodes cannot enroll a truck
    assert(parked(v,h),'pair only while physically docked at home and autopilot off')
    registry:pair(args[1],args[2]); log('paired '..args[1]..' home '..args[2]..' (session only)')
end
commands.plan=function(args)
    local v=vehicle(args[1]); authority(v)
    local job=assert(registry.pairs[args[1]],'pair first')
    assert(job.state=='Idle','busy/recovery')
    job.plan=nil -- a rejected replacement must not leave stale route intent available
    local p,q=args[2],args[3]
    assert(p and q and p~=q and p~=job.home and q~=job.home,'need three distinct stations')
    assert(call(building(p),'GetIsInLoadMode') and not call(building(q),'GetIsInLoadMode'),'provider LOAD, requester UNLOAD required')
    job.plan={provider=p,requester=q,intent=validate_legs(v,job.home,p,q)}
    log('three directed legs validated; no route modified. apply requires AllowRouteWrites=true')
end
commands.apply=function(args)
    assert(Config.AllowRouteWrites,'route writes disabled in Config.lua')
    assert(not fatal,'previous bridge exception: restart game and inspect vehicle manually')
    local id=args[1]; local v=vehicle(id); authority(v)
    local job=assert(registry.pairs[id]); local plan=assert(job.plan,'plan first')
    assert(parked(v,home_ok(job.home)),'must be stopped/docked at home')
    assert(call(call(call(v,'GetOwnerVehicle'),'GetStorageInventory'),'IsEmpty'),'truck cargo must start empty')
    assert(call(building(plan.provider),'GetIsInLoadMode') and not call(building(plan.requester),'GetIsInLoadMode'),'station modes changed')
    local intent=validate_legs(v,job.home,plan.provider,plan.requester)
    local backup=route(v)
    registry:reserve(id,plan.provider,plan.requester)
    job.backup,job.intent=backup,intent
    local ok,why=pcall(function()
        call(v,'SetVehicleRoute',intent)
        assert(Core.same_route(route(v),intent),'route readback mismatch')
    end)
    if not ok then fatal=true; recover(id,why); error('FGuid/TArray route bridge failed; no automatic restore: '..tostring(why)) end
    log('route applied/read back; autopilot still off. start is a separate explicit action')
end
commands.start=function(args)
    assert(Config.AllowRouteWrites and not fatal,'writes disabled / bridge fault')
    local id=args[1]; local v=vehicle(id); authority(v)
    local job=assert(registry.pairs[id]); assert(job.state=='Reserved','apply first')
    assert(parked(v,home_ok(job.home)) and Core.same_route(route(v),job.intent),'parking/route changed')
    assert(call(building(job.provider),'GetIsInLoadMode') and not call(building(job.requester),'GetIsInLoadMode'),'station modes changed')
    assert(call(call(call(v,'GetOwnerVehicle'),'GetStorageInventory'),'IsEmpty'),'truck cargo changed before start')
    validate_legs(v,job.home,job.provider,job.requester)
    assert(call(v,'CanEnableAutopilot'),'game rejects autopilot')
    job.state,job.started='ToProvider',os.time()
    active[#active+1]=id
    local ok,why=pcall(function() call(v,'SetAutopilotEnabled',true); assert(call(v,'IsAutopilotEnabled'),'enable failed') end)
    if not ok then recover(id,why); error(why) end
    log(id..' TO_PROVIDER; stop-at-home behavior is not yet runtime-proven')
end
commands.stop=function(args)
    assert(Config.AllowRouteWrites,'writes disabled')
    local id=args[1]; local v=vehicle(id); authority(v)
    assert(registry.pairs[id],'not paired')
    recover(id,'operator stop')
end
commands.reset=function(args)
    local id=args[1]; local v=vehicle(id); authority(v)
    local job=assert(registry.pairs[id])
    assert(parked(v,home_ok(job.home)),'must return to home and stop autopilot manually')
    assert(call(call(call(v,'GetOwnerVehicle'),'GetStorageInventory'),'IsEmpty'),'clear residual cargo first')
    registry:returned(id,true); log(id..' reset at home; original route is not restored')
end
commands.restore=function(args)
    assert(Config.AllowRouteWrites,'writes disabled')
    local id=args[1]; local v=vehicle(id); authority(v)
    local job=assert(registry.pairs[id]); local backup=assert(job.backup,'no original route in this session')
    assert(parked(v,home_ok(job.home)),'must be stopped/docked at home before restoring route')
    call(v,'SetVehicleRoute',backup)
    assert(Core.same_route(route(v),backup),'restore readback failed; inspect vehicle manually')
    registry:returned(id,true)
    log('original route restored; autopilot remains off')
end
commands.status=function()
    for id,job in pairs(registry.pairs) do log(id..' '..job.state..' home='..job.home..' error='..tostring(job.error)) end
end
local function submit(name,args,source,request_id)
    local handler=commands[name]
    if not handler then log('unknown command '..tostring(name)); return end
    log('received '..name..' via '..source)
    local queued,why=pcall(ExecuteInGameThread,function()
        local ok,reason=pcall(handler,args)
        if not ok then log('REJECTED '..name..': '..tostring(reason)) end
        if request_id then
            local detail=ok and '' or tostring(reason):gsub('[\r\n]',' ')
            log('AUTOTEST_RESULT '..request_id..' '..(ok and 'PASS' or 'FAIL')..' '..name..' '..detail)
        end
    end)
    if not queued then
        log('game-thread queue failed: '..tostring(why))
        if request_id then log('AUTOTEST_RESULT '..request_id..' FAIL '..name..' game-thread queue failed') end
    end
end
for name,handler in pairs(commands) do
    -- Satisfactory's console is not guaranteed to use the UGameViewportClient context
    -- required by RegisterConsoleCommandHandler. The global variant covers all contexts.
    RegisterConsoleCommandGlobalHandler('filn_lua.'..name,function(_,args,output)
        if output then
            pcall(function() output:Log('[FILN-Lua] received '..name..'; results in UE4SS.log') end)
        end
        submit(name,args,'console')
        return true
    end)
end
-- Independent of Unreal console interception. These shortcuts perform no route writes.
if RegisterKeyBind and Key and ModifierKey and Key.J and ModifierKey.SHIFT then
    RegisterKeyBind(Key.J,{ModifierKey.CONTROL,ModifierKey.SHIFT},function() submit('dashboard',{},'Ctrl+Shift+J') end)
end
-- A small append-only command inbox provides an alternative when console hooks fail.
-- Skip historical content at startup, bound reads and never evaluate arbitrary Lua.

local inbox_path=script_dir and (script_dir..'/commands.txt')
local inbox_offset=0
if inbox_path then
    local file=io.open(inbox_path,'rb')
    if file then inbox_offset=file:seek('end') or 0; file:close() end
end
local function read_inbox()
    if not inbox_path then return end
    local file=io.open(inbox_path,'rb'); if not file then return end
    local size=file:seek('end')
    if not size then file:close(); return end
    if size<inbox_offset then
        inbox_offset=size; file:close(); log('inbox truncated; skipped contents to prevent replay'); return
    end
    file:seek('set',inbox_offset)
    local data=file:read(4096) or ''; file:close()
    local consumed,count=0,0
    for line in data:gmatch('([^\n]*)\n') do
        if count>=16 then break end
        count=count+1; consumed=consumed+#line+1
        local words={}; for token in line:gmatch('%S+') do words[#words+1]=token end
        local name=table.remove(words,1)
        local request_id
        if name and name:match('^@[%w_%-]+$') then request_id=name:sub(2); name=table.remove(words,1) end
        if name then submit(name:gsub('^filn_lua%.',''),words,'terminal inbox',request_id) end
    end
    inbox_offset=inbox_offset+consumed
    if consumed==0 and #data==4096 then inbox_offset=size; log('oversized inbox line dropped') end
end
-- One queued callback at a time. All engine access occurs inside the game-thread callback.
local game_loop=LoopInGameThreadWithDelay or LoopAsync
game_loop(Config.PollMilliseconds,function()
    local inbox_ok,inbox_error=pcall(read_inbox)
    if not inbox_ok then log('inbox read failed: '..tostring(inbox_error)) end
    if #active==0 or pending then return false end
    pending=true
    ExecuteInGameThread(function()
        local ok,why=pcall(poll); pending=false
        if not ok then fatal=true; log('watchdog fault '..tostring(why)) end
    end)
    return false
end)
log('loaded; Ctrl+Shift+J dashboard + console and terminal inbox ready; read-only route gate='..tostring(not Config.AllowRouteWrites))
if test_run_ok and type(test_run)=='string' and test_run:match('^[%w_%-]+$') then
    log('AUTOTEST_SESSION '..test_run)
end

-- Normal sessions unlock the installed building too; no automated-test token is required.
if NotifyOnNewObject then
    require('RestStopBuilding').watch({call=call,valid=valid,find=StaticFindObject,log=log})
end

-- Register the bounded depot observer from the Lua startup thread, not inside a game callback.
if NotifyOnNewObject then
    depot()
    if Config.DashboardEnabled~=false then dashboard_telemetry():watch()end
end

-- Configuration is read from programmable stations' in-game FilterItem arrays.

-- Unlock the installed mod's existing recipe; retain its native buildable and widget.
if NotifyOnNewObject then
    require('ProgrammableUnlock').watch({call=call,valid=valid,find=StaticFindObject,load=LoadAsset,log=log})
end

if NotifyOnNewObject then
    local auto_pending=false
    local next_auto_error=0
    local game_loop=LoopInGameThreadWithDelay or LoopAsync
    game_loop(250,function()
        if auto_pending or(migration_verifier and migration_verifier.pending)then return false end
        if test_run_ok and type(test_run)=='string' and (not test_prepared or Config.AutoTestAllowAutoDispatch~=true)then return false end
        auto_pending=true
        ExecuteInGameThread(function()
            local safe,problem=pcall(function()
            if not loaded_world()then return end
            local scheduler=automatic()
            if not depot_registration then
                depot_registration=require('DepotRegistration').new({call=call,valid=valid,find=StaticFindObject,authority=authority,now=os.time,log=log,refresh=function()scan(true)end,stations=function()return stations end,vehicles=function()return vehicles end,request_path=assert(script_dir)..'/depot-registration.tsv',result_path=assert(script_dir)..'/depot-registration-result.json'})
            end
            local enrolled,issue=pcall(depot_registration.step,depot_registration)
            if not enrolled then log('DEPOT_REGISTRATION_PENDING '..tostring(issue))end
            scheduler:step()
            end)
            auto_pending=false
            if not safe and os.time()>=next_auto_error then next_auto_error=os.time()+5;log('AUTO_DISPATCH_PENDING '..tostring(problem))end
        end)
        return false
    end)
end

if NotifyOnNewObject then
    NotifyOnNewObject('/Script/FactoryGame.FGVehicleSubsystem',function()
        if depot_runtime then
            depot_runtime.jobs={};depot_runtime.owners={};depot_runtime.active={};depot_runtime.reservations={};depot_runtime.cursor=1
            if depot_runtime.fuel_service then depot_runtime.fuel_service:clear()end
        end
        auto_dispatch=nil;depot_registration=nil;context=nil;vehicles={};stations={};world_readiness:reset()
        if telemetry then telemetry:reset()end
    end)
end

if NotifyOnNewObject then
    local ok,result=pcall(require('MigrationVerify').start,{call=call,valid=valid,find=StaticFindObject,log=log,
        refresh=function()scan(true)end,stations=function()return stations end},assert(script_dir)..'/migration-pending.tsv')
    if ok then migration_verifier=result else migration_verifier={pending=true,failed=true};log('MIGRATION_BLOCKED '..tostring(result))end
end
