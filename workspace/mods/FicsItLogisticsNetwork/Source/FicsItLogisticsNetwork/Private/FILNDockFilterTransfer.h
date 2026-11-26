#pragma once
#include "FGInventoryComponent.h"
#include "FGInventoryLibrary.h"
#include "Resources/FGItemDescriptor.h"
#include "FILNDockFilterPolicy.h"

namespace filn::dockfilter {
struct TransferResult { int32 Amount=0; bool Verified=false; };
inline bool ReadCargo(UFGInventoryComponent* Inventory,TMap<UClass*,int64>& Counts) {
 if(!Inventory || Inventory->GetSizeLinear()<=0 || Inventory->GetSizeLinear()>4096) return false;
 Counts.Reset();
 for(int32 Slot=0;Slot<Inventory->GetSizeLinear();++Slot) {
  FInventoryStack Stack;
  if(!Inventory->GetStackFromIndex(Slot,Stack) || Stack.NumItems<0 ||
     (Stack.NumItems>0 && !Stack.Item.IsValid())) return false;
  if(Stack.NumItems>0) Counts.FindOrAdd(Stack.Item.GetItemClass().Get())+=Stack.NumItems;
 }
 return true;
}
inline TransferResult SelectiveSolidUnload(UFGInventoryComponent* Source,UFGInventoryComponent* Target,TSubclassOf<UFGItemDescriptor> Resource) {
 if(!Source || !Target || Source==Target || !Resource || UFGItemDescriptor::GetForm(Resource)!=EResourceForm::RF_SOLID) return {};
 TMap<UClass*,int64> Before,After;
 if(!ReadCargo(Source,Before)) return {};
 const int64 Selected=Before.FindRef(Resource.Get());
 const int64 TargetBefore=Target->GetNumItems(Resource);
 if(Selected<=0 || Selected>MAX_int32 || TargetBefore<0) return {};
 // Native filtered transfer: never emulate an inventory move with separate
 // addition/removal calls. A false result can still mean partial progress;
 // credit only an exact observed delta, not the bool returned by the helper.
 const bool NativeResult=UFGInventoryLibrary::GrabAllItemsFromInventory(Source,Target,Resource);
 if(!ReadCargo(Source,After)) return {};
 const int64 Remaining=After.FindRef(Resource.Get());
 const int64 Delta=Selected-Remaining;
 for(const auto& Item:Before) if(Item.Key!=Resource.Get() && After.FindRef(Item.Key)!=Item.Value) return {};
 for(const auto& Item:After) if(Item.Key!=Resource.Get() && Before.FindRef(Item.Key)!=Item.Value) return {};
 const int64 TargetAfter=Target->GetNumItems(Resource);
 if(Delta==0 && TargetAfter==TargetBefore) return {0,true};
 if(Delta<=0 || Delta>MAX_int32 || !Conserved(Selected,Remaining,TargetBefore,TargetAfter,static_cast<int>(Delta))) return {};
 // Success boolean is not a transfer receipt; only exact inventory deltas are.
 (void)NativeResult;
 return {static_cast<int32>(Delta),true};
}
}
