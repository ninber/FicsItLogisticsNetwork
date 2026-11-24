// SPDX-License-Identifier: GPL-3.0-only
'use strict';
// Parser objects have SaveEntity/SaveComponent prototypes. structuredClone drops
// them; retain those prototypes and preserve typed-buffer aliasing without any
// mutation of the parsed source.
function cloneSave(value,seen=new Map()) {
 if(value===null||typeof value!=='object')return value;
 if(seen.has(value))return seen.get(value);
 let copy;
 if(value instanceof ArrayBuffer || (typeof SharedArrayBuffer!=='undefined'&&value instanceof SharedArrayBuffer)) {
  copy=value.slice(0);seen.set(value,copy);return copy;
 }
 if(ArrayBuffer.isView(value)) {
  const buffer=cloneSave(value.buffer,seen);
  copy=Buffer.isBuffer(value)?Buffer.from(buffer,value.byteOffset,value.byteLength):value instanceof DataView?new DataView(buffer,value.byteOffset,value.byteLength):new value.constructor(buffer,value.byteOffset,value.length);
  seen.set(value,copy);return copy;
 }
 if(value instanceof Map) {copy=new Map();seen.set(value,copy);for(const [k,v]of value)copy.set(cloneSave(k,seen),cloneSave(v,seen));return copy;}
 if(value instanceof Set) {copy=new Set();seen.set(value,copy);for(const v of value)copy.add(cloneSave(v,seen));return copy;}
 if(value instanceof Date) {copy=new Date(value);seen.set(value,copy);return copy;}
 if(value instanceof RegExp) {copy=new RegExp(value.source,value.flags);copy.lastIndex=value.lastIndex;seen.set(value,copy);return copy;}
 copy=Array.isArray(value)?new Array(value.length):Object.create(Object.getPrototypeOf(value));seen.set(value,copy);
 for(const key of Reflect.ownKeys(value)) {
  const descriptor=Object.getOwnPropertyDescriptor(value,key);
  if('value'in descriptor)descriptor.value=cloneSave(descriptor.value,seen);
  Object.defineProperty(copy,key,descriptor);
 }
 return copy;
}
module.exports={cloneSave};
