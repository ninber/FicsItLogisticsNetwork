// SPDX-License-Identifier: GPL-3.0-only
'use strict';
const assert=require('node:assert/strict');
const {PROGRAMMABLE,FILN}=require('./station-migration.cjs');
const {REST,PROGRAM_RECIPE}=require('./native-station-conversion.cjs');
const MANAGERS=[
 {type:'/Script/FactoryGame.FGRecipeManager',name:'recipeManager',property:'mAvailableRecipes',paths:[REST,PROGRAM_RECIPE,FILN]},
 {type:'/Game/FactoryGame/Schematics/Progression/BP_SchematicManager.BP_SchematicManager_C',name:'schematicManager',property:'mPurchasedSchematics',paths:[
 '/KDataForge/Gen/FILNRestStop.Gen_FILNRestStop_Schematic_RestStop',
 '/TruckStationSettings/Schematic_SmarterTrucks.Schematic_SmarterTrucks_C',
 '/KDataForge/Gen/FILNStations.Gen_FILNStations_Schematic_LogisticsStation']}
];
const legacy=/\/(?:KDataForge\/Gen\/FILN|TruckStationSettings\/)/;
// Walk parsed values instead of JSON.stringify: save stats may contain BigInt.
function references(value,path=[],out=[]) {
 if(typeof value==='string' && legacy.test(value)) out.push({path:[...path],value});
 else if(value && typeof value==='object') {
  if(ArrayBuffer.isView(value)||value instanceof ArrayBuffer) return out;
  if(value instanceof Map) {let i=0;for(const [k,v] of value) {references(k,[...path,'map-key',i],out);references(v,[...path,'map-value',i++],out);}}
  else for(const [k,v] of Object.entries(value)) {
   if(legacy.test(k))out.push({path:[...path,k],value:k});
   references(v,[...path,k],out);
  }
 }
 return out;
}
function cleanupUnlocks(save) {
 const next=structuredClone(save);
 const objects=Object.values(next.levels).flatMap(l=>l.objects);
 const edits=[],undo=[];
 for(const spec of MANAGERS) {
  const matches=objects.filter(o=>o.typePath===spec.type&&o.instanceName==='Persistent_Level:PersistentLevel.'+spec.name);
  assert.equal(matches.length,1,'Expected exactly one native '+spec.name);
  const owner=matches[0],prop=owner.properties?.[spec.property];
  assert(prop?.type==='ArrayProperty'&&Array.isArray(prop.values)&&prop.values.every(v=>v&&typeof v.pathName==='string'&&typeof v.levelName==='string'),'Unexpected native unlock array: '+spec.property);
  const allowed=new Set(spec.paths),removed=[];
  const kept=prop.values.filter((entry,index)=>{
   if(!allowed.has(entry.pathName))return true;
   assert.equal(entry.levelName,'','Legacy unlock is not an asset reference');
   removed.push({index,ref:structuredClone(entry)});return false;
  });
  if(removed.length) {
   undo.push({prop,values:prop.values}); prop.values=kept;
   edits.push({manager:spec.name,property:spec.property,removed,before:kept.length+removed.length,after:kept.length});
  }
 }
 // No unlock deletion while selected conversion is incomplete or another object
 // still references one of the exact six unlock assets.
 const all=new Set(MANAGERS.flatMap(m=>m.paths));
 const residual=references(next.levels);
 assert(!residual.some(r=>all.has(r.value)||r.value===PROGRAMMABLE&&r.path.at(-1)==='typePath'),
  'Legacy asset still used by a serialized object; finish station conversion first');
 const restored=structuredClone(next);
 const restoredObjects=Object.values(restored.levels).flatMap(l=>l.objects);
 for(const edit of edits) {
  const prop=restoredObjects.find(o=>o.instanceName==='Persistent_Level:PersistentLevel.'+edit.manager).properties[edit.property];
  for(const row of edit.removed)prop.values.splice(row.index,0,row.ref);
 }
 assert.deepEqual(restored.levels,save.levels,'Unexpected unlock cleanup mutation');
 return {save:next,edits,residual,dependencyFree:false};
}
module.exports={cleanupUnlocks,references,MANAGERS};
