-- Explicit opt-in only. Stable depot GUID / vehicle object identity; never use transient V/S IDs.
local B=require('Bridge');local C=require('Core');local Json=require('Json')
local M={}
function M.new(api)
 local self={next_read=0,results=Json.array()}
 local function key(g)return C.guid_key(C.guid(g))end
 local function route(v)return B.values(api.call(v,'GetVehicleRoute'),C.guid)end
 local function write_result(status)
  Json.write(api.result_path,{request_id=self.request,status=status,results=self.results})
 end
 function self:step()
  if not self.queue then
   if api.now()<self.next_read then return end;self.next_read=api.now()+2
   local f=io.open(api.request_path,'rb');if not f then return end
   local content=f:read(65537);f:close();assert(#content<=65536,'registration request too large')
   local request,rest=content:match('^([0-9a-f]+)\n(.*)$');assert(request and #request==32,'invalid registration request')
   if request==self.completed then return end
   -- Acknowledgements survive restarts; recognize a completed request without parsing/evaluating JSON.
   local rf=io.open(api.result_path,'rb');local old=rf and rf:read('*a');if rf then rf:close()end
   if old and old:find('"request_id":"'..request..'"',1,true) and old:find('"status":"complete"',1,true)then self.completed=request;return end
   local queue={};for line in rest:gmatch('[^\n]+')do
    local home,vehicle=line:match('^([0-9a-f]+)\t([^\t\r]+)$');assert(home and #home==32 and #vehicle<4096,'invalid registration pair')
    queue[#queue+1]={home=home,vehicle=vehicle};assert(#queue<=128,'too many registration pairs')
   end
   assert(#queue>0,'empty registration request')
   api.refresh() -- retries harmlessly until a loaded authoritative world exists
   self.queue=queue;self.index=1;self.request=request;self.results=Json.array();write_result('running')
  end
  local entry=self.queue[self.index]
  if not entry then write_result('complete');self.completed=self.request;self.queue=nil;return end
  local ok,why=pcall(function()
   local station,vehicle
   for _,s in pairs(api.stations())do if key(api.call(s,'GetPathNodeGUID'))==entry.home then assert(not station,'duplicate depot GUID');station=s end end
   for _,v in pairs(api.vehicles())do if v:GetFullName()==entry.vehicle then assert(not vehicle,'duplicate vehicle identity');vehicle=v end end
   assert(station and vehicle,'depot or vehicle missing');api.authority(vehicle)
   local r=route(vehicle)
   if #r>0 then assert(key(r[1])==entry.home,'vehicle already has another route');return 'already assigned' end
   local owner=api.call(vehicle,'GetOwnerVehicle');assert(api.valid(owner),'owner missing')
   assert(not api.call(owner,'HasActiveDriver'),'player driving')
   assert(math.abs(api.call(owner,'GetForwardSpeed'))<=5,'truck is moving')
   assert(api.call(api.call(owner,'GetStorageInventory'),'IsEmpty'),'truck carries cargo')
   local audit=B.home_audit(station,api.call,api.valid,api.find);assert(audit.eligible,audit.reason)
   local building=api.call(station,'GetStation')
   local recipe=api.find('/KDataForge/Gen/FILNRestStop.Gen_FILNRestStop_Recipe_RestStop')
   assert(api.valid(recipe)and B.same_object(B.value(building.mBuiltWithRecipe),recipe,api.valid),'target is not a FILN rest stop')
   local node=api.call(building,'GetDockingPathNode')
   local p,q={X=0,Y=0,Z=0},{};api.call(owner,'GetVehicleLocationAndRotation',p,q)
   local location=api.call(node,'K2_GetActorLocation')
   local function near(s)
    local at=api.call(api.call(api.call(s,'GetStation'),'GetDockingPathNode'),'K2_GetActorLocation')
    return math.abs(p.Z-at.Z)<=250 and (p.X-at.X)^2+(p.Y-at.Y)^2+(p.Z-at.Z)^2<=800^2
   end
   assert(near(station),'truck moved away from depot')
   for _,other in pairs(api.stations())do
    if key(api.call(other,'GetPathNodeGUID'))~=entry.home and near(other)then
     local other_building=api.call(other,'GetStation')
     assert(not B.same_object(B.value(other_building.mBuiltWithRecipe),recipe,api.valid),'multiple nearby depots')
    end
   end
   for _,v in pairs(api.vehicles())do
    if v:GetFullName()~=entry.vehicle then
     for _,g in ipairs(route(v))do assert(key(g)~=entry.home,'depot already claimed')end
     local actor=api.call(v,'GetOwnerVehicle')
     if api.valid(actor)then
      local other_pos,rotation={X=0,Y=0,Z=0},{};api.call(actor,'GetVehicleLocationAndRotation',other_pos,rotation)
      assert((other_pos.X-location.X)^2+(other_pos.Y-location.Y)^2+(other_pos.Z-location.Z)^2>800^2,'multiple trucks near depot')
     end
    end
   end
   assert(not api.call(owner,'HasActiveDriver')and #route(vehicle)==0,'vehicle changed before registration')
   local was_enabled=api.call(vehicle,'IsAutopilotEnabled');local intended={C.guid(api.call(station,'GetPathNodeGUID'))}
   local written,problem=pcall(function()
    api.call(vehicle,'SetVehicleRoute',intended);assert(C.same_route(route(vehicle),intended),'home route readback failed')
    api.call(vehicle,'SetAutopilotEnabled',true);assert(api.call(vehicle,'IsAutopilotEnabled'),'autopilot enrollment failed')
   end)
   if not written then
    if not api.call(owner,'HasActiveDriver')and C.same_route(route(vehicle),intended)then
     api.call(vehicle,'SetVehicleRoute',{});api.call(vehicle,'SetAutopilotEnabled',was_enabled)
    end
    error(problem)
   end
   return 'registered; home only; autopilot enabled'
  end)
  self.results[#self.results+1]={home=entry.home,vehicle=entry.vehicle,ok=ok,detail=tostring(why)}
  api.log('DEPOT_REGISTRATION home='..entry.home..' vehicle='..entry.vehicle..' ok='..tostring(ok)..' detail='..tostring(why))
  self.index=self.index+1;write_result('running')
 end
 return self
end
return M
