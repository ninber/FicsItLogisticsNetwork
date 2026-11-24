-- Menu/loading is normal: wait for exactly one authoritative vehicle world, without log spam.
local M={}
function M.new(api)
 local self={next_probe=0}
 local function usable(object)
  local ok,result=pcall(function()return api.valid(object)and api.call(object,'HasAuthority')==true and api.valid(api.call(object,'GetWorld'))end)
  return ok and result==true
 end
 function self:reset()self.cached=nil;self.next_probe=0 end
 function self:ready(existing)
  if usable(existing)then self.cached=existing;return existing end
  if usable(self.cached)then return self.cached end
  self.cached=nil
  if api.now()<self.next_probe then return nil end
  self.next_probe=api.now()+2
  local ok,candidates=pcall(api.find)
  if not ok then return nil end
  local found
  for _,candidate in ipairs(candidates or {})do
   if usable(candidate)then if found then return nil end;found=candidate end
  end
  self.cached=found;return found
 end
 return self
end
return M
