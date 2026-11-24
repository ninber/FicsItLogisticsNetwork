local root=assert(arg[1]);package.path=root..'/?.lua;'..package.path
local Graph=require('RoadGraph')
local g=Graph.new({'near','far','request','home','detour'},{
 {from=1,to=5,length=100},{from=5,to=3,length=100},
 {from=2,to=3,length=50},{from=3,to=4,length=5},{from=4,to=1,length=5}})
local d=g:distances('request')
assert(d.near==200 and d.far==50 and d.home==205)
assert(g:distances('far').near==nil) -- no reverse-direction assumption
local alternate=Graph.new({'a','b','c'},{{from=1,to=3,length=30},{from=1,to=2,length=2},{from=2,to=3,length=3}})
assert(alternate:distances('c').a==5)
assert(not pcall(Graph.new,{'a'},{{from=1,to=1,length=-1}}))
print('4 directed-road distance checks passed; native snapshot APIs NOT tested')
