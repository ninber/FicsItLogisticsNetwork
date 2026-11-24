#!/usr/bin/env node
const fs=require('fs'),path=require('path'),crypto=require('crypto'),assert=require('assert');
const {Parser}=require(process.env.FILN_SAVE_PARSER||'@etothepii/satisfactory-file-parser');const {plan}=require('./station-migration.cjs');
const [input,output]=process.argv.slice(2);assert(input&&output,'Usage: plan-city.cjs INPUT.sav MANIFEST.json');assert(!fs.existsSync(output),'Manifest already exists');
const bytes=fs.readFileSync(input),save=Parser.ParseSave(path.basename(input,'.sav'),new Uint8Array(bytes).buffer);const result=plan(save,crypto.createHash('sha256').update(bytes).digest('hex'));
fs.mkdirSync(path.dirname(output),{recursive:true});fs.writeFileSync(output,JSON.stringify(result,null,2),{flag:'wx'});
console.log(JSON.stringify({selected:result.stations.length,skipped:result.skipped.length,manifest:output}));
for(const entry of result.stations)console.log(entry.mode+' '+entry.name+' -> '+(entry.item||'(existing filter)')+' ['+entry.source+']');
for(const entry of result.skipped)console.log('SKIP '+entry.name+': '+entry.reason);
