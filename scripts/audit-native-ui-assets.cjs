#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-only
'use strict';
// Read-only structural audit for the three exact UE5.6.1-CSS SML v3.12.1 SDK assets.
// No engine execution, editor input, modifications, save access or network calls.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const paths={
  "station": "Content/FactoryGame/Buildable/-Shared/Widgets/Widget_DockingStation.uasset",
  "window": "Content/FactoryGame/Interface/UI/Widget_Window_DarkMode.uasset",
  "switch": "Content/FactoryGame/Interface/UI/InGame/SignModule/BPW_OnOffSwitch.uasset"
};
const expected={
  "station": "c18eba2fa05b3e6a9b47102dae03ee431574be68",
  "window": "426a8b331887d7fd887faaa903f54ea3763362fc",
  "switch": "b87d4fac3431750a8e7aa122bce685651a00fa46"
};
function inspectAsset(bytes,pkg) {
 const fail=m=>{throw Error(pkg+": "+m)};
 const require=(b,m)=>{if(!b)fail(m)};
 const d=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength);
 const u=i=>d.getUint32(i,true),s=i=>d.getInt32(i,true);
 const str=i=>{
  const len=s(i);require(len!==0&&Math.abs(len)<100000,"unexpected FString length");
  const chars=[];const size=len>0?len:-len*2;require(i+4+size<=bytes.length,"truncated FString");
  for(let j=0;j<Math.abs(len)-1;j++)chars.push(len>0?bytes[i+4+j]:d.getUint16(i+4+2*j,true));
  require(len>0?bytes[i+4+size-1]===0:d.getUint16(i+4+size-2,true)===0,"unterminated FString");
  return [String.fromCharCode(...chars),i+4+size];
 };
 require(u(0)===0x9e2a83c1&&s(4)===-9&&u(12)===522&&u(16)===1017,"not pinned UE5.6 CSS package format");
 const signature=Array.from(pkg,c=>c.charCodeAt(0));
 let pos=4;while(pos<1024&&!signature.every((v,i)=>bytes[pos+i]===v))pos++;
 require(pos<1024,"package name not found");
 const [actual,after]=str(pos-4);require(actual===pkg,"package name mismatch");
 const count=u(after+4),offset=u(after+8);require(count>0&&count<10000&&offset<bytes.length,"invalid name map");
 let names=[],q=offset;for(let i=0;i<count;i++){const [v,next]=str(q);names.push(v);q=next+4;}
 const [,next]=str(after+20),ec=u(next+8),eo=u(next+12),ic=u(next+16),io=u(next+20);
 require(ec>0&&ic>0&&eo-io===ic*40&&u(next+28)-eo===ec*112&&q<=io&&u(next+28)<=bytes.length,"unexpected table layout");
 const name=i=>{const index=s(i),number=s(i+4);return index>=0&&index<names.length&&number>=0?names[index]+(number?"_"+(number-1):""):null};
 const imports=Array.from({length:ic},(_,j)=>({name:name(io+j*40+20)}));
 const exports=Array.from({length:ec},(_,j)=>{
  const z=eo+j*112;const x={index:j+1,name:name(z+16),classRef:s(z),outer:s(z+12),size:Number(d.getBigInt64(z+28,true)),offset:Number(d.getBigInt64(z+36,true))};
  require(x.name&&x.size>=0&&x.offset>=0&&x.offset+x.size<=bytes.length&&x.classRef>=-ic&&x.classRef<=ec&&x.outer>=-ic&&x.outer<=ec,"invalid export");
  return x;
 });
 const classOf=x=>x.classRef<0?imports[-x.classRef-1]?.name:exports[x.classRef-1]?.name;
 const refs=(x,prop,from=0)=>{
  const result=[];
  for(let j=x.offset+from;j+29<=x.offset+x.size;j++){
   if(name(j)===prop&&name(j+8)==="ObjectProperty"&&u(j+16)===0&&u(j+20)===4&&bytes[j+24]===0)result.push(s(j+25));
  }
  return result;
 };
 const findTree=(owner)=>{
  const v=exports.filter(x=>classOf(x)==="WidgetTree"&&exports[x.outer-1]?.name===owner);
  require(v.length===1,"expected one widget tree for "+owner);return v[0];
 };
 const findNode=(tree,node,cls)=>{
  const v=exports.filter(x=>x.outer===tree.index&&x.name===node&&(!cls||classOf(x)===cls));
  require(v.length===1,"missing/ambiguous "+node+" ("+cls+")");return v[0];
 };
 const edge=(parent,child)=>{
  const v=exports.filter(x=>x.outer===parent.index&&classOf(x)?.endsWith("Slot")&&refs(x,"Content").includes(child.index));
  require(v.length===1,"missing/ambiguous edge "+parent.name+" -> "+child.name);return v[0].name;
 };
 const findTag=(x,prop,type)=>{
  const v=[];for(let j=x.offset;j+20<=x.offset+x.size;j++)if(name(j)===prop&&name(j+8)===type)v.push(j-x.offset);
  require(v.length===1,"missing/ambiguous "+prop+" tag in "+x.name);return v[0];
 };
 const containsString=(x,val,begin=0,end=x.size)=>{
  const arr=Array.from(val,c=>c.charCodeAt(0));for(let j=x.offset+begin;j+arr.length<=x.offset+end;j++)if(arr.every((v,k)=>bytes[j+k]===v))return true;return false;
 };
 const rootOf=tree=>{const v=refs(tree,"RootWidget");require(v.length===1,"root reference ambiguous");return exports[v[0]-1];};
 return {exports,findTree,findNode,edge,refs,findTag,containsString,rootOf,classOf,name,require};
}
function auditAssets(input) {
 const root="/Game/FactoryGame/";
 const a=inspectAsset(input.station,root+"Buildable/-Shared/Widgets/Widget_DockingStation");
 const b=inspectAsset(input.window,root+"Interface/UI/Widget_Window_DarkMode");
 const c=inspectAsset(input.switch,root+"Interface/UI/InGame/SignModule/BPW_OnOffSwitch");
 const t=a.findTree("Widget_DockingStation_C"),win=a.findNode(t,"Widget_Window_DarkMode","Widget_Window_DarkMode_C"),canvas=a.findNode(t,"CanvasPanel_68","CanvasPanel");
 a.require(a.rootOf(t)?.index===win.index,"unexpected station root");
 const binding=a.findTag(win,"NamedSlotBindings","ArrayProperty");
 a.require(a.refs(win,"Content",binding).length===1&&a.refs(win,"Content",binding)[0]===canvas.index,"WindowBody binding does not target expected canvas");
 let key=false;for(let i=win.offset+binding;i+8<=win.offset+win.size;i++)if(a.name(i)==="WindowBody")key=true;
 a.require(key,"named slot key not found");
 const settings=a.findNode(t,"VerticalBox_1","VerticalBox"),mode=a.findNode(t,"BPW_OnOffSwitch","BPW_OnOffSwitch_C"),fluid=a.findNode(t,"mForceFluidSwitch","BPW_OnOffSwitch_C"),fluidBox=a.findNode(t,"VerticalBox_0","VerticalBox");
 a.edge(canvas,settings);a.edge(settings,mode);a.edge(canvas,fluidBox);a.edge(fluidBox,fluid);
 const on=a.findTag(mode,"mOnText","TextProperty"),off=a.findTag(mode,"mOffText","TextProperty");
 a.require(on<off&&a.containsString(mode,"Vehicles/DockingStation/Info/Status/Load",on,off)&&a.containsString(mode,"Vehicles/DockingStation/Info/Status/Unload",off),"mode switch localization keys changed");
 const wt=b.findTree("Widget_Window_DarkMode_C"),wr=b.findNode(wt,"VerticalBox_0","VerticalBox"),main=b.findNode(wt,"mMainContainer","VerticalBox"),bodyCanvas=b.findNode(wt,"CanvasPanel_0","CanvasPanel"),hbox=b.findNode(wt,"WindowHBox","HorizontalBox"),body=b.findNode(wt,"WindowBody","NamedSlot");
 b.require(b.rootOf(wt)?.index===wr.index,"unexpected dark window root");b.edge(wr,main);b.edge(main,bodyCanvas);b.edge(bodyCanvas,hbox);b.edge(hbox,body);
 const setter=c.exports.filter(x=>x.name==="SetOnOffTexts"&&c.classOf(x)==="Function"&&c.exports[x.outer-1]?.name==="BPW_OnOffSwitch_C");
 c.require(setter.length===1,"missing or ambiguous SetOnOffTexts");
 const fields=[];for(let i=setter[0].offset;i+16<=setter[0].offset+setter[0].size;i++)if(c.name(i)==="TextProperty"&&["mOnText","mOffText"].includes(c.name(i+8)))fields.push(c.name(i+8));
 c.require(fields.length===2&&new Set(fields).size===2,"unexpected switch setter serialized text fields");
 const authored=c.findTree("BPW_OnOffSwitch");
 c.findNode(authored,"mOnTextObject","TextBlock");c.findNode(authored,"mOffTextObject","TextBlock");
 return {scope:"pinned SDK asset structure only; not live widget, engine compile or gameplay acceptance",
  station:{root:win.name,namedSlot:"WindowBody",body:canvas.name,bodySlots:a.exports.filter(x=>x.outer===canvas.index&&a.classOf(x)==="CanvasPanelSlot").length,modePath:[canvas.name,settings.name,mode.name],fluidSwitch:fluid.name,modeTextKeys:["Vehicles/DockingStation/Info/Status/Load","Vehicles/DockingStation/Info/Status/Unload"]},
  window:{root:wr.name,appendContainer:main.name,namedSlotPath:[wr.name,main.name,bodyCanvas.name,hbox.name,body.name]},
  switch:{setter:setter[0].name,serializedTextFields:fields,authoredTextWidgets:["mOnTextObject","mOffTextObject"]}};
}
if(require.main===module){
 try{
  if(process.argv.length!==3)throw Error('Usage: node scripts/audit-native-ui-assets.cjs /path/to/StarterProject');
  const input={},hashes={};
  for(const k of Object.keys(paths)){
   const bytes=fs.readFileSync(path.join(process.argv[2],paths[k]));
   const sha=crypto.createHash('sha1').update('blob '+bytes.length+'\0').update(bytes).digest('hex');
   hashes[k]={sha,pinned:sha===expected[k],bytes:bytes.length};
   input[k]=bytes;
  }
  const result=auditAssets(input);
  console.log(JSON.stringify({hashes,...result},null,2));
  if(Object.values(hashes).some(h=>!h.pinned))console.warn('WARNING: local SDK asset bytes differ from pinned upstream; structure passed, review the differences.');
 }catch(error){console.error('FAIL: '+error.message);process.exitCode=1;}
}
module.exports={inspectAsset,auditAssets,paths,expected};
