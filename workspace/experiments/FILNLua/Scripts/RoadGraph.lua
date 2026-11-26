-- Immutable directed physical-length snapshot; independent of stock and job activity.
local Bridge=require('Bridge')
local Core=require('Core')
local M={}
function M.new(nodes,edges)
    local graph={index={},reverse={}}
    for i,id in ipairs(nodes)do assert(not graph.index[id],'duplicate node');graph.index[id]=i;graph.reverse[i]={}end
    for _,e in ipairs(edges)do
        assert(graph.reverse[e.from] and graph.reverse[e.to] and e.length>=0 and e.length<math.huge,'invalid directed edge')
        graph.reverse[e.to][#graph.reverse[e.to]+1]={to=e.from,length=e.length}
    end
    function graph:distances(requester)
        local target=assert(self.index[requester],'requester node missing')
        local distances={[target]=0};local heap={}
        local function push(node,d)
            local i=#heap+1;heap[i]={node=node,d=d}
            while i>1 do local parent=math.floor(i/2);if heap[parent].d<=d then break end;heap[i],heap[parent]=heap[parent],heap[i];i=parent end
        end
        local function pop()
            local result=heap[1];local last=table.remove(heap)
            if #heap>0 then
                heap[1]=last;local i=1
                while true do
                    local child=i*2;if child>#heap then break end
                    if child+1<=#heap and heap[child+1].d<heap[child].d then child=child+1 end
                    if heap[i].d<=heap[child].d then break end
                    heap[i],heap[child]=heap[child],heap[i];i=child
                end
            end
            return result
        end
        push(target,0)
        while #heap>0 do
            local current=pop()
            if distances[current.node]==current.d then
                for _,edge in ipairs(self.reverse[current.node])do
                    local d=current.d+edge.length
                    if not distances[edge.to]or d<distances[edge.to]then distances[edge.to]=d;push(edge.to,d)end
                end
            end
        end
        local out={};for id,index in pairs(self.index)do out[id]=distances[index]end;return out
    end
    return graph
end
function M.snapshot(api,network,preset)
    local nodes,edges={},{}
    for _,data in ipairs(Bridge.values(api.call(network,'GetPathNodes')))do
        nodes[#nodes+1]=Core.guid_key(Core.guid(data.PathNodeGuid))
    end
    for _,data in ipairs(Bridge.values(api.call(network,'GetPathSegments')))do
        if api.call(network,'CanVehicleTraverseSegment',data,preset)then
            edges[#edges+1]={from=data.FromNodeIndex+1,to=data.ToNodeIndex+1,length=data.SegmentLength}
        end
    end
    return M.new(nodes,edges)
end
return M
