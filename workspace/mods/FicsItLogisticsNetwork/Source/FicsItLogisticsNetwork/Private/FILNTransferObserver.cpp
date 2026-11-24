// SPDX-License-Identifier: GPL-3.0-only
#include "FILNTransferObserver.h"
#include "LogisticsSubsystem.h"
#include "FILNTransferLedger.h"
#include "FGInventoryComponent.h"
#include "Resources/FGItemDescriptor.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "WheeledVehicles/FGWheeledVehicleIdentifier.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
void UFILNTransferObserver::Bind(ALogisticsSubsystem* S,AFGWheeledVehicleIdentifier* V,UFGInventoryComponent* I) {
 Unbind(); Owner=S; Vehicle=V; Inventory=I;
 if(I) { I->OnItemAddedDelegate.AddUniqueDynamic(this,&UFILNTransferObserver::Added); I->OnItemRemovedDelegate.AddUniqueDynamic(this,&UFILNTransferObserver::Removed); }
}
bool UFILNTransferObserver::IsBoundTo(AFGWheeledVehicleIdentifier* V) const { return Vehicle.Get(true)==V; }
void UFILNTransferObserver::Unbind() {
 if(auto* I=Inventory.Get()) { I->OnItemAddedDelegate.RemoveDynamic(this,&UFILNTransferObserver::Added); I->OnItemRemovedDelegate.RemoveDynamic(this,&UFILNTransferObserver::Removed); }
 Inventory.Reset(); Owner.Reset(); Vehicle.Reset();
}
void UFILNTransferObserver::Added(TSubclassOf<UFGItemDescriptor> Item,int32 Amount,UFGInventoryComponent* Other) {
 if(Owner.IsValid() && Vehicle.IsValid()) Owner->Transfer(Vehicle.Get(),Item,Amount,Other,true);
}
void UFILNTransferObserver::Removed(TSubclassOf<UFGItemDescriptor> Item,int32 Amount,UFGInventoryComponent* Other) {
 if(Owner.IsValid() && Vehicle.IsValid()) Owner->Transfer(Vehicle.Get(),Item,Amount,Other,false);
}
void ALogisticsSubsystem::WatchTransfers(FFILNRegistration& R) {
 auto* V=IsValid(R.Vehicle)?R.Vehicle.Get():nullptr;
 auto* Owner=V?V->GetOwnerVehicle():nullptr;
 if(!IsValid(Owner) || !Owner->GetStorageInventory()) return;
 auto* Observer=NewObject<UFILNTransferObserver>(this); Observer->Bind(this,V,Owner->GetStorageInventory()); TransferObservers.Add(Observer);
}
void ALogisticsSubsystem::Transfer(AFGWheeledVehicleIdentifier* V,TSubclassOf<UFGItemDescriptor> Item,int32 Amount,UFGInventoryComponent* Other,bool Added) {
 if(!HasAuthority() || !IsInGameThread() || Amount<=0) return;
 const auto* Index=ByVehicle.Find(V); if(!Index || !Registrations.IsValidIndex(*Index)) return; auto& R=Registrations[*Index];
 using S=filn::State; const auto State=static_cast<S>(R.State);
 if(State==S::Idle || State==S::Recovery || !R.Automatic) { R.CargoOriginKnown=false; R.CargoOriginStorage=false; return; }
 if(!IsValid(V->GetOwnerVehicle())) return;
 if(V->GetOwnerVehicle()->GetDriver() || !V->IsAutopilotEnabled() || V->GetVehicleRoute()!=R.RouteIntent ||
    !Reservations.OwnsJob(TCHAR_TO_UTF8(*R.JobId.ToString()),TCHAR_TO_UTF8(*R.TruckId.ToString()))) {
  R.CargoOriginKnown=false; R.CargoOriginStorage=false;
  Recover(R,TEXT("Cargo event outside owned AP-on journey; leave cargo untouched")); return;
 }
 auto* Provider=IsValid(R.Provider)?R.Provider->GetStation():nullptr;
 auto* Requester=IsValid(R.Requester)?R.Requester->GetStation():nullptr;
 const bool AtSource=Added && !R.RetainedCargo && IsValid(Provider) && Provider->GetDockedActor()==V->GetOwnerVehicle() &&
  Other==Provider->GetInventory() && (State==S::ToProvider || State==S::Loading);
 const bool AtTarget=!Added && IsValid(Requester) && Requester->GetDockedActor()==V->GetOwnerVehicle() &&
  Other==Requester->GetInventory() && (State==S::ToRequester || State==S::Unloading);
 if(Item!=R.Resource || (!AtSource && !AtTarget)) { R.CargoOriginKnown=false; R.CargoOriginStorage=false; Recover(R,TEXT("Cargo event has no matching reserved native transfer endpoint")); return; }
 // Delegate timing (before/after inventory mutation) must not decide the ledger.
 // Reconcile the signed event total with the next authoritative inventory snapshot.
 filn::transfer::Ledger Ledger{R.LastCargo,R.LoadedCargo,R.DeliveredCargo};
 if(!filn::transfer::Record(Ledger,Amount,Added)) {
  R.CargoOriginKnown=false; R.CargoOriginStorage=false;
  Recover(R,TEXT("Native transfer event exceeds the observed cargo ledger")); return;
 }
 R.LastCargo=Ledger.cargo; R.LoadedCargo=Ledger.loaded; R.DeliveredCargo=Ledger.delivered;
 if(Added) { R.CargoOriginKnown=true; R.CargoOriginStorage=filn::dispatch::IsStorage(TCHAR_TO_UTF8(*R.Provider->GetStationName().ToString())); R.State=static_cast<uint8>(S::Loading); }
 else { R.State=static_cast<uint8>(S::Unloading); if(!R.LastCargo) { R.CargoOriginKnown=false; R.CargoOriginStorage=false; } }
}
