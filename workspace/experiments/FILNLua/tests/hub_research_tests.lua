local root=assert(arg[1]);package.path=root..'/?.lua;'..package.path
local H=require('HubResearch');local n=0;local function check(v)assert(v);n=n+1 end
local function obj(name)return {IsValid=function()return true end,GetFullName=function()return name end}end
local milestone=obj('Logistics');local active=milestone;local bought=true;local repeatable=false;local reject=false;local changes=0;local authority=true
local manager=obj('manager');manager.HasAuthority=function()return authority end;manager.GetActiveSchematic=function()return active end;manager.IsSchematicPurchased=function()return bought end;manager.GetAllPurchasedSchematics=function(_,out)if bought then out[1]=milestone end end
manager.SetActiveSchematic=function(_,value)changes=changes+1;if not reject then active=value end end
local library={GetSchematicDisplayName=function()return 'Logistics' end,GetTechTier=function()return 1 end,IsRepeatPurchasesAllowed=function()return repeatable end}
local api={valid=function(o)return o and o.IsValid and o:IsValid()end,call=function(o,m,...)return o[m](o,...)end,find=function()return library end,log=function()end}
check(H.repair(api,manager));check(active==nil);check(changes==1);check(bought)
check(not H.repair(api,manager));check(changes==1)
active=milestone;bought=false;check(not H.repair(api,manager));check(active==milestone);bought=true
repeatable=true;check(not H.repair(api,manager));check(active==milestone);repeatable=false
reject=true;check(not pcall(H.repair,api,manager));check(active==milestone);reject=false
authority=false;check(not pcall(H.repair,api,manager))
print(n..' HUB stale-selection checks passed; native UI/save persistence not tested')
