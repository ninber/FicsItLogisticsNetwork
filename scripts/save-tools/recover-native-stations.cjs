// SPDX-License-Identifier: GPL-3.0-only
'use strict';
const assert = require('node:assert/strict');
const {cloneSave} = require('./clone-save.cjs');
const {guid} = require('./native-station-conversion.cjs');
const {ORDINARY, PROGRAMMABLE} = require('./station-migration.cjs');
const ROOT = '/FicsItLogisticsNetwork/Depot/';
const SUBSYSTEM = '/Script/FicsItLogisticsNetwork.LogisticsSubsystem';
const SUFFIXES = ['FILNDepot', 'FILNFluidDepot', 'FILNStation', 'FILNFluidStation'];
const asset = (kind, suffix) => ROOT + kind + '_' + suffix + '.' + kind + '_' + suffix + '_C';
const reference = (prop) => prop?.value?.pathName;
const objectsOf = (save) => Object.values(save.levels).flatMap(level => level.objects);
function validateAssets(receipt) {
 assert(receipt?.schema===4 && Array.isArray(receipt.buildings) && receipt.buildings.length===4,
  'Four actual author/reload-validated FILN buildable receipts required');
 const roles = new Map();
 for(const suffix of SUFFIXES) {
  const rows = receipt.buildings.filter(row => row.recipe===asset('Recipe',suffix));
  assert.equal(rows.length,1,'Missing or duplicate FILN recipe: '+suffix);
  const row=rows[0];
  assert.equal(row.actor,asset('Build',suffix),'Wrong FILN actor: '+suffix);
  assert.equal(row.descriptor,asset('Desc',suffix),'Wrong FILN descriptor: '+suffix);
  assert(typeof row.vanilla_actor==='string' && row.vanilla_actor.startsWith('/Game/FactoryGame/'),'Invalid vanilla actor');
  assert(typeof row.vanilla_recipe==='string' && row.vanilla_recipe.startsWith('/Game/FactoryGame/'),'Invalid vanilla recipe');
  roles.set(suffix,row);
 }
 assert.equal(roles.get('FILNStation').vanilla_actor,ORDINARY);
 for(const [depot,station] of [['FILNDepot','FILNStation'],['FILNFluidDepot','FILNFluidStation']]) {
  assert.equal(roles.get(depot).vanilla_actor,roles.get(station).vanilla_actor);
  assert.equal(roles.get(depot).vanilla_recipe,roles.get(station).vanilla_recipe);
 }
 assert.notEqual(roles.get('FILNStation').vanilla_actor,roles.get('FILNFluidStation').vanilla_actor);
 return roles;
}
function recover(save,receipt,history) {
 const roles=validateAssets(receipt);
 const source=objectsOf(save);
 const next=cloneSave(save), objects=objectsOf(next);
 const by=new Map(objects.map(o=>[o.instanceName,o]));
 assert.equal(by.size,objects.length,'Duplicate actor identity');
 const subsystems=objects.filter(o=>o.typePath===SUBSYSTEM);
 assert.equal(subsystems.length,1,'Expected one existing native FILN subsystem; use original legacy migration for other worlds');
 const configs=subsystems[0].properties?.StationConfigs?.values;
 assert(Array.isArray(configs),'Saved station config array not readable; refuse to discard resources');
 const byId=new Map();
 for(const entry of configs) {
  const p=entry.properties, id=reference(p?.Station), item=reference(p?.Resource);
  assert(id && by.has(id) && item && !byId.has(id),'Invalid or duplicate saved station configuration');
  assert(Array.isArray(p.OriginalFilters?.values),'Missing saved original slot layout: '+id);
  assert(!p.ExternalFilter?.value,'Unowned external filter: '+id);
  byId.set(id,item);
 }
 const byGuid=new Map(), stations=new Map(), changes=[], represented=new Set();
 const solid=roles.get('FILNStation'), fluid=roles.get('FILNFluidStation');
 for(const o of objects) {
  const oldClass=o.typePath, oldRecipe=reference(o.properties?.mBuiltWithRecipe);
  assert(oldClass!==PROGRAMMABLE,'Legacy station still present; cannot discard its filter: '+o.instanceName);
  let role;
  for(const [suffix,row] of roles) if(oldClass===row.actor || oldRecipe===row.recipe) {
   assert(!role || role===suffix,'Conflicting FILN class and recipe: '+o.instanceName);
   role=suffix;
  }
  if(!role) {
   if(oldClass===solid.vanilla_actor) role='FILNStation';
   else if(oldClass===fluid.vanilla_actor) role='FILNFluidStation';
   else continue;
  }
  const target=roles.get(role), p=o.properties;
  assert([target.actor,target.vanilla_actor].includes(oldClass),'Wrong source station medium/class: '+o.instanceName);
  assert([target.recipe,target.vanilla_recipe].includes(oldRecipe),'Unexpected source station recipe: '+o.instanceName);
  assert(p.FilterItem===undefined,'Unmigrated legacy filter: '+o.instanceName);
  const id=reference(p.mStationIdentifier), node=by.get(reference(p.mDockingPathNode));
  assert(id && by.has(id) && node && !represented.has(id),'Invalid or duplicate station identity: '+o.instanceName);
  const key=guid(node.properties?.mPathNodeGUID?.value);
  assert(!byGuid.has(key),'Duplicate station road GUID: '+key);
  represented.add(id);
  const resource=byId.get(id)||null;
  assert(!role.endsWith('Depot') || resource===null,'Depot has logistics filter: '+o.instanceName);
  const info={actor:o.instanceName,id,guid:key,resource,role};
  byGuid.set(key,info);stations.set(o.instanceName,info);
  // Preserve the registry and every inventory/component/connection byte in the
  // parsed body. Existing native repair handles only owned old slot restrictions.
  if(oldClass!==target.actor || oldRecipe!==target.recipe) {
   changes.push({...info,oldClass,oldRecipe,newClass:target.actor,newRecipe:target.recipe});
   o.typePath=target.actor;
   p.mBuiltWithRecipe.value.pathName=target.recipe;
  }
 }
 assert(stations.size>0,'No truck stations found');
 for(const id of byId.keys()) assert(represented.has(id),'Saved resource configuration has no matching station: '+id);
 // A historical manifest is audit evidence, never a source of guessed demand.
 // A deleted station may be absent; identity or resource mismatches must block.
 const missing=[];
 if(history) {
  const rows=history.station_records||history.records;
  assert(Array.isArray(rows),'Invalid historical report');
  const historicalActors=new Set(), historicalGuids=new Set();
  for(const row of rows.filter(x=>x.resource)) {
   assert(typeof row.actor==='string' && /^[0-9a-f]{32}$/.test(row.road_guid),'Invalid historical station identity');
   assert(!historicalActors.has(row.actor) && !historicalGuids.has(row.road_guid),'Duplicate historical station identity');
   historicalActors.add(row.actor);historicalGuids.add(row.road_guid);
   const actor=stations.get(row.actor), road=byGuid.get(row.road_guid);
   if(!actor && !road) {missing.push(row.actor);continue;}
   assert(actor && road && actor===road,'Historical actor/road identity mismatch: '+row.actor);
   assert.equal(actor.resource,row.resource,'Historical filter missing or changed: '+row.actor);
  }
 }
 const undo=cloneSave(next),undoBy=new Map(objectsOf(undo).map(o=>[o.instanceName,o]));
 for(const c of changes) {
  const o=undoBy.get(c.actor);o.typePath=c.oldClass;o.properties.mBuiltWithRecipe.value.pathName=c.oldRecipe;
 }
 assert.deepEqual(undo.levels,save.levels,'Migration changed more than station class and recipe');
 assert.equal(source.length,objects.length,'Object count changed');
 return {save:next,changes,stations:[...stations.values()],historicalMissing:missing};
}
module.exports={recover,validateAssets,asset,SUBSYSTEM};
