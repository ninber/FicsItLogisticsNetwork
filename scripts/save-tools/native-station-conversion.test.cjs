// SPDX-License-Identifier: GPL-3.0-only
const test=require('node:test'),assert=require('node:assert/strict');
const {convert,guid,VANILLA,REST}=require('./native-station-conversion.cjs');
const {ORDINARY,PROGRAMMABLE,FILN}=require('./station-migration.cjs');
const iron='/Game/FactoryGame/Resource/Parts/IronIngot/Desc_IronIngot.Desc_IronIngot_C';
function fixture(depot=false) {
 const p={mBuiltWithRecipe:{value:{pathName:depot?REST:FILN}},mStationIdentifier:{value:{pathName:'id'}},mDockingPathNode:{value:{pathName:'node'}},mIsInLoadMode:{value:depot}};
 if(!depot)p.FilterItem={values:[{pathName:iron}]};
 return {levels:{main:{objects:[{instanceName:'s',typePath:depot?ORDINARY:PROGRAMMABLE,components:[{pathName:'s.inventory'}],properties:p,transform:{x:12}},{instanceName:'id',properties:{name:'kept'}},{instanceName:'node',properties:{mPathNodeGUID:{value:[1,2,3,4]},roads:['a','b']}},{instanceName:'s.inventory',properties:{mInventoryStacks:{values:[]}}}]}}};
}
test('preserves identity and resource assignment',()=>{const source=fixture();const before=structuredClone(source),r=convert(source,['s']);assert.deepEqual(source,before);assert.equal(r.save.levels.main.objects[0].typePath,ORDINARY);assert.equal(r.save.levels.main.objects[0].properties.mBuiltWithRecipe.value.pathName,VANILLA);assert.match(r.manifest,/station\t00000001000000020000000300000004\t.*IronIngot.*\tUNLOAD/);assert.deepEqual(r.save.levels.main.objects.slice(1),before.levels.main.objects.slice(1));});
test('depot recipe conversion preserves native actor',()=>{const r=convert(fixture(true),['s']);assert.equal(r.changes[0].oldClass,ORDINARY);assert.match(r.manifest,/depot\t.*\t-\tLOAD/);});
test('rejects ambiguous/missing selections without touching source',()=>{const s=fixture(),before=structuredClone(s);assert.throws(()=>convert(s,['s','s']));assert.throws(()=>convert(s,['s','missing']));assert.deepEqual(s,before);});
test('rejects mixed/unknown filters and missing nodes',()=>{const s=fixture();s.levels.main.objects[0].properties.FilterItem.values.push({pathName:iron});assert.throws(()=>convert(s,['s']));const t=fixture();t.levels.main.objects[0].properties.mDockingPathNode.value.pathName='missing';assert.throws(()=>convert(t,['s']));});
test('reports remaining legacy references without stripping them',()=>{const s=fixture();s.levels.main.objects[1].properties.other='/KDataForge/Gen/FILNRestStop.Other';assert.deepEqual(convert(s,['s']).residual,['id']);});
test('validates road GUIDs',()=>{assert.equal(guid([-1,1,2,3]),'ffffffff000000010000000200000003');assert.throws(()=>guid([0,0,0,0]));assert.throws(()=>guid([1,2,3]));});
