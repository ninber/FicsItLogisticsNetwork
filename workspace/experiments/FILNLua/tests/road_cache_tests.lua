local root=assert(arg[1]);package.path=root..'/?.lua;'..package.path
local Cache=require('RoadCache');local Core=require('Core');local n=0;local function check(v)assert(v);n=n+1 end
local function g(i)return {A=i,B=0,C=0,D=0}end
local version=1;local name='network1';local reads=0
local network={GetFullName=function()return name end,GetNetworkPathTraversabilityChangelist=function()return version end,
 GetPathNodes=function()reads=reads+1;return {{PathNodeGuid=g(1)},{PathNodeGuid=g(2)},{PathNodeGuid=g(3)}}end,
 GetPathSegments=function()return {{FromNodeIndex=0,ToNodeIndex=1,SegmentLength=2},{FromNodeIndex=1,ToNodeIndex=2,SegmentLength=3}}end,CanVehicleTraverseSegment=function()return true end}
local preset={GetFullName=function()return 'truck'end}
local api={call=function(o,m,...)return o[m](o,...)end,rank_nodes=function()return {Core.guid_key(g(1)),Core.guid_key(g(3))}end}
local c=Cache.new();local first=c:distances(api,network,preset,Core.guid_key(g(3)))
check(first[Core.guid_key(g(1))]==5);check(first[Core.guid_key(g(2))]==nil)
check(c:distances(api,network,preset,Core.guid_key(g(3)))==first);check(reads==1 and c.hits==1)
version=2;check(c:distances(api,network,preset,Core.guid_key(g(3)))~=first);check(reads==2)
name='rebuilt-network';c:distances(api,network,preset,Core.guid_key(g(3)));check(reads==3)
for i=1,12 do name='network'..i;c:distances(api,network,preset,Core.guid_key(g(3)))end
local count=0;for _ in pairs(c.networks)do count=count+1 end;check(count==8)
print(n..' lazy road-cache checks passed: sparse ranks, cache reuse, topology/revision invalidation and bounded history')
-- A newly registered station in an already cached graph must become queryable.
-- Topology and traversability may stay identical when registry discovery catches up.
name='station-registry-change';local ranked={Core.guid_key(g(1)),Core.guid_key(g(3))}
api.rank_nodes=function()return ranked end
local registry_cache=Cache.new();local initial=registry_cache:distances(api,network,preset,Core.guid_key(g(3)))
check(initial[Core.guid_key(g(2))]==nil)
ranked={Core.guid_key(g(1)),Core.guid_key(g(2)),Core.guid_key(g(3))}
local expanded=registry_cache:distances(api,network,preset,Core.guid_key(g(3)))
check(expanded[Core.guid_key(g(2))]==3)
print('Station-registry growth updates sparse cached distances check passed')
check(registry_cache.rank_invalidations==1 and registry_cache.builds==1)
ranked={Core.guid_key(g(3)),Core.guid_key(g(2)),Core.guid_key(g(1))}
check(registry_cache:distances(api,network,preset,Core.guid_key(g(3)))==expanded)
check(registry_cache.rank_invalidations==1)
-- Production supplies an O(1) generation instead of hashing the station list on every query.
local generation=1;api.rank_revision=function()return generation end
local generated=Cache.new();generated:distances(api,network,preset,Core.guid_key(g(3)))
generation=2;generated:distances(api,network,preset,Core.guid_key(g(3)))
check(generated.rank_invalidations==1 and generated.builds==1)
print('Rank projection invalidates on membership change, not reordering, and retains graph snapshot')
