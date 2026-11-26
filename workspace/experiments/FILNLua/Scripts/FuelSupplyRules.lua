-- Explicit fuel-belt setup: enable only the requested disabled branch, preserve other rules.
local Bridge=require('Bridge')
local M={}
local function rules(api,actor)
    local out={}
    for _,r in ipairs(Bridge.values(api.call(actor,'GetSortRules')))do
        out[#out+1]={ItemClass=Bridge.value(r.ItemClass),OutputIndex=r.OutputIndex}
    end
    return out
end
function M.apply(api,plans)
    local prepared={};local seen={}
    for _,p in ipairs(plans)do
        local actor=p.actor;assert(api.valid(actor),'fuel splitter unavailable')
        local id=actor:GetFullName();assert(not seen[id],'duplicate fuel splitter');seen[id]=true
        api.authority(actor)
        local old=rules(api,actor);local branch,bypass
        for index,r in ipairs(old)do
            if r.OutputIndex==p.output then assert(not branch,'ambiguous fuel branch rule');branch=index end
            if r.OutputIndex==p.bypass then assert(not bypass,'ambiguous bypass rule');bypass=index end
        end
        assert(branch and bypass,'fuel branch or bypass rule missing')
        local any=old[bypass].ItemClass
        assert(api.valid(any) and any:GetFullName():match('Desc_Wildcard_C$'),'bypass is not an unrestricted output; preserve custom configuration')
        local item=old[branch].ItemClass
        assert(api.valid(item) and(item:GetFullName():match('Desc_None_C$')or Bridge.same_object(item,any,api.valid)),'fuel branch has a custom filter; preserve it')
        prepared[#prepared+1]={actor=actor,old=old,index=branch,any=any,home=p.home}
    end
    local touched={}
    local ok,why=pcall(function()
        for _,p in ipairs(prepared)do
            if not Bridge.same_object(p.old[p.index].ItemClass,p.any,api.valid)then
                touched[#touched+1]=p
                api.call(p.actor,'SetSortRuleAt',p.index-1,{ItemClass=p.any,OutputIndex=p.old[p.index].OutputIndex})
            end
            local after=rules(api,p.actor);assert(#after==#p.old,'fuel splitter rule count changed')
            for i,r in ipairs(after)do
                local expected=i==p.index and p.any or p.old[i].ItemClass
                assert(r.OutputIndex==p.old[i].OutputIndex and Bridge.same_object(r.ItemClass,expected,api.valid),'fuel splitter rule readback mismatch')
            end
            api.log('FUEL_BRANCH_READY home='..p.home..' actor='..p.actor:GetFullName()..' branch=Any bypass_unchanged=true')
        end
    end)
    if not ok then
        for i=#touched,1,-1 do local p=touched[i];pcall(api.call,p.actor,'SetSortRules',p.old)end
        error(why)
    end
    return #touched
end
return M
