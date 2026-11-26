-- Move existing manually supplied coal into each truck's own depot fuel slot.
-- One coal remains in each truck; no items are created or cargo inventories used.
local M={}
function M.prepare(api,pairs,coal)
    assert(api.snapshot==true,'prepared disposable snapshot required')
    assert(api.valid(coal),'coal descriptor missing')
    local plan={}
    for _,p in ipairs(pairs)do
        assert(not api.call(p.owner,'HasActiveDriver'),'refueling fixture requires normal player exit')
        local source=api.call(p.owner,'GetFuelInventory');local destination=api.call(p.building,'GetFuelInventory')
        assert(api.valid(source) and api.valid(destination),'fuel inventory unavailable')
        local before=api.call(source,'GetNumItems',coal)
        assert(before>1 and before<=100 and api.call(destination,'IsEmpty'),'fixture requires supplied truck coal and empty depot fuel slot')
        plan[#plan+1]={home=p.home,source=source,destination=destination,before=before,amount=before-1}
    end
    for _,p in ipairs(plan)do
        assert(api.call(p.destination,'AddStack',{Item={ItemClass=coal},NumItems=p.amount},false)==p.amount,'cannot store truck fuel in depot fuel slot')
        api.call(p.source,'RemoveFromIndex',0,p.amount,p.destination)
        assert(api.call(p.source,'GetNumItems',coal)==1 and api.call(p.destination,'GetNumItems',coal)==p.amount,'fuel conservation/readback failed')
        api.log('REFUEL_FIXTURE_READY home='..p.home..' truck_items=1 depot_items='..p.amount..' conserved='..p.before..' snapshot_only=true')
    end
    return #plan
end
return M
