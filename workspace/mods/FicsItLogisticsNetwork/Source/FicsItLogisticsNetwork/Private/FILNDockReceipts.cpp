// SPDX-License-Identifier: GPL-3.0-only
#include "LogisticsSubsystem.h"
#include "FILNTransferLedger.h"
#include "FILNDockFilterPolicy.h"
#include "FILNDockFilterTransfer.h"
#include "Resources/FGItemDescriptor.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "WheeledVehicles/FGWheeledVehicleIdentifier.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "FGInventoryComponent.h"
#include "Patching/NativeHookManager.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "Engine/World.h"

namespace {
// All watches are installed/removed by the game thread. The factory hook
// only compares identities for its own station and appends positive native
// return values under one short lock. It never resolves weak refs, calls
// subsystem methods, enumerates actors or mutates game state off-thread.
struct FWatch {
 const ALogisticsSubsystem* Subsystem=nullptr;
 FGuid Job;
 TWeakObjectPtr<AFGWheeledVehicleIdentifier> Vehicle;
 TWeakObjectPtr<AFGWheeledVehicle> Owner;
 TWeakObjectPtr<UFGInventoryComponent> Cargo;
 TWeakObjectPtr<AFGBuildableDockingStation> Provider,Requester;
 const AActor* OwnerIdentity=nullptr;
 const AFGBuildableDockingStation* SourceIdentity=nullptr;
 const AFGBuildableDockingStation* TargetIdentity=nullptr;
};
struct FReceipt {
 FWatch Watch;
 filn::transfer::DockBatch Batch;
};
// Published by the authoritative game thread. Factory workers copy one
// immutable entry under lock, then operate on the native docking arguments.
// Never hold the registry lock while executing inventory callbacks.
struct FDockFilterEntry {
 const ALogisticsSubsystem* Owner=nullptr;
 TWeakObjectPtr<AFGBuildableDockingStation> Station;
 TWeakObjectPtr<UFGInventoryComponent> Inventory;
 TWeakObjectPtr<UClass> Resource;
};
FCriticalSection DockFilterLock;
TMap<const AFGBuildableDockingStation*,FDockFilterEntry> DockFilters;

FCriticalSection ReceiptLock;
TMap<const UFGInventoryComponent*,FWatch> Watches;
TMap<const ALogisticsSubsystem*,TMap<FGuid,FReceipt>> Receipts;
std::string FILNDockReceiptGuidKey(const FGuid& G) { return TCHAR_TO_UTF8(*G.ToString()); }
void RemoveJobLocked(const ALogisticsSubsystem* Subsystem,const FGuid& Job) {
 for(auto It=Watches.CreateIterator();It;++It)
  if(It.Value().Subsystem==Subsystem && It.Value().Job==Job) It.RemoveCurrent();
 if(auto* Pending=Receipts.Find(Subsystem)) {
  Pending->Remove(Job); if(Pending->IsEmpty()) Receipts.Remove(Subsystem);
 }
}
}
bool ALogisticsSubsystem::PublishDockFilters(const TArray<FFILNStationConfig>& Configs,bool Remove) {
 check(IsInGameThread());
 TMap<const AFGBuildableDockingStation*,FDockFilterEntry> Pending;
 for(const auto& C:Configs) {
  auto* B=IsValid(C.Station)?C.Station->GetStation():nullptr;
  auto* I=IsValid(B)?B->GetInventory():nullptr;
  if(!IsValid(B) || !IsValid(I) || !C.Resource || Pending.Contains(B)) return false;
  FDockFilterEntry E; E.Owner=this; E.Station=B; E.Inventory=I; E.Resource=C.Resource.Get();
  Pending.Add(B,E);
 }
 FScopeLock Lock(&DockFilterLock);
 for(const auto& E:Pending) if(const auto* Existing=DockFilters.Find(E.Key))
  if(Existing->Owner!=this && Existing->Station.IsValid()) return false;
 for(const auto& E:Pending) {
  if(Remove) DockFilters.Remove(E.Key);
  else DockFilters.Add(E.Key,E.Value);
 }
 return true;
}
bool ALogisticsSubsystem::HasDockFilter(const FFILNStationConfig& C) const {
 check(IsInGameThread());
 auto* B=IsValid(C.Station)?C.Station->GetStation():nullptr;
 if(!IsValid(B) || !C.Resource) return false;
 FScopeLock Lock(&DockFilterLock);
 const auto* E=DockFilters.Find(B);
 return E && E->Owner==this && E->Station.Get()==B && E->Inventory.Get()==B->GetInventory() && E->Resource.Get()==C.Resource.Get();
}
void ALogisticsSubsystem::ClearDockFilters() {
 FScopeLock Lock(&DockFilterLock);
 for(auto It=DockFilters.CreateIterator();It;++It) if(It.Value().Owner==this) It.RemoveCurrent();
}
void ALogisticsSubsystem::InstallDockReceiptObserver() {
 if(WITH_EDITOR) return;
 // Parenthesize the complete lambda: the SML macro has exactly two arguments,
 // and the preprocessor does not treat braces as protecting internal commas.
 SUBSCRIBE_METHOD(AFGBuildableDockingStation::Factory_LoadUnloadDockedInventory,
  ([](auto& Scope,const AFGBuildableDockingStation* Station,UFGInventoryComponent* Cargo) {
   if(!Station || Station->GetIsInLoadMode()) return;
   FDockFilterEntry Entry;
   {
    FScopeLock Lock(&DockFilterLock);
    const auto* E=DockFilters.Find(Station);
    if(!E || !E->Station.IsValid(false,true)) return;
    Entry=*E;
   }
   auto* Target=Station->GetInventory();
   // Thread-safe weak identity test only; descriptor is held by the saved config.
   auto* Resource=Entry.Resource.Get(true);
   if(!Cargo || !Target || Cargo==Target || !Entry.Inventory.IsValid(false,true) ||
      Entry.Inventory.Get(true)!=Target || !Resource || !Entry.Resource.IsValid(false,true)) {
    Scope.Override(0); return;
   }
   bool Matching=false,Other=false,Valid=true;
   const int32 Slots=Cargo->GetSizeLinear();
   if(Slots<=0 || Slots>4096) { Scope.Override(0); return; }
   for(int32 Slot=0;Slot<Slots;++Slot) {
    FInventoryStack Stack;
    if(!Cargo->GetStackFromIndex(Slot,Stack) || Stack.NumItems<0 ||
       (Stack.NumItems>0 && !Stack.Item.IsValid())) { Valid=false; break; }
    if(Stack.NumItems>0) {
     if(Stack.Item.GetItemClass().Get()==Resource) Matching=true; else Other=true;
    }
   }
   using A=filn::dockfilter::Action;
   switch(filn::dockfilter::Decide(true,false,Valid,Matching,Other,Station->GetDockingStationResourceForm()==EResourceForm::RF_SOLID)) {
    case A::Native: return; // preserve native transfer rate and fluid handling
    case A::Block: Scope.Override(0); return;
    case A::SelectiveSolid: {
     const auto Transfer=filn::dockfilter::SelectiveSolidUnload(Cargo,Target,Resource);
     if(!Transfer.Verified) UE_LOG(LogFILN,Error,TEXT("FILN_DOCK_FILTER_TRANSFER_MISMATCH; no unverified receipt credited"));
     Scope.Override(Transfer.Amount); return;
    }
   }
  }));
 // SML v3.12.1 after handlers receive the final native operation result.
 // FG header documents result = number of items transferred.
 SUBSCRIBE_METHOD_AFTER(AFGBuildableDockingStation::Factory_LoadUnloadDockedInventory,
  ([](int32 Result,const AFGBuildableDockingStation* Station,UFGInventoryComponent* Cargo) {
   if(Result<=0 || !Station || !Cargo) return;
   FScopeLock Lock(&ReceiptLock);
   const auto* W=Watches.Find(Cargo); if(!W) return; // O(1), unmanaged traffic untouched
   auto& Receipt=Receipts.FindOrAdd(W->Subsystem).FindOrAdd(W->Job);
   if(!Receipt.Watch.Job.IsValid()) Receipt.Watch=*W;
   const bool Added=Station==W->SourceIdentity;
   // These two direct native getters refer to the station whose factory
   // operation just returned; no traversal of game object graphs here.
   if(Station->GetDockedActor()!=W->OwnerIdentity ||
      (!Added && Station!=W->TargetIdentity) || Added!=Station->GetIsInLoadMode()) {
    Receipt.Batch.invalid=true; return;
   }
   Receipt.Batch.Add(Result,Added);
  }));
}
bool ALogisticsSubsystem::ArmDockReceipts(FFILNRegistration& R) {
 check(IsInGameThread());
 auto* V=IsValid(R.Vehicle)?R.Vehicle.Get():nullptr;
 auto* O=V?V->GetOwnerVehicle():nullptr;
 auto* Cargo=IsValid(O)?O->GetStorageInventory():nullptr;
 auto* Source=IsValid(R.Provider)?R.Provider->GetStation():nullptr;
 auto* Target=IsValid(R.Requester)?R.Requester->GetStation():nullptr;
 if(!R.JobId.IsValid() || !R.Automatic || R.AbortReturn || !IsValid(Cargo) || !IsValid(Target) ||
    (!R.RetainedCargo && !IsValid(Source))) return false;
 FWatch W; W.Subsystem=this; W.Job=R.JobId; W.Vehicle=V; W.Owner=O; W.Cargo=Cargo; W.Provider=Source; W.Requester=Target;
 W.OwnerIdentity=O; W.SourceIdentity=Source; W.TargetIdentity=Target;
 FScopeLock Lock(&ReceiptLock);
 if(const auto* Existing=Watches.Find(Cargo)) {
  if(Existing->Subsystem!=this || Existing->Job!=R.JobId) return false;
  if(Existing->OwnerIdentity==O && Existing->SourceIdentity==Source && Existing->TargetIdentity==Target) return true;
 }
 // Owner/inventory replacement is only attempted after exact ledger
 // reconciliation by WatchTransfers. Never carry a receipt across identities.
 RemoveJobLocked(this,R.JobId);
 Watches.Add(Cargo,MoveTemp(W)); R.ReceiptMismatchSince=-1;
 return true;
}
void ALogisticsSubsystem::DisarmDockReceipts(const FGuid& Job) {
 if(!Job.IsValid()) return;
 FScopeLock Lock(&ReceiptLock); RemoveJobLocked(this,Job);
}
void ALogisticsSubsystem::ClearDockReceipts() {
 FScopeLock Lock(&ReceiptLock);
 for(auto It=Watches.CreateIterator();It;++It) if(It.Value().Subsystem==this) It.RemoveCurrent();
 Receipts.Remove(this);
}
bool ALogisticsSubsystem::HasPendingDockReceipts(const FGuid& Job) const {
 FScopeLock Lock(&ReceiptLock);
 const auto* Pending=Receipts.Find(this); return Pending && Pending->Contains(Job);
}
void ALogisticsSubsystem::ConsumeDockReceipts() {
 check(IsInGameThread());
 if(!HasAuthority() || !bRegistryValidated || bPreparingUninstall || !GetWorld()) return;
 TMap<FGuid,FReceipt> Batch;
 { FScopeLock Lock(&ReceiptLock); if(auto* Pending=Receipts.Find(this)) { Batch=MoveTemp(*Pending); Receipts.Remove(this); } }
 const double Now=GetWorld()->GetTimeSeconds();
 for(auto& Entry:Batch) {
  auto& Receipt=Entry.Value; const auto& W=Receipt.Watch; const auto& B=Receipt.Batch;
  auto* V=W.Vehicle.Get(); const auto* Index=V?ByVehicle.Find(V):nullptr;
  if(!Index || !Registrations.IsValidIndex(*Index)) continue;
  auto& R=Registrations[*Index]; using S=filn::State;
  const auto Phase=static_cast<S>(R.State);
  if(R.JobId!=W.Job || !R.Automatic || Phase==S::Idle || Phase==S::Recovery) continue; // stale job, never credit
  auto* O=W.Owner.Get(); auto* Cargo=W.Cargo.Get(); auto* Source=W.Provider.Get(); auto* Target=W.Requester.Get();
  const bool SourceValid=R.RetainedCargo?Source==nullptr:IsValid(R.Provider) && Source && R.Provider->GetStation()==Source;
  const bool TargetValid=IsValid(R.Requester) && Target && R.Requester->GetStation()==Target;
  const bool CanLoad=!B.loaded || (!R.RetainedCargo && (Phase==S::ToProvider || Phase==S::Loading));
  const bool PickupDeparture=Source && O && Source->GetDockedActor()!=O &&
   (B.loaded>0 || filn::recovery::ProviderDepartureProven(Phase==S::Loading,false,R.LoadedCargo,R.LastCargo,R.ExpectedCargo));
  const bool CanUnload=!B.unloaded || Phase==S::ToRequester || Phase==S::Unloading || PickupDeparture;
  if(B.invalid || !SourceValid || !TargetValid || !CanLoad || !CanUnload || R.AbortReturn ||
     !IsValid(O) || !IsValid(Cargo) || V->GetOwnerVehicle()!=O || O->GetStorageInventory()!=Cargo ||
     O->GetDriver() || !V->IsAutopilotEnabled() || V->GetVehicleRoute()!=R.RouteIntent ||
     !R.Resource || !EndpointConfigurationValid(R) || !Reservations.OwnsJob(FILNDockReceiptGuidKey(R.JobId),FILNDockReceiptGuidKey(R.TruckId))) {
   R.CargoOriginKnown=false; R.CargoOriginStorage=false;
   Recover(R,TEXT("Native docking receipt does not match owned job, endpoint or phase")); continue;
  }
  TArray<FInventoryStack> Stacks; Cargo->GetInventoryStacks(Stacks); bool Mixed=false;
  for(const auto& Stack:Stacks) if(Stack.NumItems<0 || (Stack.NumItems>0 && Stack.Item.GetItemClass()!=R.Resource)) Mixed=true;
  filn::transfer::Ledger Ledger{R.LastCargo,R.LoadedCargo,R.DeliveredCargo};
  if(!filn::transfer::ApplyDockBatch(Ledger,B,Cargo->GetNumItems(R.Resource),Mixed)) {
   // A factory worker may have completed the inventory mutation while its
   // after-hook is waiting for the lock. Permit a bounded retry, never infer
   // quantity or credit a mixed/invalid inventory. Merge in original order.
   if(!Mixed && (R.ReceiptMismatchSince<0 || Now-R.ReceiptMismatchSince<1.0)) {
    if(R.ReceiptMismatchSince<0) R.ReceiptMismatchSince=Now;
    FScopeLock Lock(&ReceiptLock);
    auto& Pending=Receipts.FindOrAdd(this).FindOrAdd(W.Job);
    if(Pending.Watch.Job.IsValid()) { Receipt.Batch.Append(Pending.Batch); }
    Pending=MoveTemp(Receipt); continue;
   }
   R.CargoOriginKnown=false; R.CargoOriginStorage=false;
   Recover(R,TEXT("Native docking receipt and actual cargo disagree; no delivery credited")); continue;
  }
  R.ReceiptMismatchSince=-1;
  R.LastCargo=Ledger.cargo; R.LoadedCargo=Ledger.loaded; R.DeliveredCargo=Ledger.delivered;
  R.DepartedHome=true;
  if(B.loaded) {
   const bool Existing=static_cast<int64>(Ledger.cargo)+Ledger.delivered-Ledger.loaded>0;
   const bool Storage=filn::dispatch::IsStorage(TCHAR_TO_UTF8(*R.Provider->GetStationName().ToString()));
   R.CargoOriginKnown=!Existing || R.CargoOriginKnown;
   R.CargoOriginStorage=Storage || (Existing && R.CargoOriginStorage);
   R.State=static_cast<uint8>(S::Loading);
  }
  if(B.unloaded) { R.State=static_cast<uint8>(S::Unloading); if(!R.LastCargo) { R.CargoOriginKnown=false; R.CargoOriginStorage=false; } }
 }
}
