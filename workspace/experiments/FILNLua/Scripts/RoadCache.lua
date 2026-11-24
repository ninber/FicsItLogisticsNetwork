-- SPDX-License-Identifier: GPL-3.0-only
-- Networks are recreated on topology changes; traversability has a native revision.
local Graph=require('RoadGraph')
local M={}
function M.new()
    local self={networks={},order={},builds=0,hits=0,queries=0}
    function self:distances(api,network,preset,requester)
        local key=network:GetFullName()..'|'..preset:GetFullName()
        local rank_revision=api.rank_revision and api.rank_revision()
        if rank_revision==nil and api.rank_nodes then
            local nodes={};for _,id in ipairs(api.rank_nodes())do nodes[#nodes+1]=id end;table.sort(nodes);rank_revision=table.concat(nodes,'|')
        end
        local revision=api.call(network,'GetNetworkPathTraversabilityChangelist')
        local entry=self.networks[key]
        if not entry then
            self.order[#self.order+1]=key
            if #self.order>8 then self.networks[table.remove(self.order,1)]=nil end
        end
        if not entry or entry.revision~=revision then
            entry={revision=revision,rank_revision=rank_revision,graph=Graph.snapshot(api,network,preset),requests={}}
            self.networks[key]=entry;self.builds=self.builds+1
        end
        if entry.rank_revision~=rank_revision then
            -- Sparse projections omit unregistered stations; topology need not change
            -- when delayed discovery adds one. Keep the graph, invalidate projections.
            entry.rank_revision=rank_revision;entry.requests={}
            self.rank_invalidations=(self.rank_invalidations or 0)+1
        end
        if entry.requests[requester]then self.hits=self.hits+1;return entry.requests[requester]end
        local distances=entry.graph:distances(requester)
        if api.rank_nodes then
            local sparse={};for _,id in ipairs(api.rank_nodes())do sparse[id]=distances[id]end;distances=sparse
        end
        entry.requests[requester]=distances;self.queries=self.queries+1
        return distances
    end
    return self
end
return M
