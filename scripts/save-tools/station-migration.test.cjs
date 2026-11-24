const test=require('node:test'),assert=require('node:assert/strict');
const {plan,itemFromName,ORDINARY,PROGRAMMABLE,FILN}=require('./station-migration.cjs');
const iron='/Game/FactoryGame/Resource/Parts/IronIngot/Desc_IronIngot.Desc_IronIngot_C';
const copper='/Game/FactoryGame/Resource/Parts/CopperIngot/Desc_CopperIngot.Desc_CopperIngot_C';
function fixture(name,stock=[],kind=ORDINARY,filter){
 const station={typePath:kind,instanceName:'station',components:[{pathName:'station.inventory'}],properties:{mStationIdentifier:{value:{pathName:'id'}},mDockingPathNode:{value:{pathName:'node'}},mBuiltWithRecipe:{value:{pathName:'old-recipe'}},mIsInLoadMode:{value:false}}};
 if(filter)station.properties.FilterItem={values:filter.map(pathName=>({pathName}))};
 return {levels:{main:{objects:[station,{instanceName:'id',properties:{mStationName:{value:{value:name}}}},{instanceName:'node',properties:{}},{instanceName:'station.inventory',properties:{mInventoryStacks:{values:stock.map(([item,count])=>({properties:{NumItems:{value:count},Item:{value:{itemReference:{pathName:item}}}}}))}}}]}}};
}
test('name resolver handles typos and separates the transported part from consumer names',()=>{
 assert.equal(itemFromName('Iron Ignots OUT Station 2'),iron);
 assert.match(itemFromName('Cooper Ignots OUT'),/CopperIngot/);
 assert.match(itemFromName('Iron Ore IN Station Iron Ignots'),/OreIron/);
 assert.match(itemFromName('IRON IN IGNOTS Station'),/OreIron/);
 assert.match(itemFromName('Plates IN Station Reinforced Plate'),/Parts\/IronPlate\//);
 assert.match(itemFromName('Reinforced IP IN Station Smart Plating'),/IronPlateReinforced/);
 assert.match(itemFromName('IronRods IN Station Screws'),/IronRod/);
 assert.equal(itemFromName('Truck Station'),null);
});
test('default names with one stocked cargo type are converted statically',()=>{
 const p=plan(fixture(null,[[iron,100]]),'hash');assert.equal(p.stations[0].item,iron);assert.equal(p.stations[0].mode,'UNLOAD');
 assert.equal(p.stations[0].source,'unique existing cargo');
});
test('empty unnamed and mixed cargo are preserved',()=>{
 assert.equal(plan(fixture(null),'hash').stations.length,0);
 assert.equal(plan(fixture(null,[[iron,1],[copper,1]]),'hash').stations.length,0);
});
test('conflicting name and stock is reported rather than replacing cargo',()=>{
 const p=plan(fixture('Iron Ingots IN',[[copper,100]]),'hash');assert.equal(p.stations.length,0);assert.match(p.skipped[0].reason,/disagree/);
});
test('existing operator filters win and are not inferred again',()=>{
 const p=plan(fixture('Iron Ingots IN',[],PROGRAMMABLE,[copper]),'hash');assert.equal(p.stations.length,1);assert.equal(p.stations[0].item,undefined);assert.equal(p.stations[0].recipe,FILN);
});
test('parking bays and the source structure remain untouched',()=>{
 const save=fixture('Iron Ingots OUT',[[iron,100]]);save.levels.main.objects[0].properties.mBuiltWithRecipe.value.pathName='/KDataForge/Gen/FILNRestStop.Gen_FILNRestStop_Recipe_RestStop';
 const before=structuredClone(save);assert.equal(plan(save,'hash').stations.length,0);assert.deepEqual(save,before);
});
