-- Category metadata only; blueprint file conversion is performed on backed-up copies offline.
local Bridge=require('Bridge')
local M={}
local order={'Smelting','Constructors','Assemblers','Factory Modules','Truck Stations','Depots'}
function M.manifest(file)
    local stream=assert(io.open(file,'rb'),'FILN blueprint category manifest missing')
    local groups,managed={},{}
    for line in stream:lines()do
        local group,name=line:match('^([^\t]+)\t([^\t\r\n]+)$')
        assert(group and name and name:match('^FILN %- '),'invalid blueprint manifest row')
        local allowed=false;for _,g in ipairs(order)do if group==g then allowed=true end end
        assert(allowed and not managed[name],'invalid category or duplicate blueprint')
        groups[group]=groups[group]or {};groups[group][#groups[group]+1]=name;managed[name]=group
    end
    stream:close();assert(next(managed),'empty FILN blueprint manifest');return groups,managed
end
local function subsystem(api)
    local found
    for _,b in ipairs(FindAllOf('FGBlueprintSubsystem')or {})do if api.valid(b)and api.call(b,'HasAuthority')then assert(not found,'multiple blueprint subsystems');found=b end end
    return assert(found,'authoritative blueprint subsystem unavailable')
end
function M.organize(api,file)
    error('live category mutation disabled after UE4SS crash; use verified offline category migration')
end
function M.verify(api,file)
    local groups,managed=M.manifest(file);local b=subsystem(api);local categories={};api.call(b,'GetAllBlueprintCategories',categories)
    api.log('BLUEPRINT_INITIALIZATION ready='..tostring(b.mHasInitialized)..' client_manifest='..tostring(b.mClientHasGeneratedInitialManifest))
    api.log('BLUEPRINT_CONTROL found='..tostring(api.valid(api.call(b,'GetBlueprintDescriptorByNameString','FILN - Control Station Floor'))))
    for name in pairs(managed)do
        local found=api.valid(api.call(b,'GetBlueprintDescriptorByNameString',name))
        api.log('BLUEPRINT_DISCOVERY name='..name..' found='..tostring(found))
    end
    local seen={};local category_count=0;local original_assignments={}
    for _,cat in ipairs(Bridge.values(categories))do
        local cat_name=Bridge.text(api.call(cat,'GetCategoryNameFromInstanceAsString'));api.log('BLUEPRINT_ROOT name='..cat_name..' undefined='..tostring(api.call(cat,'IsUndefined')));local all_subs={};api.call(cat,'GetSubCategories',all_subs)
        for _,sub in ipairs(Bridge.values(all_subs))do
            for _,desc in ipairs(Bridge.values(api.call(sub,'GetBlueprintDescriptors')))do
                local name=Bridge.text(api.call(desc,'GetItemNameFromInstance'))
                api.log('BLUEPRINT_ALL_NAME name='..name..' category='..cat_name..' sub='..Bridge.text(api.call(sub,'GetCategoryNameFromInstanceAsString')))
                if not managed[name]then original_assignments[name]=cat_name..'/'..Bridge.text(api.call(sub,'GetCategoryNameFromInstanceAsString'))end
            end
        end
        if Bridge.text(api.call(cat,'GetCategoryNameFromInstanceAsString'))=='FILN'then
            local subs={};api.call(cat,'GetSubCategories',subs)
            for _,sub in ipairs(Bridge.values(subs))do
                local group=Bridge.text(api.call(sub,'GetCategoryNameFromInstanceAsString'));local descriptors=Bridge.values(api.call(sub,'GetBlueprintDescriptors'));local count=0
                api.log('BLUEPRINT_GROUP_DIAGNOSTIC name='..group..' descriptors='..#descriptors)
                for _,desc in ipairs(descriptors)do
                    local name=Bridge.text(api.call(desc,'GetItemNameFromInstance'))
                    api.log('BLUEPRINT_NAME_DIAGNOSTIC group='..group..' name='..name..' managed='..tostring(managed[name]~=nil))
                    if managed[name]then assert(managed[name]==group and not seen[name],'wrong or duplicate blueprint category');seen[name]=true;count=count+1;assert(api.call(desc,'GetRecipeRequirementsAreMet'),'blueprint recipes not unlocked: '..name)end
                end
                if groups[group]then category_count=category_count+1;assert(count==#groups[group],'subcategory '..group..' has '..count..' of '..#groups[group]..' blueprint copies');api.log('BLUEPRINT_SUBCATEGORY name='..group..' copies='..count)end
            end
        end
    end
    local expected=0;for name in pairs(managed)do expected=expected+1;assert(seen[name],'blueprint not in FILN category: '..name)end
    assert(category_count==6,'expected six FILN subcategories')
    for name,assignment in pairs(M.baseline or {})do assert(original_assignments[name]==assignment,'original blueprint moved: '..name)end
    api.log('BLUEPRINT_LIBRARY_PASS root=FILN groups=6 copies='..expected..' recipes_available=true')
end
return M
