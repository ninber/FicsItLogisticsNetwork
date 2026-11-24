#!/usr/bin/env node
const fs=require('fs'),path=require('path'),assert=require('assert'),crypto=require('crypto');
const {Parser}=require('@etothepii/satisfactory-file-parser');
const ORDINARY='/Game/FactoryGame/Buildable/Factory/TruckStation/Build_TruckStation.Build_TruckStation_C';
const TARGET='/TruckStationSettings/Build_ProgrammableTruckStation.Build_ProgrammableTruckStation_C';
const LOGISTICS='/KDataForge/Gen/FILNStations.Gen_FILNStations_Recipe_LogisticsStation';
const DEPOT='/KDataForge/Gen/FILNRestStop.Gen_FILNRestStop_Recipe_RestStop';
const STATION_RECIPE='/Game/FactoryGame/Recipes/Buildings/Recipe_TruckStation.Recipe_TruckStation_C';
function group(name){if(/rest depot/i.test(name))return 'Depots';if(/^road/i.test(name))return 'Truck Stations';if(/smelt/i.test(name))return 'Smelting';if(/constructor/i.test(name))return 'Constructors';if(/assembler/i.test(name))return 'Assemblers';return 'Factory Modules';}
function convert(source){
 const bp=structuredClone(source),before=structuredClone(source.objects);const stations=bp.objects.filter(o=>o.typePath===ORDINARY||o.typePath===TARGET);
 assert(stations.length,'No stations to convert');const depot=/rest depot/i.test(source.name);const recipe=depot?DEPOT:LOGISTICS;const edits=[];
 for(const station of stations){
  assert(station.properties.mBuiltWithRecipe?.value,'Missing original recipe');
  const original=structuredClone(station);station.typePath=depot?ORDINARY:TARGET;
  station.properties.mBuiltWithRecipe.value.pathName=recipe;
  if(!depot&&!station.properties.FilterItem)station.properties.FilterItem={type:'ArrayProperty',name:'FilterItem',propertyTagType:{name:'ArrayProperty',children:[{name:'ObjectProperty',children:[]}]},values:[]};
  edits.push({object:station,original});
 }
 const expected=structuredClone(bp.objects);
 for(const {object,original}of edits){for(const key of Object.keys(object))delete object[key];Object.assign(object,structuredClone(original));}
 assert.deepStrictEqual(bp.objects,before,'Unexpected changes outside declared station class/recipe/filter edits');
 bp.objects=expected;bp.name='FILN - '+source.name;
 // Every truck station in this copy uses the FILN recipe; preserve all other requirements/costs.
 for(const ref of bp.header.recipeReferences)if(ref.pathName===STATION_RECIPE)ref.pathName=recipe;
 return {bp,stations:stations.length,category:group(source.name),depot};
}
function write(bp){const chunks=[];const summary=Parser.WriteBlueprintFiles(bp,h=>chunks.push(Buffer.from(h)),c=>chunks.push(Buffer.from(c)));const main=Buffer.concat(chunks),config=Buffer.from(summary.configFileBinary);
 const decoded=Parser.ParseBlueprintFiles(bp.name,new Uint8Array(main).buffer,new Uint8Array(config).buffer);
 assert.deepStrictEqual(JSON.parse(JSON.stringify(decoded.objects)),JSON.parse(JSON.stringify(bp.objects)),'Blueprint object roundtrip mismatch');assert.deepStrictEqual(decoded.config,bp.config,'Blueprint config roundtrip mismatch');assert.deepStrictEqual(decoded.header,bp.header,'Blueprint header roundtrip mismatch');return {main,config};}
function main(){const[input,output]=process.argv.slice(2);assert(input&&output,'Usage: filn-blueprints.cjs SOURCE_DIR OUTPUT_DIR');assert(path.resolve(input)!==path.resolve(output),'Copies only');fs.mkdirSync(output,{recursive:true});const entries=[];
 for(const file of fs.readdirSync(input).filter(n=>n.endsWith('.sbp')&&!n.startsWith('FILN - ')).sort()){
  const stem=file.slice(0,-4);const main=fs.readFileSync(path.join(input,file)),cfg=fs.readFileSync(path.join(input,file+'cfg'));
  const source=Parser.ParseBlueprintFiles(stem,new Uint8Array(main).buffer,new Uint8Array(cfg).buffer);
  if(!source.objects.some(o=>o.typePath===ORDINARY||o.typePath===TARGET))continue;
  const result=convert(source);const encoded=write(result.bp);
  for(const[suffix,bytes]of [['.sbp',encoded.main],['.sbpcfg',encoded.config]])fs.writeFileSync(path.join(output,result.bp.name+suffix),bytes,{flag:'wx'});
  entries.push({original:stem,name:result.bp.name,category:result.category,stations:result.stations,depot:result.depot,source_sha256:crypto.createHash('sha256').update(main).digest('hex'),source_config_sha256:crypto.createHash('sha256').update(cfg).digest('hex'),filters:'existing preserved; generic stations intentionally blank'});
 }
 fs.writeFileSync(path.join(output,'manifest.json'),JSON.stringify({category:'FILN',entries},null,2)+'\n',{flag:'wx'});
 fs.writeFileSync(path.join(output,'blueprint-categories.tsv'),entries.map(e=>e.category+'\t'+e.name).join('\n')+'\n',{flag:'wx'});
 console.log(JSON.stringify({copies:entries.length,stations:entries.reduce((a,e)=>a+e.stations,0),groups:[...new Set(entries.map(e=>e.category))]}));
}
if(require.main===module)main();module.exports={convert,group,write};
