local root=assert(arg[1]);package.path=root..'/?.lua;'..package.path
local Rules=require('FuelSupplyRules');local n=0;local function check(v)assert(v);n=n+1 end
local function item(name)return {GetFullName=function()return name end}end
local any,none,coal=item('Class Desc_Wildcard_C'),item('Class Desc_None_C'),item('Class Desc_Coal_C')
local writes=0;local corrupt=false
local function splitter(name)
 local o={GetFullName=function()return name end,data={{ItemClass=any,OutputIndex=0},{ItemClass=none,OutputIndex=2},{ItemClass=none,OutputIndex=1}}}
 o.GetSortRules=function(self)return self.data end
 o.SetSortRuleAt=function(self,index,r)writes=writes+1;self.data[index+1]=r;if corrupt then self.data[1]={ItemClass=coal,OutputIndex=0}end end
 o.SetSortRules=function(self,r)self.data=r end
 return o
end
local a,b=splitter('a'),splitter('b')
local api={call=function(o,m,...)return o[m](o,...)end,valid=function(o)return o~=nil end,authority=function()end,log=function()end}
local plan={{actor=a,home='h1',output=2,bypass=0},{actor=b,home='h2',output=2,bypass=0}}
check(Rules.apply(api,plan)==2);check(a.data[2].ItemClass==any and a.data[1].ItemClass==any and a.data[3].ItemClass==none)
check(Rules.apply(api,plan)==0 and writes==2)
a.data[2].ItemClass=none;b.data[2].ItemClass=coal
check(not pcall(Rules.apply,api,plan));check(a.data[2].ItemClass==none and writes==2)
b.data[2].ItemClass=none;corrupt=true
check(not pcall(Rules.apply,api,plan));check(a.data[2].ItemClass==none and a.data[1].ItemClass==any)
corrupt=false;check(not pcall(Rules.apply,api,{plan[1],plan[1]}))
print(n..' fuel-branch setup checks passed: native mutations not tested')
