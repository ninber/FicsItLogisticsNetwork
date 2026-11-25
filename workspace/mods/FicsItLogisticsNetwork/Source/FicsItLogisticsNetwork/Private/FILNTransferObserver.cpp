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
#include "Patching/NativeHookManager.h"
#include "Subsystem/SubsystemActorManager.h"
#include "Engine/World.h"
void UFILNTransferObserver::Bind(ALogisticsSubsystem* S,AFGWheeledVehicleIdentifier* V,UFGInventoryComponent* I,const FGuid& Truck) {
 Unbind(); Owner=S; Vehicle=V; Inventory=I; TruckId=Truck;
 if(IsValid(V)) { BoundOwner=V->GetOwnerVehicle(); V->OnDestroyed.AddUniqueDynamic(this,&UFILNTransferObserver::Destroyed); }
 if(I) { I->OnItemAddedDelegate.AddUniqueDynamic(this,&UFILNTransferObserver::Added); I->OnItemRemovedDelegate.AddUniqueDynamic(this,&UFILNTransferObserver::Removed); }
}
void UFILNTransferObserver::Destroyed(AActor* Actor) {
 if(auto* S=Owner.Get()) if(auto* V=Cast<AFGWheeledVehicleIdentifier>(Actor)) S->NoteVehicleDestroyed(TruckId,V,BoundOwner);
}
void UFILNTransferObserver::Unbind() {
 if(auto* I=Inventory.Get()) { I->OnItemAddedDelegate.RemoveDynamic(this,&UFILNTransferObserver::Added); I->OnItemRemovedDelegate.RemoveDynamic(this,&UFILNTransferObserver::Removed); }
 if(auto* V=Vehicle.Get()) V->OnDestroyed.RemoveDynamic(this,&UFILNTransferObserver::Destroyed);
 Inventory.Reset(); Owner.Reset(); Vehicle.Reset(); BoundOwner.Reset(); TruckId.Invalidate();
}
bool UFILNTransferObserver::Matches(AFGWheeledVehicleIdentifier* V,AFGWheeledVehicle* O,UFGInventoryComponent* I) const {
 return IsValid(V) && IsValid(O) && IsValid(I) && Vehicle.Get()==V && BoundOwner.Get()==O && Inventory.Get()==I;
}
void UFILNTransferObserver::Forward(TSubclassOf<UFGItemDescriptor> Item,int32 Amount,UFGInventoryComponent* Other,bool Added) {
 auto* S=Owner.Get(); auto* V=Vehicle.Get();
 if(!S || !V || !IsInGameThread() || !S->HasAuthority() || !S->bRegistryValidated) return;
 auto* O=V->GetOwnerVehicle();
 if(!IsValid(O) || !Matches(V,O,O->GetStorageInventory())) {
  // An old component must never credit events to a replacement inventory.
  const auto* Index=S->ByVehicle.Find(V);
  if(Index && S->Registrations.IsValidIndex(*Index)) {
   auto& R=S->Registrations[*Index]; R.CargoOriginKnown=false; R.CargoOriginStorage=false;
   if(R.JobId.IsValid()) S->Recover(R,TEXT("Native cargo inventory identity changed during delivery"));
  }
  return;
 }
 S->Transfer(V,Item,Amount,Other,Added);
}
void UFILNTransferObserver::Added(TSubclassOf<UFGItemDescriptor> Item,int32 Amount,UFGInventoryComponent* Other) {
 Forward(Item,Amount,Other,true);
}
void UFILNTransferObserver::Removed(TSubclassOf<UFGItemDescriptor> Item,int32 Amount,UFGInventoryComponent* Other) {
 Forward(Item,Amount,Other,false);
}
bool ALogisticsSubsystem::HasTransferObserver(const FFILNRegistration& R) const {
 auto* V=IsValid(R.Vehicle)?R.Vehicle.Get():nullptr;
 auto* O=V?V->GetOwnerVehicle():nullptr;
 const auto* Ref=TransferByTruck.Find(R.TruckId);
 auto* Observer=Ref?Ref->Get():nullptr;
 return Observer && IsValid(O) && Observer->Matches(V,O,O->GetStorageInventory());
}
bool ALogisticsSubsystem::WatchTransfers(FFILNRegistration& R) {
 if(HasTransferObserver(R)) return true;
 auto* V=IsValid(R.Vehicle)?R.Vehicle.Get():nullptr;
 auto* O=V?V->GetOwnerVehicle():nullptr;
 if(!V || DestroyedVehicles.Contains(R.TruckId)) return false;
 auto* Cargo=IsValid(O)?O->GetStorageInventory():nullptr;
 if(R.JobId.IsValid()) {
  // A vehicle identifier can change physical owner as the game changes
  // significance/proxy state. Resume only on the same reserved route and
  // an exact homogeneous cargo ledger; never infer a transfer on rebinding.
  if(!IsValid(Cargo) || !R.Resource || !R.Automatic || O->GetDriver() || !V->IsAutopilotEnabled() ||
     V->GetVehicleRoute()!=R.RouteIntent || !Reservations.OwnsJob(TCHAR_TO_UTF8(*R.JobId.ToString()),TCHAR_TO_UTF8(*R.TruckId.ToString()))) return false;
  TArray<FInventoryStack> Stacks; Cargo->GetInventoryStacks(Stacks); bool Mixed=false;
  for(const auto& Stack:Stacks) if(Stack.NumItems<0 || (Stack.NumItems>0 && Stack.Item.GetItemClass()!=R.Resource)) Mixed=true;
  if(!filn::transfer::Reconciled({R.LastCargo,R.LoadedCargo,R.DeliveredCargo},Cargo->GetNumItems(R.Resource),Mixed)) return false;
 }
 if(auto* Ref=TransferByTruck.Find(R.TruckId)) {
  if(auto* Existing=Ref->Get()) {
   if(!R.JobId.IsValid()) { R.CargoOriginKnown=false; R.CargoOriginStorage=false; }
   Existing->Unbind(); TransferObservers.Remove(Existing);
  }
  TransferByTruck.Remove(R.TruckId);
 }
 auto* Observer=NewObject<UFILNTransferObserver>(this);
 Observer->Bind(this,V,IsValid(O)?O->GetStorageInventory():nullptr,R.TruckId);
 TransferObservers.Add(Observer); TransferByTruck.Add(R.TruckId,Observer);
 return HasTransferObserver(R);
}
void ALogisticsSubsystem::Transfer(AFGWheeledVehicleIdentifier* V,TSubclassOf<UFGItemDescriptor> Item,int32 Amount,UFGInventoryComponent* Other,bool Added,bool ConfirmedProgrammableUnload) {
 if(!HasAuthority() || !IsInGameThread() || !bRegistryValidated || Amount<=0) return;
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
 const bool AtSource=Added && !R.AbortReturn && !R.RetainedCargo && IsValid(Provider) && Provider->GetIsInLoadMode() && Provider->GetDockedActor()==V->GetOwnerVehicle() &&
  Other==Provider->GetInventory() && (State==S::ToProvider || State==S::Loading);
 const bool TargetDocking=!Added && !R.AbortReturn && IsValid(Requester) && !Requester->GetIsInLoadMode() && Requester->GetDockedActor()==V->GetOwnerVehicle() &&
  (State==S::ToRequester || State==S::Unloading ||
   filn::recovery::CanBridgePickupDeparture(State==S::Loading,IsValid(Provider) && Provider->GetDockedActor()==V->GetOwnerVehicle(),R.LoadedCargo,R.LastCargo));
 // Smarter Truck Stations calls RemoveFromIndex without targetInventory.
 // Defer its removal delegates to the station operation's actual result;
 // never infer delivery from a null target or count the same transfer twice.
 if(TargetDocking && IsProgrammableStation(Requester) && !ConfirmedProgrammableUnload &&
    (!Other || Other==Requester->GetInventory()) && Item==R.Resource && EndpointConfigurationValid(R)) return;
 const bool AtTarget=TargetDocking && Other==Requester->GetInventory();
 if(Item!=R.Resource || (!AtSource && !AtTarget) || !EndpointConfigurationValid(R)) { R.CargoOriginKnown=false; R.CargoOriginStorage=false; Recover(R,TEXT("Cargo event has no matching reserved native transfer endpoint")); return; }
 // Delegate timing (before/after inventory mutation) must not decide the ledger.
 // Reconcile the signed event total with the next authoritative inventory snapshot.
 filn::transfer::Ledger Ledger{R.LastCargo,R.LoadedCargo,R.DeliveredCargo};
 if(!filn::transfer::Record(Ledger,Amount,Added)) {
  R.CargoOriginKnown=false; R.CargoOriginStorage=false;
  Recover(R,TEXT("Native transfer event exceeds the observed cargo ledger")); return;
 }
 R.LastCargo=Ledger.cargo; R.LoadedCargo=Ledger.loaded; R.DeliveredCargo=Ledger.delivered; R.DepartedHome=true;
 if(Added) { R.CargoOriginKnown=true; R.CargoOriginStorage=filn::dispatch::IsStorage(TCHAR_TO_UTF8(*R.Provider->GetStationName().ToString())); R.State=static_cast<uint8>(S::Loading); }
 else { R.State=static_cast<uint8>(S::Unloading); if(!R.LastCargo) { R.CargoOriginKnown=false; R.CargoOriginStorage=false; } }
}

void ALogisticsSubsystem::InstallProgrammableUnloadObserver() {
 if(WITH_EDITOR) return;
 // SML 3.12.1 invokes after handlers with the final overridden return value.
 // Do not install a competing before hook or override TruckStationSettings.
 SUBSCRIBE_METHOD_AFTER(AFGBuildableDockingStation::Factory_LoadUnloadDockedInventory,
  [](int32 Result,const AFGBuildableDockingStation* Station,UFGInventoryComponent* Cargo) {
   if(Result<=0 || !IsInGameThread() || !IsProgrammableStation(Station) || !Station->HasAuthority() || Station->GetIsInLoadMode()) return;
   auto* World=Station->GetWorld();
   auto* Manager=World?World->GetSubsystem<USubsystemActorManager>():nullptr;
   auto* S=Manager?Manager->GetSubsystemActor<ALogisticsSubsystem>():nullptr;
   if(S) S->ObserveProgrammableUnload(Station,Cargo,Result);
  });
}
void ALogisticsSubsystem::ObserveProgrammableUnload(const AFGBuildableDockingStation* Station,UFGInventoryComponent* Cargo,int32 Result) {
 if(!HasAuthority() || !bRegistryValidated || bDispatchBlocked || !IsValid(Station) || !IsValid(Cargo) || Result<=0) return;
 auto* O=Cast<AFGWheeledVehicle>(Station->GetDockedActor());
 auto* V=IsValid(O)?O->GetVehicleIdentifier():nullptr;
 if(!IsValid(V) || O->GetStorageInventory()!=Cargo) return;
 const auto* Index=ByVehicle.Find(V); if(!Index || !Registrations.IsValidIndex(*Index)) return;
 auto& R=Registrations[*Index];
 using S=filn::State; const auto Phase=static_cast<S>(R.State);
 if(!R.JobId.IsValid() || !R.Automatic || R.AbortReturn || Phase==S::Idle || Phase==S::Recovery ||
    !IsValid(R.Requester) || R.Requester->GetStation()!=Station || !R.Resource ||
    !EndpointConfigurationValid(R)) return;
 if(O->GetDriver() || !V->IsAutopilotEnabled() || V->GetVehicleRoute()!=R.RouteIntent ||
    !Reservations.OwnsJob(TCHAR_TO_UTF8(*R.JobId.ToString()),TCHAR_TO_UTF8(*R.TruckId.ToString()))) return;
 TArray<FInventoryStack> Stacks; Cargo->GetInventoryStacks(Stacks); bool Mixed=false;
 for(const auto& Stack:Stacks) if(Stack.NumItems<0 || (Stack.NumItems>0 && Stack.Item.GetItemClass()!=R.Resource)) Mixed=true;
 const int32 Observed=Cargo->GetNumItems(R.Resource);
 if(!filn::transfer::ConfirmedUnload({R.LastCargo,R.LoadedCargo,R.DeliveredCargo},Observed,Result,Mixed)) {
  R.CargoOriginKnown=false; R.CargoOriginStorage=false;
  Recover(R,TEXT("Programmable unload result disagrees with cargo readback; no delivery credited")); return;
 }
 Transfer(V,R.Resource,Result,Station->GetInventory(),false,true);
}
