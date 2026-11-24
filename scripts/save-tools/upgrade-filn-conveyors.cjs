#!/usr/bin/env node
// Offline FILN-copy conveyor upgrade; never rename actors or rewrite their connections.
const fs=require('fs'),path=require('path'),assert=require('assert'),crypto=require('crypto');
const {Parser}=require('@etothepii/satisfactory-file-parser');
const {write}=require('./filn-blueprints.cjs');
const part=id=>`/Game/FactoryGame/Resource/Parts/${id}/Desc_${id}.Desc_${id}_C`;
const materials={1:part('IronPlate'),2:part('IronPlateReinforced'),3:part('SteelPlate'),4:part('SteelPlateReinforced')};
function conveyor(o){return /^\/Game\/FactoryGame\/Buildable\/Factory\/Conveyor(Belt|Lift)Mk([1-6])\/Build_Conveyor\1Mk\2\.Build_Conveyor\1Mk\2_C$/.exec(o.typePath||'');}
function length(o,kind){
 if(kind==='Lift')return Math.abs(o.properties.mTopTransform.value.properties.Translation.value.z);
 const points=o.properties.mSplineData.values.map(v=>v.properties);
 // Integrate the cubic Hermite derivative using five-point Gaussian quadrature.
 const nodes=[0,.5384693101056831,-.5384693101056831,.906179845938664,-.906179845938664];
 const weights=[.5688888888888889,.4786286704993665,.4786286704993665,.2369268850561891,.2369268850561891];
 assert(points.length>=2,'Missing belt spline');let total=0;
 for(let i=1;i<points.length;i++)for(let j=0;j<5;j++){
  const a=points[i-1],b=points[i],t=(nodes[j]+1)/2;
  const velocity=['x','y','z'].map(k=>(6*t*t-6*t)*a.Location.value[k]+(3*t*t-4*t+1)*a.LeaveTangent.value[k]+(-6*t*t+6*t)*b.Location.value[k]+(3*t*t-2*t)*b.ArriveTangent.value[k]);
  total+=weights[j]/2*Math.hypot(...velocity);
 }
 return total;
}
function upgrade(source){
 assert(source.name.startsWith('FILN - '),'Only FILN copies may be upgraded');
 const bp=structuredClone(source),edits=[],counts={belts:0,lifts:0,already_mk4:0},deltas=new Map();
 for(let i=0;i<bp.objects.length;i++){
  const o=bp.objects[i],match=conveyor(o);if(!match)continue;
  const kind=match[1],tier=Number(match[2]);if(tier>=4){if(tier===4)counts.already_mk4++;continue;}
  const before=structuredClone(o),recipe=o.properties.mBuiltWithRecipe?.value;
  assert(recipe?.pathName===`/Game/FactoryGame/Recipes/Buildings/Recipe_Conveyor${kind}Mk${tier}.Recipe_Conveyor${kind}Mk${tier}_C`,'Unexpected conveyor recipe');
  assert(!o.properties.mSpeed,'Explicit conveyor speed override needs separate migration');
  const cm=length(o,kind);assert(Number.isFinite(cm)&&cm>=0,'Invalid conveyor length');
  // Avoid treating sub-millimetre save precision noise as another two-metre section.
  const units=Math.max(1,Math.ceil(cm/200-0.00001))*(kind==='Lift'?2:1);
  deltas.set(materials[tier],(deltas.get(materials[tier])||0)-units);
  deltas.set(materials[4],(deltas.get(materials[4])||0)+units);
  o.typePath=o.typePath.replaceAll(`Mk${tier}`, 'Mk4');recipe.pathName=recipe.pathName.replaceAll(`Mk${tier}`,'Mk4');
  counts[kind==='Belt'?'belts':'lifts']++;edits.push({index:i,before,kind,tier,units});
 }
 if(edits.length){
  for(const [item,delta]of deltas){
   let entry=bp.header.itemCosts.find(e=>e[0].pathName===item);
   if(!entry){assert(delta>=0,'Original construction cost missing');entry=[{levelName:'',pathName:item},0];bp.header.itemCosts.push(entry);}
   entry[1]+=delta;assert(Number.isInteger(entry[1])&&entry[1]>=0,'Construction cost underflow');
  }
  bp.header.itemCosts=bp.header.itemCosts.filter(e=>e[1]!==0);
  for(const ref of bp.header.recipeReferences)ref.pathName=ref.pathName.replace(/Recipe_Conveyor(Belt|Lift)Mk[1-3]/g,'Recipe_Conveyor$1Mk4');
  const seen=new Set();bp.header.recipeReferences=bp.header.recipeReferences.filter(r=>{const k=JSON.stringify(r);if(seen.has(k))return false;seen.add(k);return true;});
 }
 // Prove every actor property, transform and connection survives except declared class/recipe edits.
 const restored=structuredClone(bp.objects);for(const e of edits)restored[e.index]=e.before;
 assert.deepStrictEqual(restored,structuredClone(source.objects));assert.deepStrictEqual(bp.config,structuredClone(source.config));
 return {bp,counts,edits};
}
const sha=b=>crypto.createHash('sha256').update(b).digest('hex');
function main(){
 const [input,output]=process.argv.slice(2);assert(input&&output,'Usage: upgrade-filn-conveyors.cjs INPUT_DIR STAGING_DIR');assert(path.resolve(input)!==path.resolve(output),'Use a separate staging directory');fs.mkdirSync(output,{recursive:true});
 const entries=[];
 for(const file of fs.readdirSync(input).filter(n=>n.startsWith('FILN - ')&&n.endsWith('.sbp')).sort()){
  const main=fs.readFileSync(path.join(input,file)),config=fs.readFileSync(path.join(input,file+'cfg')),name=file.slice(0,-4);
  const source=Parser.ParseBlueprintFiles(name,Uint8Array.from(main).buffer,Uint8Array.from(config).buffer),r=upgrade(source);
  let bytes=main;if(r.edits.length){const encoded=write(r.bp);bytes=encoded.main;const decoded=Parser.ParseBlueprintFiles(name,Uint8Array.from(bytes).buffer,Uint8Array.from(config).buffer);assert.deepStrictEqual(decoded.config,source.config);}
  fs.writeFileSync(path.join(output,file),bytes,{flag:'wx'});fs.writeFileSync(path.join(output,file+'cfg'),config,{flag:'wx'});
  entries.push({name,...r.counts,source_sha256:sha(main),output_sha256:sha(bytes),config_sha256:sha(config),edits:r.edits.map(({index,kind,tier,units})=>({index,kind,tier,units}))});
 }
 assert(entries.length,'No FILN blueprints found');
 fs.writeFileSync(path.join(output,'upgrade-manifest.json'),JSON.stringify({input,entries,cost_note:'Geometry-scaled cost delta; existing unrelated header costs preserved. Native placement quote requires manual verification.'},null,2)+'\n',{flag:'wx'});
 console.log(JSON.stringify({blueprints:entries.length,changed:entries.filter(e=>e.belts+e.lifts).length,belts:entries.reduce((n,e)=>n+e.belts,0),lifts:entries.reduce((n,e)=>n+e.lifts,0)}));
}
if(require.main===module)main();module.exports={upgrade,length,conveyor};
