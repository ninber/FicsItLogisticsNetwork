// SPDX-License-Identifier: GPL-3.0-only
'use strict';
const test=require('node:test'),assert=require('node:assert/strict');
const {cleanupUnlocks,MANAGERS,references}=require('./legacy-unlock-cleanup.cjs');
const {PROGRAMMABLE}=require('./station-migration.cjs');
const asset=pathName=>({levelName:'',pathName});
function fixture(){return {levels:{main:{objects:MANAGERS.map(m=>({typePath:m.type,instanceName:'Persistent_Level:PersistentLevel.'+m.name,properties:{[m.property]:{type:'ArrayProperty',values:[asset('/Game/keep/A'),...m.paths.map(asset),asset('/Game/keep/B')]}}}))}}};}
test('removes exactly six observed unlocks and preserves order and original',()=>{const s=fixture(),before=structuredClone(s),r=cleanupUnlocks(s);assert.deepEqual(s,before);assert.equal(r.edits.length,2);assert.equal(r.edits.reduce((n,e)=>n+e.removed.length,0),6);for(const [i,m] of MANAGERS.entries())assert.deepEqual(r.save.levels.main.objects[i].properties[m.property].values,[asset('/Game/keep/A'),asset('/Game/keep/B')]);assert.equal(r.dependencyFree,false);});
test('rejects an unconverted programmable actor',()=>{const s=fixture();s.levels.main.objects.push({typePath:PROGRAMMABLE,instanceName:'station'});assert.throws(()=>cleanupUnlocks(s));});
test('rejects a still-referenced legacy recipe',()=>{const s=fixture();s.levels.main.objects.push({properties:{mBuiltWithRecipe:{value:asset(MANAGERS[0].paths[0])}}});assert.throws(()=>cleanupUnlocks(s));});
test('retains unrelated legacy references and reports building statistics',()=>{const s=fixture();s.levels.main.objects.push({properties:{stats:new Map([[PROGRAMMABLE,123n]])}});const r=cleanupUnlocks(s);assert.equal(r.residual.length,1);assert.deepEqual(r.save.levels.main.objects[2],s.levels.main.objects[2]);assert.equal(r.dependencyFree,false);});
test('rejects unexpected manager types or malformed arrays',()=>{const s=fixture();s.levels.main.objects[0].typePath='other';assert.throws(()=>cleanupUnlocks(s));const t=fixture();delete t.levels.main.objects[0].properties[MANAGERS[0].property].values;assert.throws(()=>cleanupUnlocks(t));});
test('repeat cleanup is idempotent',()=>{const r=cleanupUnlocks(fixture());const next=cleanupUnlocks(r.save);assert.deepEqual(next.save,r.save);assert.deepEqual(next.edits,[]);});
test('rejects duplicate managers',()=>{const s=fixture();s.levels.main.objects.push(structuredClone(s.levels.main.objects[0]));assert.throws(()=>cleanupUnlocks(s));});
