// SPDX-License-Identifier: GPL-3.0-only
const test=require('node:test'),assert=require('node:assert/strict');
const {recover,asset,SUBSYSTEM}=require('./recover-native-stations.cjs');
const {ORDINARY}=require('./station-migration.cjs');
const item='/Game/FactoryGame/Resource/RawResources/Coal/Desc_Coal.Desc_Coal_C';
const fluid='/Game/FactoryGame/Test/FluidStation.FluidStation_C';
const obj=pathName=>({value:{pathName}});
const suffixes=['FILNDepot','FILNFluidDepot','FILNStation','FILNFluidStation'];
const receipt={schema:4,buildings:suffixes.map(s=>({actor:asset('Build',s),recipe:asset('Recipe',s),descriptor:asset('Desc',s),
 vanilla_actor:s.includes('Fluid')?fluid:ORDINARY,vanilla_recipe:s.includes('Fluid')?'/Game/FactoryGame/Test/FluidRecipe.FluidRecipe_C':'/Game/FactoryGame/Test/SolidRecipe.SolidRecipe_C'}))};
function fixture() {
 const objects=[];
 for(let i=0;i<4;i++) {
  const s=suffixes[i], row=receipt.buildings[i];
  objects.push({instanceName:'s'+i,typePath:i<2?row.actor:row.vanilla_actor,
   properties:{mBuiltWithRecipe:obj(i<2?row.recipe:row.vanilla_recipe),mStationIdentifier:obj('id'+i),mDockingPathNode:obj('n'+i),mIsInLoadMode:{value:i!==2},mDockedActor:obj(i===2?'truck':'')},
   components:[{pathName:'inventory'+i}]},
   {instanceName:'id'+i,properties:{name:'original '+i}},
   {instanceName:'n'+i,properties:{mPathNodeGUID:{value:[1,2,3,i+1]},edges:['a','b']}},
   {instanceName:'inventory'+i,properties:{mInventoryStacks:{values:[{properties:{NumItems:{value:12}}}]},mAllowedItemDescriptors:{values:[]}}});
 }
 objects.push({instanceName:'registry',typePath:SUBSYSTEM,properties:{StationConfigs:{values:[{properties:{Station:obj('id2'),Resource:obj(item),OriginalFilters:{values:[{pathName:''}]}}}]}}});
 return {levels:{main:{objects}}};
}
const history={station_records:[{actor:'s2',road_guid:'00000001000000020000000300000003',resource:item}]};
test('all solid/fluid logistics stations become their distinct FILN classes; docked, blank and loaded stations included',()=>{
 const s=fixture(), before=structuredClone(s),r=recover(s,receipt,history);
 assert.deepEqual(s,before);
 assert.equal(r.stations.length,4);
 assert.equal(r.changes.length,2);
 assert.equal(r.stations.find(x=>x.actor==='s2').resource,item);
 assert.equal(r.stations.find(x=>x.actor==='s3').resource,null);
 for(const [i,suffix] of suffixes.entries()) {
  const a=r.save.levels.main.objects.find(o=>o.instanceName==='s'+i);
  assert.equal(a.typePath,asset('Build',suffix));
  assert.equal(a.properties.mBuiltWithRecipe.value.pathName,asset('Recipe',suffix));
 }
 assert.deepEqual(r.save.levels.main.objects.at(-1),s.levels.main.objects.at(-1));
 assert.equal(recover(r.save,receipt,history).changes.length,0);
});
test('rejects missing historical filter rather than migrating it as blank',()=>{
 const s=fixture();s.levels.main.objects.at(-1).properties.StationConfigs.values=[];
 assert.throws(()=>recover(s,receipt,history),/Historical filter missing/);
});
test('rejects class/recipe, identity, asset and depot filter conflicts',()=>{
 const s=fixture();assert.throws(()=>recover(s,{...receipt,buildings:receipt.buildings.slice(0,3)},history));
 assert.throws(()=>recover(s,receipt,{station_records:[{...history.station_records[0],road_guid:'00000001000000020000000300000004'}]}),/identity mismatch/);
 s.levels.main.objects.at(-1).properties.StationConfigs.values[0].properties.Station=obj('id0');
 assert.throws(()=>recover(s,receipt),/Depot has logistics filter/);
});
test('a fully absent historical identity is explicitly reported, never applied to another station',()=>{
 const r=recover(fixture(),receipt,{station_records:[...history.station_records,{actor:'deleted',road_guid:'ffffffffffffffffffffffffffffffff',resource:item}]});
 assert.deepEqual(r.historicalMissing,['deleted']);
});
