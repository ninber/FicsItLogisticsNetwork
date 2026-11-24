-- SPDX-License-Identifier: GPL-3.0-only
-- Data-only write-ahead home/route intent; never evaluate persistence as Lua code.
local Core=require('Core')
local Bridge=require('Bridge')
local M={}
local function key(v)return type(v)=='string' and #v==32 and v:match('^[0-9a-f]+$')end
function M.open(path)
    local self={entries={},path=path}
    local f=io.open(path,'rb') or io.open(path..'.bak','rb')
    if f then
        for line in f:lines()do
            local home,provider,requester=line:match('^(%x+)\t([^\t]+)\t(%x+)$')
            assert(key(home) and (key(provider)or provider=='-') and key(requester),'invalid dispatch journal')
            assert(not self.entries[home],'duplicate dispatch journal home');self.entries[home]=provider=='-'and {kind='cargo',requester=requester}or {provider=provider,requester=requester}
        end
        f:close()
    end
    function self:flush()
        local file=assert(io.open(self.path..'.tmp','wb'),'cannot create dispatch journal')
        local keys={};for home in pairs(self.entries)do keys[#keys+1]=home end;table.sort(keys)
        for _,home in ipairs(keys)do local e=self.entries[home];assert(file:write(home..'\t'..(e.kind=='cargo'and '-'or e.provider)..'\t'..e.requester..'\n'))end
        assert(file:close())
        -- Windows rename cannot overwrite; preserve a recoverable old copy.
        os.remove(self.path..'.bak');os.rename(self.path,self.path..'.bak')
        local ok,why=os.rename(self.path..'.tmp',self.path)
        if not ok then os.rename(self.path..'.bak',self.path);error(why)end
    end
    function self:put(home,provider,requester)
        assert(key(home) and key(provider) and key(requester),'invalid journal identifiers')
        local previous=self.entries[home];self.entries[home]={provider=provider,requester=requester}
        local ok,why=pcall(self.flush,self);if not ok then self.entries[home]=previous;error(why)end
    end
    function self:put_cargo(home,requester)
        assert(key(home)and key(requester)and home~=requester,'invalid cargo delivery identifiers')
        local previous=self.entries[home];self.entries[home]={kind='cargo',requester=requester}
        local ok,why=pcall(self.flush,self);if not ok then self.entries[home]=previous;error(why)end
    end
    function self:remove(home)
        local previous=self.entries[home];self.entries[home]=nil
        local ok,why=pcall(self.flush,self);if not ok then self.entries[home]=previous;error(why)end
    end
    function self:recover(api,budget)
        if not self.recovery_pass then
            local keys={};for home in pairs(self.entries)do keys[#keys+1]=home end;table.sort(keys)
            self.recovery_pass={keys=keys,index=1}
        end
        local pass=self.recovery_pass;local remove={};local count=0
        while pass.keys[pass.index]and count<(budget or #pass.keys)do
            local home=pass.keys[pass.index];local e=self.entries[home]
            pass.index=pass.index+1;count=count+1
            if e then
                local ok,why=pcall(function()
                    local runtime=api.runtime and api.runtime()
                    if runtime and runtime.jobs and runtime.jobs[home]then
                        api.log('AUTO_RECOVERY_TRACKED home='..home..' state='..tostring(runtime.jobs[home].state));return
                    end
                    local matches={}
                    local observed=api.recovery_vehicles and api.recovery_vehicles()
                    if not observed then
                        observed={}
                        for _,vehicle in pairs(api.vehicles())do observed[#observed+1]={vehicle=vehicle,route=Bridge.values(api.call(vehicle,'GetVehicleRoute'),Core.guid)}end
                    end
                    for _,entry in ipairs(observed)do
                        local vehicle,route=entry.vehicle,entry.route
                        local keys={};for _,g in ipairs(route)do keys[#keys+1]=Core.guid_key(g)end
                        if (e.kind=='cargo'and ((#keys==3 and keys[1]==home and keys[2]==e.requester and keys[3]==home)or(#keys==2 and (keys[1]==home or keys[2]==home))))or(#keys==1 and keys[1]==home)or(#keys==3 and keys[1]==e.provider and keys[2]==e.requester and keys[3]==home)or(#keys==4 and keys[1]==home and keys[2]==e.provider and keys[3]==e.requester and keys[4]==home)or(#keys==2 and (keys[1]==home or keys[2]==home))then
                            matches[#matches+1]={vehicle=vehicle,route=keys,native_route=route}
                        end
                    end
                    assert(#matches<=1,'ambiguous persisted truck ownership')
                    if #matches==1 then
                        local match=matches[1];api.authority(match.vehicle)
                        assert(Core.same_route(Bridge.values(api.call(match.vehicle,'GetVehicleRoute'),Core.guid),match.native_route),'native route changed since recovery registry')
                        if e.kind=='cargo'then
                            local owner=api.call(match.vehicle,'GetOwnerVehicle')
                            if not api.call(match.vehicle,'IsAutopilotEnabled')then
                                remove[#remove+1]=home;api.log('CARGO_REUSE_RECOVERY_DETACHED home='..home..' controls_unchanged=true')
                            elseif api.call(owner,'HasActiveDriver')then
                                api.log('CARGO_REUSE_RECOVERY_DEFERRED home='..home..' player_present=true')
                            elseif #match.route==1 and api.call(api.call(owner,'GetStorageInventory'),'IsEmpty')then
                                remove[#remove+1]=home
                            elseif api.runtime and api.runtime().restore_cargo then
                                api.runtime():restore_cargo(home,e.requester,match.vehicle)
                                api.log('CARGO_REUSE_RECOVERY_RESTORED home='..home..' requester='..e.requester)
                            else api.log('CARGO_REUSE_RECOVERY_BLOCKED home='..home..' reason=runtime-observer-unavailable')end
                        elseif #match.route>1 and not api.call(match.vehicle,'IsAutopilotEnabled')then
                            remove[#remove+1]=home;api.log('AUTO_RECOVERY_DETACHED home='..home..' autopilot=false controls_unchanged=true')
                        elseif #match.route>1 and api.call(api.call(match.vehicle,'GetOwnerVehicle'),'HasActiveDriver')then
                            api.log('AUTO_RECOVERY_DEFERRED home='..home..' player_present=true controls_unchanged=true')
                        elseif #match.route>1 and api.runtime and api.runtime().restore_persisted then
                            api.runtime():restore_persisted(home,e.provider,e.requester,match.vehicle)
                            api.log('AUTO_RECOVERY_RESTORED home='..home..' observed=true autopilot_unchanged=true')
                        elseif #match.route>1 then
                            api.log('AUTO_RECOVERY_BLOCKED home='..home..' reason=runtime-observer-unavailable controls_unchanged=true')
                        else
                            local owner=api.call(match.vehicle,'GetOwnerVehicle')
                            if api.call(api.call(owner,'GetStorageInventory'),'IsEmpty')then remove[#remove+1]=home end
                        end
                    end
                end)
                if not ok then api.log('AUTO_RECOVERY_BLOCKED home='..home..' retained=true reason='..tostring(why))end
            end
        end
        for _,home in ipairs(remove)do self:remove(home)end
        if not pass.keys[pass.index]then self.recovery_pass=nil;return true end
        return false
    end

    return self
end
return M
