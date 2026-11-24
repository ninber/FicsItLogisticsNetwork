local root=assert(arg[1]);package.path=root..'/?.lua;'..package.path
local P=require('StockPolicy');local n=0
local function check(v)assert(v);n=n+1 end
for _,q in ipairs({0,1,8,4799,4800})do
 check(P.evaluate({},'requester',q,4800)==(q==0))
 check(P.evaluate({},'provider',q,4800,4800)==(q>=4800))
end
check(not P.evaluate({},'provider',4800,4800))
check(P.evaluate({},'provider',2500,4800,2500))
check(not P.evaluate({},'provider',2499,4800,2500))
check(not P.evaluate(nil,'requester',0,4800))
check(not P.evaluate({enabled=false},'requester',0,4800))
print(n..' truckload policy checks passed; no configurable thresholds or hysteresis')
local storage=require('StockPolicy')
assert(not storage.pair_allowed('[S] OUT','[S] IN'))
assert(not storage.pair_allowed('Warehouse [s] OUT','Warehouse [S] IN'))
assert(storage.pair_allowed('[S] OUT','Factory IN'))
assert(storage.pair_allowed('Factory OUT','[S] IN'))
assert(storage.pair_allowed('Factory OUT','Factory IN'))
assert(storage.pair_allowed('S OUT','[S] IN'))
print('Storage tag pairing checks passed')
