#pragma once
#include "FGInventoryComponent.h"
#include "Resources/FGItemDescriptor.h"
#include "GameFramework/Actor.h"

namespace filn::inventory {
// Shipping CL520140 crashed in GetSlotSizeForItem when its optional Item
// argument was omitted. Never call that API with a null or mismatched item.
// No estimated capacities or mutation of the inventory.
inline bool TryCapacity(UFGInventoryComponent* Inventory,TSubclassOf<UFGItemDescriptor> Resource,int64& Out) {
 Out=0;
 if(!IsInGameThread() || !IsValid(Inventory) || !Inventory->IsRegistered() ||
    !IsValid(Inventory->GetOwner()) || !IsValid(Resource.Get()) ||
    !IsValid(Resource.GetDefaultObject())) return false;
 const int32 Slots=Inventory->GetSizeLinear();
 if(Slots<=0 || Slots>4096) return false;
 const FInventoryItem Probe(Resource);
 if(!Probe.IsValid() || Probe.GetItemClass()!=Resource) return false;
 int64 Sum=0;
 for(int32 Slot=0;Slot<Slots;++Slot) {
  if(!Inventory->IsValidIndex(Slot)) return false;
  const auto Filter=Inventory->GetAllowedItemOnIndex(Slot);
  if(Filter && Filter!=Resource) continue;
  if(!Inventory->IsItemAllowed(Resource,Slot)) continue;
  FInventoryStack Stack;
  if(!Inventory->GetStackFromIndex(Slot,Stack) || Stack.NumItems<0 ||
     (Stack.NumItems>0 && !Stack.Item.IsValid())) return false;
  // Use the real item state where this resource already occupies the slot.
  // For empty/other slots use a valid descriptor-specific probe, not nullptr.
  const FInventoryItem& Item=Stack.NumItems>0 && Stack.Item.GetItemClass()==Resource?Stack.Item:Probe;
  const int32 Size=Inventory->GetSlotSizeForItem(Slot,Resource,&Item);
  if(Size<=0 || (Stack.NumItems>0 && Stack.Item.GetItemClass()==Resource && Stack.NumItems>Size)) return false;
  Sum+=Size;
  if(Sum>MAX_int32) return false;
 }
 if(Inventory->GetSizeLinear()!=Slots) return false;
 Out=Sum;
 return true;
}
}
