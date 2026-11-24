-- SPDX-License-Identifier: GPL-3.0-only
-- No engine dependencies: shared with host tests.
local M = {}
function M.guid(value)
    local out, nonzero = {}, false
    for _, field in ipairs({'A','B','C','D'}) do
        local n = value[field]
        assert(type(n) == 'number' and n == math.floor(n) and n >= -2147483648 and n <= 4294967295,
            'invalid GUID field '..field..' type='..type(n)..' value='..tostring(n))
        -- FGuid words may be exposed as signed int32 by the reflection bridge.
        -- Normalize the same 32-bit pattern rather than rejecting negative words.
        if n < 0 then n=n+4294967296 end
        out[field] = n
        nonzero = nonzero or n ~= 0
    end
    assert(nonzero, 'invalid zero path GUID')
    return out
end
function M.guid_key(g)
    return string.format('%08x%08x%08x%08x',g.A,g.B,g.C,g.D)
end
function M.same_route(a,b)
    if #a ~= #b then return false end
    for i=1,#a do if M.guid_key(a[i]) ~= M.guid_key(b[i]) then return false end end
    return true
end
function M.new()
    local r = {pairs={},homes={},slots={}}
    function r:pair(v,h)
        assert(v and h, 'missing vehicle/home identity')
        if self.pairs[v] then assert(self.pairs[v].home==h, 'truck already has a home'); return self.pairs[v] end
        assert(not self.homes[h] and not self.slots[h], 'home already owned or reserved')
        local p={vehicle=v,home=h,state='Idle'}
        self.pairs[v],self.homes[h]=p,v
        return p
    end
    function r:reserve(v,p,q)
        local job=assert(self.pairs[v], 'truck not paired')
        assert(job.state=='Idle','truck already assigned or recovering')
        assert(p and q and p~=q and p~=job.home and q~=job.home,'route needs distinct provider/requester/home')
        assert(not self.homes[p] and not self.homes[q],'home cannot handle cargo')
        assert(not self.slots[p] and not self.slots[q],'endpoint reserved')
        self.slots[p],self.slots[q]=v,v
        job.provider,job.requester,job.state=p,q,'Reserved'
        return job
    end
    function r:recover(v,why)
        local job=assert(self.pairs[v]); job.state,job.error='Recovery',why
    end
    function r:returned(v,parked)
        assert(parked, 'must confirm stopped at home')
        local job=assert(self.pairs[v])
        if job.provider then self.slots[job.provider]=nil end
        if job.requester then self.slots[job.requester]=nil end
        job.state,job.provider,job.requester,job.error='Idle',nil,nil,nil
    end
    return r
end
function M.advance(state,at)
    if state=='ToProvider' and at=='Provider' then return 'Loading' end
    if state=='Loading' and at~='Provider' then return 'ToRequester' end
    if state=='ToRequester' and at=='Requester' then return 'Unloading' end
    if state=='Unloading' and at~='Requester' then return 'Returning' end
    if state=='Returning' and at=='Home' then return 'HomeArrived' end
    if (state=='ToProvider' and at=='Requester') or (state=='ToRequester' and at=='Home') or (state=='Returning' and at=='Provider') then return 'Recovery' end
    return state
end
return M
