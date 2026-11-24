local root=assert(arg[1]);package.path=root..'/?.lua;'..package.path
local Library=require('BlueprintLibrary');local n=0;local function check(v)assert(v);n=n+1 end
local file=os.tmpname();local f=assert(io.open(file,'w'));local order={'Smelting','Constructors','Assemblers','Factory Modules','Truck Stations','Depots'}
for _,group in ipairs(order)do f:write(group..'\tFILN - '..group..'\n')end;f:close()
local desc={}
local function string_value(name)return {ToString=function()return name end}end
local function descriptor(name)return {GetItemNameFromInstance=function()return string_value(name)end,GetRecipeRequirementsAreMet=function()return true end}end
for _,group in ipairs(order)do desc['FILN - '..group]=descriptor('FILN - '..group)end;desc.Original=descriptor('Original')
local records={{CategoryName='Old',MenuPriority=1,IconID=4,IsUndefined=false,LastEditedBy={},SubCategoryRecords={{SubCategoryName='Old Sub',MenuPriority=1,IsUndefined=0,LastEditedBy={},BlueprintNames={'Original','FILN - Smelting'}}}}}
local b={HasAuthority=function()return true end,RefreshBlueprintsAndDescriptors=function()end,GetBlueprintDescriptorByNameString=function(_,name)return desc[name]end}
local function categories()
 local result={}
 for _,r in ipairs(records)do
  local rec=r;local cat={IsUndefined=function()return rec.IsUndefined end,GetCategoryNameFromInstanceAsString=function()return string_value(rec.CategoryName)end}
  cat.GetSubCategories=function(_,out)for _,s in ipairs(rec.SubCategoryRecords)do local sub=s;out[#out+1]={GetCategoryNameFromInstanceAsString=function()return string_value(sub.SubCategoryName)end,GetBlueprintDescriptors=function()local out={};for _,name in ipairs(sub.BlueprintNames)do out[#out+1]=desc[name]end;return out end}end end
  result[#result+1]=cat
 end;return result
end
b.GetAllBlueprintCategories=function(_,out)for _,cat in ipairs(categories())do out[#out+1]=cat end end
local rco={Server_SetCategoryRecordData=function(_,r)records=r;b.mBlueprintCategoryRecords=r end};b.mBlueprintCategoryRecords=records
local controller={GetRemoteCallObjectOfClass=function()return rco end};local pawn={IsLocallyControlled=function()return true end,GetController=function()return controller end}
_G.FindAllOf=function(class)return class=='FGBlueprintSubsystem'and {b}or {pawn}end
local api={call=function(o,m,...)return o[m](o,...)end,valid=function(o)return o~=nil end,find=function()return {}end,log=function()end}
check(not pcall(Library.organize,api,file));check(#records==1)
records[1].SubCategoryRecords[1].BlueprintNames={'Original'}
local filn={CategoryName='FILN',MenuPriority=2,IconID=4,IsUndefined=false,SubCategoryRecords={}}
for index,group in ipairs(order)do filn.SubCategoryRecords[#filn.SubCategoryRecords+1]={SubCategoryName=group,MenuPriority=index,BlueprintNames={'FILN - '..group}}end
records[#records+1]=filn;Library.verify(api,file);check(true)
f=assert(io.open(file,'w'));f:write('Unknown\tFILN - bad\n');f:close();check(not pcall(Library.manifest,file));os.remove(file)
print(n..' blueprint category checks passed: read-only membership, unsafe mutation blocked and invalid manifest; native API not tested')
