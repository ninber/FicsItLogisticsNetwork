// SPDX-License-Identifier: GPL-3.0-only
'use strict';
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
module.exports={references};
