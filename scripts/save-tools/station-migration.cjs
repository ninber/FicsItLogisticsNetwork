const FILN='/KDataForge/Gen/FILNStations.Gen_FILNStations_Recipe_LogisticsStation';
const ORDINARY='/Game/FactoryGame/Buildable/Factory/TruckStation/Build_TruckStation.Build_TruckStation_C';
const PROGRAMMABLE='/TruckStationSettings/Build_ProgrammableTruckStation.Build_ProgrammableTruckStation_C';
const part=(folder,desc)=>`/Game/FactoryGame/Resource/${folder}/${desc}.${desc}_C`;
const rules=[
 [/\breinforced\s*(?:iron\s*)?(?:plates?|ip)\b/,part('Parts/IronPlateReinforced','Desc_IronPlateReinforced')],
 [/\bsmart\s*plating\b/,part('Parts/SpaceElevatorParts','Desc_SpaceElevatorPart_1')],
 [/\biron\s*ore\b/,part('RawResources/OreIron','Desc_OreIron')],
 [/\bcopper\s*ore\b/,part('RawResources/OreCopper','Desc_OreCopper')],
 [/\biron\s*ingots?\b/,part('Parts/IronIngot','Desc_IronIngot')],
 [/\bcopper\s*ingots?\b/,part('Parts/CopperIngot','Desc_CopperIngot')],
 [/\b(?:iron\s*)?rods?\b/,part('Parts/IronRod','Desc_IronRod')],
 [/\bscrews?\b/,part('Parts/IronScrew','Desc_IronScrew')],
 [/\brotors?\b/,part('Parts/Rotor','Desc_Rotor')],
 [/\blimestone\b/,part('RawResources/Stone','Desc_Stone')],
 [/\bconcrete\b/,part('Parts/Cement','Desc_Cement')],
 [/\bcoal\b/,part('RawResources/Coal','Desc_Coal')],
 [/\bwires?\b/,part('Parts/Wire','Desc_Wire')],
 [/\bcables?\b/,part('Parts/Cable','Desc_Cable')],
 [/\b(?:iron\s*)?plates?\b/,part('Parts/IronPlate','Desc_IronPlate')],
];
function itemFromName(name){
 if(!name)return null;
 const normalized=name.toLowerCase().replace(/ignots?|ognots?/g,'ingots').replace(/\bcooper\b/g,'copper');
 if(/^iron\s+in\s+ingots?\b/.test(normalized))return part('RawResources/OreIron','Desc_OreIron');
 if(/^copper\s+in\s+ingots?\b/.test(normalized))return part('RawResources/OreCopper','Desc_OreCopper');
 if(/^concrete\s+limestone\b/.test(normalized))return part('RawResources/Stone','Desc_Stone');
 const prefix=normalized.split(/\b(?:in|out|input|output)\b/)[0].trim();
 const matches=rules.filter(([pattern])=>pattern.test(prefix)).map(([,item])=>item);
 // Reinforced plates also match the general plate token; the more specific item wins.
 if(matches[0]===rules[0][1])return matches[0];
 return matches.length===1?matches[0]:null;
}
function label(identifier){const text=identifier?.properties.mStationName?.value;return typeof text?.value==='string'?text.value:null;}
function cargoItems(by,station){
 const refs=(station.components||[]).filter(r=>/\.inventory$/i.test(r.pathName));
 if(refs.length!==1)return null;
 const inventory=by.get(refs[0].pathName);if(!inventory)return null;
 const totals=new Map();
 for(const stack of inventory.properties.mInventoryStacks?.values||[]){
  const count=stack.properties?.NumItems?.value||0;
  if(!Number.isInteger(count)||count<0)return null;
  if(count){const item=stack.properties.Item?.value.itemReference?.pathName;if(!item)return null;totals.set(item,(totals.get(item)||0)+count);}
 }
 return [...totals].map(([item,quantity])=>({item,quantity}));
}
function plan(save,sha256){
 const objects=Object.values(save.levels).flatMap(l=>l.objects),by=new Map(objects.map(o=>[o.instanceName,o]));
 const stations=[],skipped=[];
 for(const station of objects){
  if(![ORDINARY,PROGRAMMABLE].includes(station.typePath))continue;
  const recipe=station.properties.mBuiltWithRecipe?.value.pathName;
  const identifier=by.get(station.properties.mStationIdentifier?.value.pathName);const name=label(identifier);
  const mode=station.properties.mIsInLoadMode?.value===false?'UNLOAD':'LOAD';
  const base={path:station.instanceName,name:name||'(default name)',mode};
  if(recipe?.includes('FILNRestStop')){skipped.push({...base,reason:'parking bay/depot preserved'});continue;}
  if(recipe===FILN){skipped.push({...base,reason:'already FILN'});continue;}
  const filter=station.properties.FilterItem?.values;
  if(station.typePath===PROGRAMMABLE&&Array.isArray(filter)&&filter.some(x=>x.pathName)){
   stations.push({...base,recipe:FILN,source:'existing in-game filter preserved'});continue;
  }
  const stock=cargoItems(by,station);
  if(!stock){skipped.push({...base,reason:'cargo inventory could not be verified'});continue;}
  if(stock.length>1){skipped.push({...base,reason:'mixed cargo is ambiguous',stock});continue;}
  const named=itemFromName(name),stored=stock[0]?.item;
  if(named&&stored&&named!==stored){skipped.push({...base,reason:'name and cargo disagree',name_item:named,stock});continue;}
  const item=named||stored;
  if(!item){skipped.push({...base,reason:'empty with no unambiguous name or filter'});continue;}
  stations.push({...base,item,recipe:FILN,source:named?'station name':'unique existing cargo',stock});
 }
 return {sha256,stations,skipped,policy:'static migration only; preserve native LOAD/UNLOAD, explicit filters, identity and all connections'};
}
module.exports={plan,itemFromName,cargoItems,label,FILN,ORDINARY,PROGRAMMABLE};
