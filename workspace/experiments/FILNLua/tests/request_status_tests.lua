local root=assert(arg[1]);package.path=root..'/?.lua;'..package.path
local Status=require('RequestStatus');local n=0;local function check(v)assert(v);n=n+1 end
local catalog={requesters={{guid='a',label='Fuel',item='coal'},{guid='b',label='Rods',item='ingots'}},providers={{item='coal',quantity=2445,label='mine'},{item='coal',quantity=4800,label='mine2'},{item='ingots',quantity=12,label='smelter'}}}
local rows=Status.rows(catalog,{}, {},function()return 4800 end)
check(#rows==2);check(rows[1].stock==4800 and rows[1].supplier=='mine2');check(rows[1].reason=='waiting for reachable available truck')
check(rows[2].reason=='waiting for full truckload')
rows=Status.rows(catalog,{}, {a='home'},function()return nil end)
check(rows[1].reason=='delivery reserved');check(rows[2].reason=='no eligible vehicle')
check(#Status.rows(nil,{}, {},function()end)==0)
print(n..' request status checks passed; screen rendering not tested')
