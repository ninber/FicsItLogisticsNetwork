// SPDX-License-Identifier: GPL-3.0-only
'use strict';
const assert=require('node:assert/strict');
const {isDeepStrictEqual}=require('node:util');
const {cloneSave}=require('./clone-save.cjs');
const {references}=require('./legacy-references.cjs');
const {ORDINARY,PROGRAMMABLE,FILN,cargoItems}=require('./station-migration.cjs');
const VANILLA='/Game/FactoryGame/Recipes/Buildings/Recipe_TruckStation.Recipe_TruckStation_C';
const DEPOT_CLASS='/FicsItLogisticsNetwork/Depot/Build_FILNDepot.Build_FILNDepot_C';
const DEPOT_RECIPE='/FicsItLogisticsNetwork/Depot/Recipe_FILNDepot.Recipe_FILNDepot_C';
const REST='/KDataForge/Gen/FILNRestStop.Gen_FILNRestStop_Recipe_RestStop';
const PROGRAM_RECIPE='/TruckStationSettings/Recipe_ProgrammableTruckStation.Recipe_ProgrammableTruckStation_C';
const resource=/^\/Game\/FactoryGame\/Resource\/[\w/.]+_C$/;
function guid(value) {
 assert(Array.isArray(value)&&value.length===4&&value.every(v=>Number.isInteger(v)&&v>=-2147483648&&v<=4294967295),'Missing/invalid road GUID');
 const key=value.map(v=>(v>>>0).toString(16).padStart(8,'0')).join('');
 assert(!/^0+$/.test(key),'Zero road GUID');return key;
}
function convert(save,selection) {
 assert(Array.isArray(selection)&&selection.length>0&&selection.length<=512,'Explicit selection of 1..512 stations required');
 // Never mutate source, including when a later row fails validation.
 const next=cloneSave(save);
 const objects=Object.values(next.levels).flatMap(l=>l.objects),by=new Map(objects.map(o=>[o.instanceName,o]));
 assert.equal(by.size,objects.length,'Ambiguous object identities');
 const seen=new Set(),nodes=new Set(),rows=['FILN_NATIVE_STATIONS_V1'],changes=[];
 for(const path of selection) {
  assert(typeof path==='string'&&!seen.has(path),'Invalid/duplicate station selection');seen.add(path);
  const station=by.get(path);assert(station,'Station missing');
  const props=station.properties,recipe=props.mBuiltWithRecipe?.value?.pathName;
  const depot=station.typePath===ORDINARY&&recipe===REST;
  assert(depot||(station.typePath===PROGRAMMABLE&&[FILN,PROGRAM_RECIPE].includes(recipe)),'Unsupported legacy class/recipe');
  // R16: the old converter destroys the player-visible selector by replacing
  // logistics stations with vanilla actors. Do not emit another such save.
  // Remove this gate only when the dedicated FILN logistics actor/recipe and
  // lossless, runtime-verified resource restoration are implemented.
  assert(depot,'R16 BLOCKED: dedicated FILN logistics station and lossless FILTERS migration are required; refusing vanilla downgrade');
  const id=by.get(props.mStationIdentifier?.value?.pathName),node=by.get(props.mDockingPathNode?.value?.pathName);
  assert(id&&node,'Missing station identifier/road node');
  const key=guid(node.properties.mPathNodeGUID?.value);
  assert(!nodes.has(key),'Duplicate selected node GUID');nodes.add(key);
  const mode=props.mIsInLoadMode?.value===false?'UNLOAD':'LOAD';
  const stock=cargoItems(by,station);assert(stock,'Cargo inventory cannot be verified');
  assert(!props.mDockedActor?.value?.pathName,'Docked station is not eligible for offline conversion');
  let item='-',role=depot?'depot':'station';
  if(depot) {
   assert(mode==='LOAD'&&stock.length===0&&!props.FilterItem,'Depot cargo/mode/filter conflict');
  } else {
   const filter=props.FilterItem?.values;
   if(props.FilterItem===undefined || (Array.isArray(filter)&&filter.length===0)) {
    // Blank programmable station templates are present in real cities. Convert
    // to vanilla, not to a guessed FILN requester. Preserve even mixed cargo.
    role='ordinary';
   } else {
    assert(Array.isArray(filter)&&filter.length===1&&resource.test(filter[0]?.pathName||''),'Exactly one explicit vanilla resource or an absent/empty filter required');
    item=filter[0].pathName;
    assert(stock.every(x=>x.item===item),'Cargo differs from explicit legacy filter');
   }
  }
  changes.push({path,oldClass:station.typePath,oldRecipe:recipe,oldFilter:cloneSave(props.FilterItem),identifier:id.instanceName,node:node.instanceName,guid:key,mode,item,depot,role});
  station.typePath=depot?DEPOT_CLASS:ORDINARY;
  props.mBuiltWithRecipe.value.pathName=depot?DEPOT_RECIPE:VANILLA;
  delete props.FilterItem;
  Object.assign(changes.at(-1),{newClass:station.typePath,newRecipe:props.mBuiltWithRecipe.value.pathName,configurationPending:role==='station'});
  rows.push([role,key,item,mode].join('\t'));
 }
 // Invert precisely the three declared edits, and compare the full parsed body.
 const restored=cloneSave(next);
 const undo=new Map(Object.values(restored.levels).flatMap(l=>l.objects).map(o=>[o.instanceName,o]));
 for(const c of changes) {
  const o=undo.get(c.path);o.typePath=c.oldClass;o.properties.mBuiltWithRecipe.value.pathName=c.oldRecipe;
  if(c.oldFilter!==undefined)o.properties.FilterItem=c.oldFilter;
 }
 assert(isDeepStrictEqual(restored.levels,save.levels),'Unexpected body changes');
 // Do not certify removal of dependencies: schematic/descriptor/custom component
 // references elsewhere must be reviewed, not blindly stripped.
 const residual=objects.filter(o=>references(o).length>0).map(o=>o.instanceName);
 assert(Buffer.byteLength(rows.join('\n')+'\n','utf8')<=262144,'Manifest exceeds native import size limit');
 return {save:next,manifest:rows.join('\n')+'\n',changes,residual};
}
function validateDepotReceipt(receipt) {
 assert(receipt?.schema===4&&Array.isArray(receipt.buildings),'Schema-4 author/reload receipt required for named depot conversion');
 const matches=receipt.buildings.filter(b=>b.recipe===DEPOT_RECIPE);
 assert(matches.length===1&&matches[0].actor===DEPOT_CLASS&&matches[0].vanilla_actor===ORDINARY&&matches[0].vanilla_recipe===VANILLA,'Authored depot actor/recipe or vanilla parent differs from conversion contract');
}
module.exports={convert,guid,VANILLA,REST,PROGRAM_RECIPE,DEPOT_CLASS,DEPOT_RECIPE,validateDepotReceipt};
