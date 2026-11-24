#!/usr/bin/env node
// Offline category metadata only. No UE4SS struct-array writes or live descriptor refresh.
const fs=require('fs'),path=require('path'),assert=require('assert'),crypto=require('crypto');
const {Parser}=require('@etothepii/satisfactory-file-parser');
const ORDER=['Smelting','Constructors','Assemblers','Factory Modules','Truck Stations','Depots'];
function organize(property,manifest){
 const result=structuredClone(property),managed=new Map(manifest.entries.map(e=>[e.name,e.category]));
 assert(managed.size===manifest.entries.length&&manifest.entries.every(e=>e.name.startsWith('FILN - ')&&ORDER.includes(e.category)),'Invalid copy manifest');
 const template=result.values[0],subTemplate=result.values.flatMap(c=>c.properties.SubCategoryRecords.values)[0];assert(template&&subTemplate,'Missing native category templates');
 for(const cat of result.values)for(const sub of cat.properties.SubCategoryRecords.values)sub.properties.BlueprintNames.values=sub.properties.BlueprintNames.values.filter(name=>!managed.has(name));
 let filn=result.values.find(c=>c.properties.CategoryName.value==='FILN');
 if(!filn){filn=structuredClone(template);filn.properties.CategoryName.value='FILN';filn.properties.IconID.value=4;filn.properties.IsUndefined.value=false;filn.properties.MenuPriority.value=result.values.length;filn.properties.SubCategoryRecords.values=[];result.values.push(filn);}
 for(const[group,index]of ORDER.map((g,i)=>[g,i])){
  const names=manifest.entries.filter(e=>e.category===group).map(e=>e.name).sort();if(!names.length)continue;
  let sub=filn.properties.SubCategoryRecords.values.find(s=>s.properties.SubCategoryName.value===group);
  if(!sub){sub=structuredClone(subTemplate);sub.properties.SubCategoryName.value=group;sub.properties.MenuPriority.value=filn.properties.SubCategoryRecords.values.length;sub.properties.IsUndefined.value=typeof sub.properties.IsUndefined.value==='object'?{...sub.properties.IsUndefined.value,value:0}:0;sub.properties.BlueprintNames.values=[];filn.properties.SubCategoryRecords.values.push(sub);}
  sub.properties.MenuPriority.value=filn.properties.SubCategoryRecords.values.indexOf(sub);
  sub.properties.BlueprintNames.values.push(...names);
 }
 return result;
}
function main(){const[input,output,manifestPath]=process.argv.slice(2);assert(input&&output&&manifestPath,'Usage: organize-blueprint-categories.cjs AUTOSAVE OUTPUT MANIFEST');assert(path.resolve(input)!==path.resolve(output),'Never edit input save');assert(!fs.existsSync(output),'Output exists');
 const bytes=fs.readFileSync(input),save=Parser.ParseSave(path.basename(input,'.sav'),new Uint8Array(bytes).buffer),manifest=JSON.parse(fs.readFileSync(manifestPath));
 const matches=Object.values(save.levels).flatMap(l=>l.objects).filter(o=>o.properties?.mBlueprintCategoryRecords);assert.equal(matches.length,1,'Ambiguous category subsystem');
 const object=matches[0],before=JSON.stringify(save.levels),original=object.properties.mBlueprintCategoryRecords;
 object.properties.mBlueprintCategoryRecords=organize(original,manifest);const expected=JSON.stringify(save.levels);
 object.properties.mBlueprintCategoryRecords=original;assert.equal(JSON.stringify(save.levels),before,'Unexpected non-category mutation');
 object.properties.mBlueprintCategoryRecords=organize(original,manifest);assert.equal(JSON.stringify(save.levels),expected);
 const chunks=[];Parser.WriteSave(save,h=>chunks.push(Buffer.from(h)),c=>chunks.push(Buffer.from(c)));const encoded=Buffer.concat(chunks);
 const check=Parser.ParseSave(save.name,new Uint8Array(encoded).buffer);assert(require('util').isDeepStrictEqual(JSON.parse(JSON.stringify(check.levels)),JSON.parse(expected)),'Autosave roundtrip changed body data');
 fs.mkdirSync(path.dirname(output),{recursive:true});fs.writeFileSync(output,encoded,{flag:'wx'});
 fs.writeFileSync(output+'.report.json',JSON.stringify({source:input,output,source_sha256:crypto.createHash('sha256').update(bytes).digest('hex'),sha256:crypto.createHash('sha256').update(encoded).digest('hex'),scope:'only mBlueprintCategoryRecords changed; all other save-body data preserved',copies:manifest.entries.length,groups:ORDER},null,2)+'\n',{flag:'wx'});
 console.log('Category-only autosave copy verified:',output);
}
if(require.main===module)main();module.exports={organize};
