-- SPDX-License-Identifier: GPL-3.0-only
local M={}
local function integer(n)return type(n)=='number' and n>=0 and n==math.floor(n) and n<math.huge end
function M.evaluate(settings,role,quantity,capacity,truckload)
    if not settings or settings.enabled==false then return false,'station not explicitly configured' end
    if not integer(quantity) or not integer(capacity) then return false,'invalid stock sample' end
    if role=='requester' then return quantity==0,quantity==0 and 'station empty' or 'station not empty' end
    if role=='provider' then
        if not integer(truckload) or truckload<1 then return false,'vehicle cargo capacity required' end
        return quantity>=truckload,quantity>=truckload and 'full truckload available' or 'less than one truckload available'
    end
    return false,'unknown role'
end
function M.storage(name)return type(name)=='string'and name:upper():find('[S]',1,true)~=nil end
function M.pair_allowed(provider,requester)return not(M.storage(provider)and M.storage(requester))end
return M
