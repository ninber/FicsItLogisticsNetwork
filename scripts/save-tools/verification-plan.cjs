#!/usr/bin/env node
const fs=require('fs'),assert=require('assert');
const {Parser}=require(process.env.FILN_SAVE_PARSER||'@etothepii/satisfactory-file-parser');
const [input,reportPath,output]=process.argv.slice(2);assert(input&&reportPath&&output,'Usage: verification-plan.cjs SAVE REPORT TSV');
const bytes=fs.readFileSync(input),save=Parser.ParseSave('verify',new Uint8Array(bytes).buffer);
const objects=Object.values(save.levels).flatMap(l=>l.objects),by=new Map(objects.map(o=>[o.instanceName,o]));
const report=JSON.parse(fs.readFileSync(reportPath)),rows=['FILN_MIGRATION_V1'];
for(const change of report.changes){
 const station=by.get(change.path);assert(station,'converted station missing');
 const inventory=station.components.find(x=>/\.inventory$/i.test(x.pathName));assert(inventory,'cargo inventory reference missing');
 const guid=change.guid.map(x=>(x>>>0).toString(16).padStart(8,'0')).join('');
 const filters=station.properties.FilterItem.values.map(x=>x.pathName).filter(Boolean);
 rows.push([guid,station.properties.mIsInLoadMode?.value===false?'UNLOAD':'LOAD',filters.join('|'),inventory.pathName,change.node,change.incoming,change.outgoing].join('\t'));
}
fs.writeFileSync(output,rows.join('\n')+'\n',{flag:'wx'});console.log('Verification rows:',report.changes.length);
