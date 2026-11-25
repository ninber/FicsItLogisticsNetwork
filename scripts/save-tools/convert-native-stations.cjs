#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-only
'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const {Parser}=require(process.env.FILN_SAVE_PARSER||'@etothepii/satisfactory-file-parser');
const {convert,validateDepotReceipt}=require('./native-station-conversion.cjs');
const {cleanupUnlocks}=require('./legacy-unlock-cleanup.cjs');
const [input,output,selectionFile,...flags]=process.argv.slice(2);
assert(input&&output&&selectionFile&&flags.every(x=>['--write','--write-preparation','--cleanup-unlocks'].includes(x)||x.startsWith('--asset-receipt=')),'Usage: convert-native-stations.cjs INPUT OUTPUT SELECTION.json [--write | --write-preparation] [--asset-receipt=RELOAD.json] [--cleanup-unlocks]');
assert(!(flags.includes('--write')&&flags.includes('--write-preparation')),'Choose one output mode');
const receiptFlags=flags.filter(x=>x.startsWith('--asset-receipt='));
assert(receiptFlags.length<=1,'Multiple asset receipts');
const src=fs.realpathSync(input),dir=fs.realpathSync(path.dirname(output)),dst=path.join(dir,path.basename(output));
assert(src!==dst&&!fs.existsSync(dst),'Never overwrite source/existing save');
assert(!dir.split(path.sep).some(x=>x.toLowerCase()==='savegames'),'Output must be outside live SaveGames');
const bytes=fs.readFileSync(src),selection=JSON.parse(fs.readFileSync(selectionFile));
const sha=crypto.createHash('sha256').update(bytes).digest('hex');
assert.equal(selection.sha256,sha,'Input save hash mismatch');
const parsed=Parser.ParseSave(path.basename(src,'.sav'),new Uint8Array(bytes).buffer);
const converted=convert(parsed,selection.stations);
const unlocks=flags.includes('--cleanup-unlocks')?cleanupUnlocks(converted.save):null;
if(unlocks) converted.save=unlocks.save;
const report={unlock_cleanup:unlocks?.edits??[],remaining_typed_references:unlocks?.residual??null,dependency_removal_verified:false,source_sha256:sha,changes:converted.changes,residual_mod_references:converted.residual,scope:'copy-only conversion; independent native load not verified',written:false};
report.configuration_pending=converted.changes.filter(c=>c.configurationPending).map(c=>({path:c.path,guid:c.guid,item:c.item,mode:c.mode}));
report.native_acceptance_verified=false;
report.status=report.configuration_pending.length?'BLOCKED_NATIVE_CONFIGURATION':'AWAITING_RUNTIME_VERIFICATION';
if(flags.includes('--write')&&report.configuration_pending.length) {
 console.log(JSON.stringify(report,null,2));
 console.error('BLOCKED: external migration must apply and verify every explicit resource configuration. Use --write-preparation only to create an explicitly incomplete staging copy.');
 process.exitCode=1;
} else if(flags.includes('--write')||flags.includes('--write-preparation')) {
 if(converted.changes.some(c=>c.depot)) {
  assert(receiptFlags.length===1,'Named depot conversion requires actual schema-4 editor reload receipt (--asset-receipt=...)');
  validateDepotReceipt(JSON.parse(fs.readFileSync(receiptFlags[0].slice('--asset-receipt='.length))));
 }
 const manifest=dst+'.native.tsv',receipt=dst+'.report.json';
 assert(!fs.existsSync(manifest)&&!fs.existsSync(receipt),'Output receipt already exists');
 converted.save.name=path.basename(dst,'.sav');converted.save.header.saveName=converted.save.name;
 const chunks=[];Parser.WriteSave(converted.save,h=>chunks.push(Buffer.from(h)),c=>chunks.push(Buffer.from(c)));
 const data=Buffer.concat(chunks),readback=Parser.ParseSave('verify',new Uint8Array(data).buffer);
 assert.deepEqual(readback.levels,converted.save.levels,'Encoded save roundtrip altered body');
 assert.equal(crypto.createHash('sha256').update(fs.readFileSync(src)).digest('hex'),sha,'Source changed during conversion');
 const created=[];
 try {
  for(const [file,body] of [[dst,data],[manifest,converted.manifest]]) {fs.writeFileSync(file,body,{flag:'wx'});created.push(file);}
  report.written=true;report.output_sha256=crypto.createHash('sha256').update(data).digest('hex');
  fs.writeFileSync(receipt,JSON.stringify(report,null,2),{flag:'wx'});
 } catch(e) {for(const f of created)fs.unlinkSync(f);throw e;}
}
console.log(JSON.stringify(report,null,2));
