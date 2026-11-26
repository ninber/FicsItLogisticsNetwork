#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-only
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const {Parser}=require(process.env.FILN_SAVE_PARSER||'@etothepii/satisfactory-file-parser');
const {recover}=require('./recover-native-stations.cjs');
const [input,output,receiptFile,historyFile,...extra]=process.argv.slice(2);
assert(input && output && receiptFile && historyFile && !extra.length,
 'Usage: node recover-native-city.cjs INPUT.sav OUTPUT.sav FRESH_RELOAD_RECEIPT.json HISTORICAL_MIGRATION_REPORT.json');
const src=fs.realpathSync(input),dir=fs.realpathSync(path.dirname(output)),dst=path.join(dir,path.basename(output));
assert(src!==dst && !fs.existsSync(dst),'Never overwrite existing save');
assert(!dir.split(path.sep).some(s=>s.toLowerCase()==='savegames'),'Output must be outside live SaveGames');
const bytes=fs.readFileSync(src),sha=crypto.createHash('sha256').update(bytes).digest('hex');
const receipt=JSON.parse(fs.readFileSync(receiptFile)), history=JSON.parse(fs.readFileSync(historyFile));
if(history.current_save_sha256) assert.equal(history.current_save_sha256,sha,'Recovery report was verified against a different save');
const parsed=Parser.ParseSave(path.basename(src,'.sav'),new Uint8Array(bytes).buffer);
const result=recover(parsed,receipt,history);
result.save.name=path.basename(dst,'.sav');result.save.header.saveName=result.save.name;
const chunks=[];Parser.WriteSave(result.save,h=>chunks.push(Buffer.from(h)),c=>chunks.push(Buffer.from(c)));
const encoded=Buffer.concat(chunks),readback=Parser.ParseSave('verify',new Uint8Array(encoded).buffer);
assert.deepEqual(readback.levels,result.save.levels,'Typed roundtrip changed save');
const verified=recover(readback,receipt,history);
assert.equal(verified.changes.length,0,'Output still contains unconverted stations');
assert.deepEqual(verified.stations,result.stations,'Output station configurations changed');
assert.equal(crypto.createHash('sha256').update(fs.readFileSync(src)).digest('hex'),sha,'Source changed');
fs.writeFileSync(dst,encoded,{flag:'wx'});
console.log('STAGING ONLY: '+result.stations.length+' station classes verified; '+result.stations.filter(s=>s.resource).length+' saved filters preserved. Historical deleted/missing: '+result.historicalMissing.length+'. Shipping FILTERS UI, inventory and dispatch acceptance still required. Do not install as a completed migration.');
