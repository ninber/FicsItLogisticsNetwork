-- Verify one migrated station per pass before permitting new automatic assignments.
local Core=require('Core')
local Bridge=require('Bridge')
local M={}
local recipe_path='/KDataForge/Gen/FILNStations.Gen_FILNStations_Recipe_LogisticsStation'
local function suffix(object,expected)return object:GetFullName():sub(-#expected)==expected end
function M.verify(api,entry,identifier)
    local b=api.call(identifier,'GetStation');assert(api.valid(b),'station missing')
    assert(b:GetClass():GetFullName():match('%.Build_ProgrammableTruckStation_C$'),'wrong migrated class')
    assert((api.call(b,'GetIsInLoadMode')and 'LOAD'or 'UNLOAD')==entry.mode,'station mode changed')
    assert(Bridge.same_object(Bridge.value(b.mBuiltWithRecipe),api.find(recipe_path),api.valid),'wrong migrated recipe')
    local node=api.call(b,'GetDockingPathNode');assert(api.valid(node)and suffix(node,entry.node),'road node identity changed')
    assert(Core.guid_key(Core.guid(api.call(node,'GetPathNodeGUID')))==entry.guid,'road GUID changed')
    assert(#Bridge.values(api.call(node,'GetArrivingConnections'))==entry.incoming,'incoming road connections changed')
    assert(#Bridge.values(api.call(node,'GetLeavingConnections'))==entry.outgoing,'outgoing road connections changed')
    local inventory=api.call(b,'GetInventory');assert(api.valid(inventory)and suffix(inventory,entry.inventory),'cargo inventory identity changed')
    local filters={};for _,item in ipairs(Bridge.values(b.FilterItem))do
        if api.valid(item)then local name=item:GetFullName();filters[#filters+1]=name:match('^%S+ (.+)$')or name end
    end
    assert(table.concat(filters,'|')==entry.filters,'operator filter changed')
    return true
end
function M.start(api,path)
    local self={pending=false,passed=0,index=1,entries={},registry=nil}
    local f=io.open(path,'rb');if not f then return self end
    self.pending=true;assert(f:read('*l')=='FILN_MIGRATION_V1','invalid migration plan header')
    for line in f:lines()do
        local fields={};for value in (line..'\t'):gmatch('(.-)\t')do fields[#fields+1]=value end
        assert(#fields==7 and #fields[1]==32 and fields[1]:match('^[0-9a-f]+$'),'invalid migration row')
        assert(fields[2]=='LOAD'or fields[2]=='UNLOAD','invalid migration mode')
        local incoming,outgoing=tonumber(fields[6]),tonumber(fields[7]);assert(incoming and outgoing and incoming>=0 and outgoing>=0,'invalid road counts')
        self.entries[#self.entries+1]={guid=fields[1],mode=fields[2],filters=fields[3],inventory=fields[4],node=fields[5],incoming=incoming,outgoing=outgoing}
    end
    f:close();assert(#self.entries>0,'migration plan is empty')
    local attempts=0
    local game_loop=LoopInGameThreadWithDelay or LoopAsync
    game_loop(1000,function()
        if not self.pending or self.failed then return false end
        ExecuteInGameThread(function()
            if not api.valid(api.find(recipe_path))then attempts=attempts+1;if attempts==60 then self.failed=true;api.log('MIGRATION_BLOCKED FILN recipe unavailable')end;return end
            if not self.registry then
                local ok=pcall(api.refresh)
                if not ok then attempts=attempts+1;if attempts==60 then self.failed=true;api.log('MIGRATION_BLOCKED world not ready')end;return end
                self.registry={}
                for _,identifier in pairs(api.stations())do self.registry[Core.guid_key(Core.guid(api.call(identifier,'GetPathNodeGUID')))]=identifier end
                for _,entry in ipairs(self.entries)do if not self.registry[entry.guid]then self.registry=nil;return end end
            end
            local entry=self.entries[self.index]
            local ok,why=pcall(M.verify,api,entry,self.registry[entry.guid])
            if not ok then self.failed=true;api.log('MIGRATION_BLOCKED guid='..entry.guid..' reason='..tostring(why));return end
            self.passed=self.passed+1;self.index=self.index+1
            api.log('MIGRATION_STATION_PASS guid='..entry.guid..' role='..entry.mode..' identity=true inventory=true filter=true roads=true')
            if self.index>#self.entries then
                local renamed,reason=os.rename(path,path..'.verified')
                if not renamed then self.failed=true;api.log('MIGRATION_BLOCKED cannot finalize verification: '..tostring(reason));return end
                self.pending=false;api.log('MIGRATION_COMPLETE stations='..self.passed..' automatic_dispatch_released=true')
            end
        end)
        return false
    end)
    return self
end
return M
