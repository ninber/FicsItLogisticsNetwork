#!/usr/bin/env node
// Offline conversion preserves actor/component identities, references and inventory.
const fs=require('fs'),path=require('path'),crypto=require('crypto'),assert=require('assert');
const {Parser}=require(process.env.FILN_SAVE_PARSER||'@etothepii/satisfactory-file-parser');
const [input,output,manifestFile,...flags]=process.argv.slice(2);
if(!input||!output||!manifestFile||flags.some(x=>x!=='--write'))throw Error('Usage: node replace-stations.cjs INPUT OUTPUT MANIFEST [--write]');
assert(path.resolve(input)!==path.resolve(output),'Never replace the input save');assert(!fs.existsSync(output),'Output already exists');
const bytes=fs.readFileSync(input),manifest=JSON.parse(fs.readFileSync(manifestFile));
assert.strictEqual(crypto.createHash('sha256').update(bytes).digest('hex'),manifest.sha256,'Input save changed');
assert(Array.isArray(manifest.stations)&&manifest.stations.length>0,'Explicit station selection required');
const save=Parser.ParseSave(path.basename(input,'.sav'),new Uint8Array(bytes).buffer);
const objects=Object.values(save.levels).flatMap(x=>x.objects),by=new Map(objects.map(o=>[o.instanceName,o]));
assert.strictEqual(by.size,objects.length,'Ambiguous object identities');
const ORIGINAL='/Game/FactoryGame/Buildable/Factory/TruckStation/Build_TruckStation.Build_TruckStation_C';
const TARGET='/TruckStationSettings/Build_ProgrammableTruckStation.Build_ProgrammableTruckStation_C';
const RECIPE='/TruckStationSettings/Recipe_ProgrammableTruckStation.Recipe_ProgrammableTruckStation_C';
const before=JSON.stringify(save.levels),changes=[];const seen=new Set();
for(const entry of manifest.stations){
 assert(!seen.has(entry.path),'Duplicate selection');seen.add(entry.path);
 const station=by.get(entry.path);assert(station&&[ORIGINAL,TARGET].includes(station.typePath),'Selection is not a supported truck station');
 const keepFilter=entry.item===undefined&&station.typePath===TARGET&&Array.isArray(station.properties.FilterItem?.values);
 assert(keepFilter||(typeof entry.item==='string'&&/^\/[\w/.:-]+$/.test(entry.item)),'Explicit item or existing programmable filter required');
 const newRecipe=entry.recipe||RECIPE;assert([RECIPE,'/KDataForge/Gen/FILNStations.Gen_FILNStations_Recipe_LogisticsStation'].includes(newRecipe),'Unsupported target recipe');
 const identifier=by.get(station.properties.mStationIdentifier?.value.pathName),node=by.get(station.properties.mDockingPathNode?.value.pathName);
 assert(identifier&&node,'Station identity/road node missing');
 assert(station.typePath===TARGET||!station.properties.FilterItem,'Unexpected preexisting programmable state');
 const old={typePath:station.typePath,recipe:structuredClone(station.properties.mBuiltWithRecipe),filter:structuredClone(station.properties.FilterItem)};
 station.typePath=TARGET;
 station.properties.mBuiltWithRecipe.value.pathName=newRecipe;
 if(!keepFilter)station.properties.FilterItem={type:'ArrayProperty',name:'FilterItem',propertyTagType:{name:'ArrayProperty',children:[{name:'ObjectProperty',children:[]}]},values:[{levelName:'',pathName:entry.item}]};
 changes.push({station,old,newRecipe,newFilter:structuredClone(station.properties.FilterItem),path:entry.path,item:entry.item,identifier:identifier.instanceName,node:node.instanceName,guid:node.properties.mPathNodeGUID?.value,
  incoming:node.properties.mArrivingConnections?.values.length||0,outgoing:node.properties.mLeavingConnections?.values.length||0});
}
const expected=JSON.stringify(save.levels);
// Demonstrate that undoing only the declared edits restores the entire parsed body.
for(const c of changes){c.station.typePath=c.old.typePath;c.station.properties.mBuiltWithRecipe=c.old.recipe;if(c.old.filter)c.station.properties.FilterItem=c.old.filter;else delete c.station.properties.FilterItem;}
assert.strictEqual(JSON.stringify(save.levels),before,'Unexpected body mutation');
for(const c of changes){c.station.typePath=TARGET;c.station.properties.mBuiltWithRecipe=structuredClone(c.old.recipe);c.station.properties.mBuiltWithRecipe.value.pathName=c.newRecipe;c.station.properties.FilterItem=structuredClone(c.newFilter);}
assert.strictEqual(JSON.stringify(save.levels),expected);
save.name=path.basename(output,'.sav');save.header.saveName=save.name;
const report={output_save_name:save.name,source:input,output,source_sha256:manifest.sha256,scope:'copy-only class/recipe/filter conversion; native reload verification required',objects:objects.length,
 changes:changes.map(({station,old,newFilter,...c})=>c),written:false};
if(flags.includes('--write')){
 const chunks=[];Parser.WriteSave(save,header=>chunks.push(Buffer.from(header)),chunk=>chunks.push(Buffer.from(chunk)));
 const encoded=Buffer.concat(chunks);const decoded=Parser.ParseSave('verification',new Uint8Array(encoded).buffer);
 assert.deepStrictEqual(decoded.levels,save.levels,'Save roundtrip altered the parsed body');
 fs.mkdirSync(path.dirname(output),{recursive:true});fs.writeFileSync(output,encoded,{flag:'wx'});report.written=true;report.sha256=crypto.createHash('sha256').update(encoded).digest('hex');
 fs.writeFileSync(output+'.report.json',JSON.stringify(report,null,2),{flag:'wx'});
}
console.log(JSON.stringify(report,null,2));
