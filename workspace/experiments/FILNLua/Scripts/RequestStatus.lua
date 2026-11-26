-- Read-only view of the scheduler's last completed stock scan; no extra world search.
local M={}
function M.rows(catalog,jobs,reservations,capacity)
    local rows={}
    for _,request in ipairs(catalog and catalog.requesters or {})do
        local best=0;local supplier='none'
        for _,provider in ipairs(catalog.providers)do
            if provider.item==request.item and provider.quantity>best then best=provider.quantity;supplier=provider.label end
        end
        local need=capacity(request)
        local reason=reservations[request.guid] and 'delivery reserved' or
            (not need and 'no eligible vehicle' or(best<need and 'waiting for full truckload' or 'waiting for reachable available truck'))
        rows[#rows+1]={guid=request.guid,label=request.label,item=request.item,stock=best,truckload=need,supplier=supplier,reason=reason}
    end
    for _,job in pairs(jobs)do
        if job.state~='Idle' and job.state~='Detached' then
            rows[#rows+1]={guid=job.home,label='Truck '..job.vehicle_key,item=job.resource and job.resource:GetFullName()or 'return to depot',
                stock=job.loaded,truckload=job.truckload,reason=job.state..(job.recovery and '/'..job.recovery.state or '')}
        end
    end
    table.sort(rows,function(a,b)return a.guid<b.guid end)
    return rows
end
return M
