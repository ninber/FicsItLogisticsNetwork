// SPDX-License-Identifier: GPL-3.0-only
#include "LogisticsSubsystem.h"
#include "FILNRepairPolicy.h"
#include "FILNDispatchPolicy.h"
#include "FILNEnrollment.h"
#include "FILNDepotPolicy.h"
#include "FILNRestore.h"
#include "FILNTransferLedger.h"
#include "FILNRegistryPolicy.h"
#include "FILNStationControls.h"
#include "FILNBlueprintSettings.h"
#include "FILNTransferObserver.h"
#include "Resources/FGItemDescriptor.h"
#include "UObject/UnrealType.h"
#include "WheeledVehicles/FGVehicleSubsystem.h"
#include "WheeledVehicles/FGWheeledVehicleIdentifier.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "FGInventoryComponent.h"
#include "WheeledVehicles/FGVehiclePathNode.h"
#include "FGFactoryConnectionComponent.h"
#include "FGPipeConnectionComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogFILN);
namespace {
using S = filn::State;
std::string Key(const FGuid& Id) { return TCHAR_TO_UTF8(*Id.ToString()); }
S StateOf(const FFILNRegistration& R) { return static_cast<S>(R.State); }
void SetState(FFILNRegistration& R,S State) { R.State=static_cast<uint8>(State); }
AFGBuildableDockingStation* Building(AFGDockingStationIdentifier* S) { return IsValid(S)? S->GetStation():nullptr; }
}
ALogisticsSubsystem::ALogisticsSubsystem() {
 ReplicationPolicy=ESubsystemReplicationPolicy::SpawnOnServer;
 PrimaryActorTick.bCanEverTick=false;
}
bool ALogisticsSubsystem::ShouldSave_Implementation() const {
 // A prepared, empty subsystem has no state to serialize into vanilla saves.
 // Keep invalid/conflicted metadata for diagnosis instead of dropping it.
 return SchemaVersion!=1 || bDispatchBlocked || (!bUninstallPrepared && bAutoDispatchEnabled) || !Registrations.IsEmpty() ||
  !StationConfigs.IsEmpty() || !DepotStations.IsEmpty() || !Stations.IsEmpty() || (!bUninstallPrepared && (!NetworkNameCounters.IsEmpty() || !PendingDepotMembership.IsEmpty() || NetworkSwatch || !NetworkPaint.IsEmpty()));
}
bool ALogisticsSubsystem::SetDispatchEnabled(bool Enabled) {
 if(!HasAuthority() || (Enabled && (!bRegistryValidated || bDispatchBlocked || bPreparingUninstall || bChangingFilters))) return false;
 bAutoDispatchEnabled=Enabled; if(Enabled) bUninstallPrepared=false; ResetScan(); return true;
}
void ALogisticsSubsystem::BeginPlay() {
 Super::BeginPlay();
 if(!HasAuthority()) return;
 UE_LOG(LogFILN,Log,TEXT("FILN_NATIVE_WORLD_BEGIN actor=%s world=%s"),*GetPathName(),*GetWorld()->GetPathName());
 RefreshNativeDashboardHeartbeat();
 Rebuild();
 GetWorldTimerManager().SetTimer(DispatchTimer,this,&ALogisticsSubsystem::Schedule,0.1f,true);
 GetWorldTimerManager().SetTimer(TelemetryTimer,this,&ALogisticsSubsystem::SampleTelemetry,0.1f,true);
 GetWorldTimerManager().SetTimer(Watchdog,this,&ALogisticsSubsystem::Observe,1.f,true);
 UE_LOG(LogFILN,Log,TEXT("M1 subsystem ready; schema=%d registrations=%d. Runtime acceptance unverified."),SchemaVersion,Registrations.Num());
}
void ALogisticsSubsystem::EndPlay(const EEndPlayReason::Type Reason) {
 ClearDockReceipts(); ClearDockFilters();
 GetWorldTimerManager().ClearTimer(Watchdog);
 GetWorldTimerManager().ClearTimer(DispatchTimer);
 GetWorldTimerManager().ClearTimer(TelemetryTimer);
 GetWorldTimerManager().ClearTimer(DeferredRebuild); bRegistryValidated=false; RegistryDiagnostics.Reset(); DestroyedVehicles.Empty();
 StopNativeDashboardHeartbeat();
 ResetTelemetry();
 for(const auto& Observer:TransferObservers) if(Observer) Observer->Unbind();
 TransferObservers.Empty(); TransferByTruck.Empty();
 Super::EndPlay(Reason);
}
void ALogisticsSubsystem::PostLoadGame_Implementation(int32,int32) {
 // Rebuild validates saved identities before it touches native routes or filters.
 // Do not normalize unknown states during deserialization.
 bRegistryValidated=false; bDispatchBlocked=true;
 if(HasAuthority() && HasActorBegunPlay() && GetWorld() && !GetWorld()->bIsTearingDown) {
  GetWorldTimerManager().ClearTimer(DeferredRebuild);
  DeferredRebuild=GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this,&ALogisticsSubsystem::Rebuild));
 }
}
FGuid ALogisticsSubsystem::StationId(AFGDockingStationIdentifier* Station) {
 if(!IsValid(Station) || Station->GetWorld()!=GetWorld()) return FGuid();
 const TWeakObjectPtr<AFGDockingStationIdentifier> Ref(Station);
 if(const auto* Id=StationIds.Find(Ref)) return *Id;
 FFILNStation Entry;
 do { Entry.InstanceId=FGuid::NewGuid(); } while(!Entry.InstanceId.IsValid() || StationIdentityIndex.Contains(Entry.InstanceId));
 Entry.Station=Station;
 Stations.Add(Entry); StationIds.Add(Ref,Entry.InstanceId); StationIdentityIndex.Add(Entry.InstanceId);
 return Entry.InstanceId;
}
bool ALogisticsSubsystem::RestoreJobReservation(FFILNRegistration& R) {
 if(!R.JobId.IsValid() || !R.TruckId.IsValid()) return false;
 const auto Resolve=[&](AFGDockingStationIdentifier* Station,FGuid& Saved) {
  const bool Live=IsValid(Station);
  const auto* Existing=Live?StationIds.Find(TWeakObjectPtr<AFGDockingStationIdentifier>(Station)):nullptr;
  // Earlier native saves may lack redundant lease fields. Recover only from a
  // pre-existing validated registry entry, never allocate an ID during restore.
  if(!Saved.IsValid() && Existing) Saved=*Existing;
  return filn::registry::LeaseReference(Saved.IsValid() && StationIdentityIndex.Contains(Saved),Live,Existing && *Existing==Saved);
 };
 if(!Resolve(R.Requester,R.RequesterLease)) return false;
 if(R.RetainedCargo) {
  if(IsValid(R.Provider) || R.ProviderLease.IsValid()) return false;
  return Reservations.AcquireDelivery(Key(R.JobId),Key(R.TruckId),Key(R.RequesterLease));
 }
 if(!Resolve(R.Provider,R.ProviderLease)) return false;
 return Reservations.Acquire(Key(R.JobId),Key(R.TruckId),Key(R.ProviderLease),Key(R.RequesterLease));
}
void ALogisticsSubsystem::Rebuild() {
 if(!HasAuthority() || !GetWorld() || GetWorld()->bIsTearingDown) return;
 bRegistryValidated=false; DestroyedVehicles.Empty();
 ClearDockReceipts(); ClearDockFilters(); ResetTelemetry();
 for(const auto& Observer:TransferObservers) if(Observer) Observer->Unbind();
 TransferObservers.Empty(); TransferByTruck.Empty();
 Ownership=filn::Ownership(); Reservations=filn::Reservations();
 HomeStations.Empty(); DepotIndex.Empty();
 ByVehicle.Empty(); StationIds.Empty(); StationIdentityIndex.Empty(); Active.Empty(); Cursor=0;
 RoadCache.Clear(); ResetScan();
 // Reconcile independent records; no station/job error can disable the network.
 bDispatchBlocked=false;
 RegistryDiagnostics.Reset();
 const auto RegistryIssue=[this](const FString& Why) { RegistryDiagnostics.AddUnique(Why); UE_LOG(LogFILN,Warning,TEXT("FILN_REGISTRY_DETAIL %s"),*Why); };
 if(SchemaVersion!=1) { RegistryIssue(FString::Printf(TEXT("Rebuilding known fields from saved schema %d"),SchemaVersion)); SchemaVersion=1; }
 // Reconstruct the live station identity index, retaining unique tombstone IDs.
 TArray<FFILNStation> CleanStations;
 for(auto E:Stations) {
  const bool Live=IsValid(E.Station) && E.Station->GetWorld()==GetWorld();
  if(!Live) E.Station=nullptr;
  const TWeakObjectPtr<AFGDockingStationIdentifier> Ref(E.Station.Get());
  if(Live && StationIds.Contains(Ref)) continue;
  if(!E.InstanceId.IsValid() || StationIdentityIndex.Contains(E.InstanceId)) {
   if(!Live) continue;
   E.InstanceId=FGuid::NewGuid();
  }
  StationIdentityIndex.Add(E.InstanceId);
  if(Live) StationIds.Add(Ref,E.InstanceId);
  CleanStations.Add(E);
 }
 Stations=MoveTemp(CleanStations);
 TArray<TObjectPtr<AFGDockingStationIdentifier>> CleanDepots;
 for(const auto& D:DepotStations) if(IsValid(D) && D->GetWorld()==GetWorld()) {
  const TWeakObjectPtr<AFGDockingStationIdentifier> Ref(D.Get());
  if(DepotIndex.Contains(Ref)) continue;
  StationId(D); DepotIndex.Add(Ref); CleanDepots.Add(D);
 }
 DepotStations=MoveTemp(CleanDepots);
 // First accepted owner per exclusive live bay. Duplicate records of the same
 // actor are discarded as metadata only; competing vehicles return to discovery.
 TArray<FFILNRegistration> Saved=MoveTemp(Registrations);
 Registrations.Reset();
 TSet<FGuid> TruckIds;
 for(auto& R:Saved) {
  auto* V=R.Vehicle.Get(); auto* H=R.Home.Get();
  if(!IsValid(V) || V->GetWorld()!=GetWorld()) { RegistryIssue(TEXT("Removed stale vehicle registration; no actor mutation")); continue; }
  if(ByVehicle.Contains(V)) { RegistryIssue(FString::Printf(TEXT("Duplicate saved vehicle record removed: %s"),*V->GetPathName())); continue; }
  if(!IsValid(H) || H->GetWorld()!=GetWorld() || HomeStations.Contains(TWeakObjectPtr<AFGDockingStationIdentifier>(H))) {
   PendingDepotMembership.AddUnique(V); PendingHomeReason.Add(V,TEXT("Saved home unavailable or already owned; rediscovery pending"));
   RegistryIssue(FString::Printf(TEXT("Home claim requires rediscovery: %s"),*V->GetPathName())); continue;
  }
  if(!R.TruckId.IsValid() || TruckIds.Contains(R.TruckId)) R.TruckId=FGuid::NewGuid();
  R.BayId=StationId(H);
  if(!Ownership.Pair(Key(R.TruckId),Key(R.BayId))) { PendingDepotMembership.AddUnique(V); continue; }
  TruckIds.Add(R.TruckId); HomeStations.Add(H); TrackDepot(H);
  ByVehicle.Add(V,Registrations.Add(R));
 }
 // Keep the configured resource independent of slot layout. Restore each
 // station separately, retry incomplete native readback in the watchdog.
 TSet<AFGDockingStationIdentifier*> ConfigActors;
 for(int32 I=0;I<StationConfigs.Num();) {
  auto& C=StationConfigs[I];
  if(!IsValid(C.Station) || C.Station->GetWorld()!=GetWorld() || IsDepot(C.Station)) {
   RegistryIssue(TEXT("Removed stale or depot cargo configuration")); StationConfigs.RemoveAt(I); continue;
  }
  if(ConfigActors.Contains(C.Station)) {
   RegistryIssue(FString::Printf(TEXT("Duplicate station config removed: %s"),*C.Station->GetPathName())); StationConfigs.RemoveAt(I); continue;
  }
  auto* B=Building(C.Station);
  if(IsValid(B) && B->IsBuildableInsideBlueprintDesigner()) { StationConfigs.RemoveAt(I); continue; }
  ConfigActors.Add(C.Station); StationId(C.Station);
  if(FFILNStationSettings::HasProperty(B) && !C.ExternalFilter) {
   if(const auto Saved=FFILNStationSettings::Read(B)) C.Resource=Saved;
   else if(C.Resource) FFILNStationSettings::Write(B,C.Resource);
  }
  if(!C.ExternalFilter && C.Resource && ValidateStationSlots(C)) {
   if(!ApplyStationFilter(C)) RegistryIssue(FString::Printf(TEXT("Station filter validation pending: %s"),*C.Station->GetPathName()));
  } else RegistryIssue(FString::Printf(TEXT("Station filter validation pending: %s"),*C.Station->GetPathName()));
  ++I;
 }
 FilterRepairCursor=0;
 // Recreate reservations only for jobs with consistent current actor identity.
 // Invalid jobs are retired to native parking with their real cargo intact.
 for(auto& R:Registrations) {
  if(StateOf(R)==S::Idle && !R.JobId.IsValid()) continue;
  if(IsValid(R.Provider)) R.ProviderLease=StationId(R.Provider);
  if(IsValid(R.Requester)) R.RequesterLease=StationId(R.Requester);
  if(!filn::registry::StateAndJob(R.State,R.JobId.IsValid()) || !RestoreJobReservation(R)) {
   RetireJobToParking(R,TEXT("Saved job could not be restored; returning with actual cargo"));
  }
 }
 bRegistryValidated=true;
 UE_LOG(LogFILN,Log,TEXT("FILN_REGISTRY_REBUILT identities=%d configs=%d depots=%d vehicles=%d"),Stations.Num(),StationConfigs.Num(),DepotStations.Num(),Registrations.Num());
 bAutoDispatchEnabled=!bPreparingUninstall && !bUninstallPrepared;
 bDepotRecipeReady=false; NextDepotRecipeCheck=0; DepotRecipeFailures.Reset();
 PlayerStationScan.Empty(); PlayerVehicleScan.Empty(); PlayerStationCursor=PlayerVehicleCursor=0;
 NextPlayerStationScan=NextPlayerVehicleScan=0; OverflowCursor.Empty();
 for(const auto& Depot:DepotStations) if(IsValid(Depot)) {
  if(auto* B=Building(Depot); IsValid(B) && !B->FindComponentByClass<UFILNStationMarker>()) NewObject<UFILNStationMarker>(B,NAME_None,RF_Transient)->RegisterComponent();
 }
 for(const auto& C:StationConfigs) if(IsValid(C.Station)) {
  if(auto* B=Building(C.Station); IsValid(B) && !B->FindComponentByClass<UFILNStationMarker>()) NewObject<UFILNStationMarker>(B,NAME_None,RF_Transient)->RegisterComponent();
 }
 for(int32 I=0;I<Registrations.Num();++I) {
  auto& R=Registrations[I];
  if(IsValid(R.Home)) TrackDepot(R.Home);
  const bool Observed=IsValid(R.Vehicle) && WatchTransfers(R);
  auto* V=IsValid(R.Vehicle)?R.Vehicle.Get():nullptr;
  auto* O=V?V->GetOwnerVehicle():nullptr;
  if(StateOf(R)==S::Idle) {
   // Saved provenance cannot survive a discrepancy in the saved cargo ledger.
   if(R.CargoOriginKnown) {
    auto* Cargo=IsValid(O)?O->GetStorageInventory():nullptr;
    bool Verified=Observed && Cargo && R.Resource && R.LastCargo>0 && Cargo->GetNumItems(R.Resource)==R.LastCargo;
    if(Verified) {
     TArray<FInventoryStack> Stacks; Cargo->GetInventoryStacks(Stacks);
     for(const auto& Stack:Stacks) if(Stack.NumItems<0 || (Stack.NumItems>0 && Stack.Item.GetItemClass()!=R.Resource)) Verified=false;
    }
    if(!Verified) { R.CargoOriginKnown=false; R.CargoOriginStorage=false; }
   }
   continue;
  }
  // Earlier builds could persist a provider-less retained-cargo job. Do not
  // resume that obsolete delivery or delete its cargo. Convert the owned
  // native route to a parking transition; transition-node exclusion protects
  // its old endpoints until the truck leaves that route.
  if(R.RetainedCargo && R.JobId.IsValid() && IsValid(O) && IsValid(V) && IsValid(R.Home) &&
     V->GetVehicleRoute()==R.RouteIntent && Reservations.OwnsJob(Key(R.JobId),Key(R.TruckId))) {
   DisarmDockReceipts(R.JobId); Reservations.Release(Key(R.JobId));
   R.IdleRoute=V->GetVehicleRoute(); R.RouteIntent.Empty(); R.JobId.Invalidate();
   R.Provider=nullptr; R.Requester=nullptr; R.ProviderLease.Invalidate(); R.RequesterLease.Invalidate();
   R.RetainedCargo=false; R.AbortReturn=false; R.DepartedHome=false; R.Held=false; R.Relocating=true;
   R.AwaitingHomeIntent=!R.IdleRoute.Contains(R.Home->GetPathNodeGUID()); R.State=static_cast<uint8>(S::Idle);
   if(!Observed) { R.CargoOriginKnown=false; R.CargoOriginStorage=false; }
   R.NavigationRetry.Reset(); R.TravelProgress.Reset();
   R.Diagnostic=TEXT("Obsolete provider-less saved job retired; returning with actual cargo");
   UE_LOG(LogFILN,Log,TEXT("FILN_OBSOLETE_DIRECT_JOB_RETIRED truck=%s"),*R.TruckId.ToString()); continue;
  }
  const bool Resume=Observed && R.Automatic && R.Resource && R.ExpectedCargo>0 && IsValid(O) && !O->GetDriver() && V->IsAutopilotEnabled() && IsValid(R.Home) &&
    filn::repair::CanRestoreJob(R.State>=static_cast<uint8>(S::ToProvider) && R.State<=static_cast<uint8>(S::Returning),Reservations.OwnsJob(Key(R.JobId),Key(R.TruckId)),V->GetVehicleRoute()==R.RouteIntent,R.AbortReturn || EndpointConfigurationValid(R));
  if(Resume && (R.AbortReturn || ArmDockReceipts(R))) { Active.Add(I); R.LastObserved=GetWorld()->GetTimeSeconds(); if(!R.AbortReturn && IsValid(R.Provider)) KnownNetworkProviders.AddUnique(R.Provider); }
  else RetireJobToParking(R,TEXT("Saved journey cannot resume; parking with actual cargo"));
 }
}
void ALogisticsSubsystem::RequestRegistryRepair() {
 if(!HasAuthority() || !GetWorld() || GetWorld()->bIsTearingDown) return;
 // A damaged runtime index is rebuilt on the next game-thread tick, never latched.
 if(!GetWorldTimerManager().IsTimerActive(DeferredRebuild))
  DeferredRebuild=GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this,&ALogisticsSubsystem::Rebuild));
}
void ALogisticsSubsystem::RetireJobToParking(FFILNRegistration& R,const FString& Why) {
 if(R.JobId.IsValid() && Reservations.OwnsJob(Key(R.JobId),Key(R.TruckId))) {
  DisarmDockReceipts(R.JobId); Reservations.Release(Key(R.JobId));
 }
 auto* V=R.Vehicle.Get();
 const bool OwnedRoute=IsValid(V) && V->GetVehicleRoute()==R.RouteIntent;
 R.IdleRoute=IsValid(V)?V->GetVehicleRoute():TArray<FGuid>();
 R.AwaitingHomeIntent=OwnedRoute && IsValid(R.Home) && !R.IdleRoute.Contains(R.Home->GetPathNodeGUID());
 R.RouteIntent.Reset(); R.JobId.Invalidate(); R.Provider=nullptr; R.Requester=nullptr;
 R.ProviderLease.Invalidate(); R.RequesterLease.Invalidate();
 R.State=static_cast<uint8>(S::Idle); R.Held=false; R.Relocating=true;
 R.RetainedCargo=false; R.AbortReturn=false; R.DepartedHome=false;
 R.CargoOriginKnown=false; R.CargoOriginStorage=false;
 R.NavigationRetry.Reset(); R.TravelProgress.Reset(); R.ReceiptMismatchSince=-1;
 R.Diagnostic=Why;
 UE_LOG(LogFILN,Warning,TEXT("FILN_JOB_RETIRED truck=%s %s"),*R.TruckId.ToString(),*Why);
 ResetScan();
}
bool ALogisticsSubsystem::ValidateStationSlots(const FFILNStationConfig& C) {
 auto* B=Building(C.Station); auto* I=IsValid(B)?B->GetInventory():nullptr;
 if(!IsValid(I) || !C.Resource || C.ExternalFilter || I->GetSizeLinear()!=C.OriginalFilters.Num() ||
    I->GetSizeLinear()<=0 || I->GetSizeLinear()>4096) return false;
 // Normal own-state validation only. Never rewrite physical slots on load,
 // selection or retry. Any inconsistent private save must be fixed offline.
 for(int32 Slot=0;Slot<I->GetSizeLinear();++Slot)
  if(!filn::repair::SlotMatches(C.OriginalFilters[Slot].Get(),I->GetAllowedItemOnIndex(Slot).Get())) return false;
 return true;
}
void ALogisticsSubsystem::RetryStationFilters() {
 if(bChangingFilters || bPreparingUninstall || bUninstallPrepared) return;
 for(int32 Work=0;Work<FMath::Min(4,StationConfigs.Num());++Work) {
  FilterRepairCursor%=StationConfigs.Num(); const auto& C=StationConfigs[FilterRepairCursor++];
  if(!IsValid(C.Station) || !C.Resource || C.ExternalFilter || StationResourceMatches(C)) continue;
  if(ValidateStationSlots(C) && ApplyStationFilter(C)) {
   const FString Pending=FString::Printf(TEXT("Station filter validation pending: %s"),*C.Station->GetPathName());
   const FString Prefix=FString::Printf(TEXT("Filter preflight %s:"),*GetNameSafe(C.Station.Get()));
   RegistryDiagnostics.RemoveAll([&](const FString& D){return D==Pending || D.StartsWith(Prefix);});
   ResetScan();
  }
 }
}
bool ALogisticsSubsystem::ApplyStationFilter(const FFILNStationConfig& C,TSubclassOf<UFGItemDescriptor> PriorResource) {
 return ChangeStationFilters(TArray<FFILNStationConfig>{C},false,PriorResource);
}
bool ALogisticsSubsystem::ChangeStationFilters(const TArray<FFILNStationConfig>& Configs,bool Restore,TSubclassOf<UFGItemDescriptor> PriorResource) {
 check(IsInGameThread());
 if(!HasAuthority() || bChangingFilters) return false;
 TGuardValue<bool> Guard(bChangingFilters,true);
 TSet<UFGInventoryComponent*> Seen;
 for(const auto& C:Configs) {
  auto* B=Building(C.Station); auto* I=IsValid(B)?B->GetInventory():nullptr;
  const auto Fail=[&](const TCHAR* Why) { const FString Detail=FString::Printf(TEXT("Filter preflight %s: %s"),*GetNameSafe(C.Station.Get()),Why); if(!RegistryDiagnostics.Contains(Detail)) { RegistryDiagnostics.Add(Detail); UE_LOG(LogFILN,Error,TEXT("FILN_FILTER_PREFLIGHT %s"),*Detail); } return false; };
  if(C.ExternalFilter || !IsValid(I) || !C.Resource || Seen.Contains(I) || I->GetSizeLinear()<=0 || I->GetSizeLinear()>4096 ||
     I->GetSizeLinear()!=C.OriginalFilters.Num() || (Restore && B->GetDockedActor())) return Fail(TEXT("external/invalid/duplicate inventory, slot count mismatch or occupied restore"));
  Seen.Add(I);
  if(!Restore) {
   if(!StationAcceptsResource(B,C.Resource)) return Fail(TEXT("native resource form or fluid descriptor mismatch"));
   // Existing cargo is not a filter-setting failure. It remains untouched;
   // scheduler eligibility separately checks actual stock.
  }
  // FILN publishes a docking resource filter; it never rewrites physical slot restrictions.
  for(int32 Slot=0;Slot<I->GetSizeLinear();++Slot) {
   if(I->GetAllowedItemOnIndex(Slot)!=C.OriginalFilters[Slot]) {
    UE_LOG(LogFILN,Error,TEXT("FILN_SLOT_LAYOUT_CONFLICT station=%s slot=%d; no inventory mutation"),*B->GetPathName(),Slot);
    return Fail(TEXT("native slot restriction differs from original saved layout"));
   }
  }
 }
 return PublishDockFilters(Configs,Restore);
}
bool ALogisticsSubsystem::ConfigureStation(AFGDockingStationIdentifier* Station,TSubclassOf<UFGItemDescriptor> Resource) {
 auto* B=Building(Station); auto* Inventory=IsValid(B)?B->GetInventory():nullptr;
 if(bDispatchBlocked || !bRegistryValidated || bChangingFilters || bPreparingUninstall || !HasAuthority() || !IsValid(Station) || Station->GetWorld()!=GetWorld() || !IsValid(Inventory) || !Resource || Inventory->GetSizeLinear()<=0 || Inventory->GetSizeLinear()>4096 || B->GetDockedActor()) return false;
 if(IsDepot(Station) || B->IsBuildableInsideBlueprintDesigner() || Reservations.StationBusy(Key(StationId(Station))) || !StationAcceptsResource(B,Resource)) return false;
 if(FFILNStationSettings::IsDedicated(B) && !FFILNStationSettings::HasProperty(B)) return false;
 // Explicit selection, never inference from stock. Native cargo and slot
 // descriptors remain untouched; only the docking filter is published.
 FFILNStationConfig* Existing=StationConfigs.FindByPredicate([Station](const FFILNStationConfig& C){return C.Station==Station;});
 if(Existing) {
  if(Existing->ExternalFilter || !ValidateStationSlots(*Existing)) return false;
  // Preserve native slot restrictions and publish only the new docking selection.
  for(const auto& Original:Existing->OriginalFilters) if(Original && Original!=Resource) return false;
  ResetScan();
  FFILNStationConfig Updated=*Existing; Updated.Resource=Resource;
  if(!ApplyStationFilter(Updated,Existing->Resource)) return false;
  *Existing=Updated;
  return !FFILNStationSettings::IsDedicated(B) || FFILNStationSettings::Write(B,Resource);
 }
 FFILNStationConfig C; C.Station=Station; C.Resource=Resource;
 for(int32 I=0;I<Inventory->GetSizeLinear();++I) {
  const auto Original=Inventory->GetAllowedItemOnIndex(I);
  if(Original && Original!=Resource) return false;
  C.OriginalFilters.Add(Original);
 }
 if(!ApplyStationFilter(C)) return false;
 StationConfigs.Add(C);
 if(!B->FindComponentByClass<UFILNStationMarker>()) NewObject<UFILNStationMarker>(B,NAME_None,RF_Transient)->RegisterComponent();
 ResetScan(); return !FFILNStationSettings::IsDedicated(B) || FFILNStationSettings::Write(B,Resource);
}
bool ALogisticsSubsystem::RestoreStationFilters(const TArray<int32>& Indices) {
 TArray<FFILNStationConfig> Configs; TSet<int32> Seen;
 for(int32 Index:Indices) {
  if(!StationConfigs.IsValidIndex(Index) || Seen.Contains(Index)) return false;
  Seen.Add(Index); if(!StationConfigs[Index].ExternalFilter) Configs.Add(StationConfigs[Index]);
 }
 return ChangeStationFilters(Configs,true);
}
bool ALogisticsSubsystem::PrepareStationsForUninstall() {
 check(IsInGameThread());
 if(!HasAuthority() || !bRegistryValidated || bDispatchBlocked || bPreparingUninstall || bChangingFilters ||
    Reservations.Size()!=0 || !Active.IsEmpty()) return false;
 // Explicit preparation requires a quiet fleet. Keep all physical routes,
 // inventories and connections; release only FILN metadata and restrictions.
 for(const auto& R:Registrations) {
  if(StateOf(R)!=S::Idle || R.JobId.IsValid() || R.FuelPending || !IsValid(R.Vehicle) || !IsValid(R.Home)) return false;
  auto* O=R.Vehicle->GetOwnerVehicle(); auto* H=Building(R.Home);
  if(!IsValid(O) || !IsValid(H) || !H->GetDockingPathNode() || O->GetDriver()) return false;
  const FVector Delta=O->GetVehicleLocation()-H->GetDockingPathNode()->GetActorLocation();
  if(!filn::dispatch::HomePositionReady(Delta.SizeSquared(),Delta.Z,O->GetForwardSpeed(),H->GetDockedActor() && H->GetDockedActor()!=O)) return false;
  const TArray<FGuid> HomeRoute={R.Home->GetPathNodeGUID()};
  if(R.Vehicle->IsAutopilotEnabled() && (!R.Held || R.IdleRoute!=HomeRoute || R.Vehicle->GetVehicleRoute()!=HomeRoute)) return false;
 }
 // Restore optional cosmetic state before changing routes or inventory filters.
 // If an actor is unavailable, keep the operational network intact.
 if(!RestoreAllNetworkPaint()) return false;
 NetworkSwatch=nullptr;
 TGuardValue<bool> Preparing(bPreparingUninstall,true);
 SetDispatchEnabled(false);
 // Removing the AP-tick hook must not release a parked truck into traffic.
 // This is an explicit administrator action; never touch a foreign route.
 // If a later step fails, leave previously stopped trucks AP-off, retaining
 // their ownership/configuration. Never silently turn AP back on.
 for(auto& R:Registrations) if(R.Vehicle->IsAutopilotEnabled()) {
  auto* V=R.Vehicle.Get(); auto* O=V->GetOwnerVehicle();
  if(!IsValid(O) || O->GetDriver() || V->GetVehicleRoute()!=R.IdleRoute || FMath::Abs(O->GetForwardSpeed())>1.f) return false;
  V->SetAutopilotEnabled(false);
  if(V->IsAutopilotEnabled() || V->GetVehicleRoute()!=R.IdleRoute || O->GetDriver()) return false;
  R.Held=false;
 }
 TArray<int32> Indices;
 for(int32 I=0;I<StationConfigs.Num();++I) Indices.Add(I);
 if(!RestoreStationFilters(Indices)) return false;
 for(const auto& C:StationConfigs) if(auto* B=Building(C.Station); IsValid(B)) {
  if(FFILNStationSettings::HasProperty(B)) FFILNStationSettings::Write(B,nullptr);
  if(auto* Marker=B->FindComponentByClass<UFILNStationMarker>()) Marker->DestroyComponent();
 }
 for(const auto& Depot:DepotStations) if(auto* B=Building(Depot); IsValid(B)) {
  if(auto* Marker=B->FindComponentByClass<UFILNStationMarker>()) Marker->DestroyComponent();
 }
 StationConfigs.Empty(); Registrations.Empty(); Stations.Empty(); DepotStations.Empty();
 NamedNetworkStations.Empty(); NamedNetworkVehicles.Empty(); KnownNetworkProviders.Empty(); NetworkNameCounters.Empty();
 bUninstallPrepared=true; Rebuild();
 return bRegistryValidated && !bDispatchBlocked;
}
bool ALogisticsSubsystem::IsDepot(AFGDockingStationIdentifier* Station) const {
 if(!IsValid(Station)) return false;
 return IsAuthoredDepot(Station) || HomeStations.Contains(TWeakObjectPtr<AFGDockingStationIdentifier>(Station)) || DepotIndex.Contains(TWeakObjectPtr<AFGDockingStationIdentifier>(Station));
}
void ALogisticsSubsystem::TrackDepot(AFGDockingStationIdentifier* Station) {
 if(!IsValid(Station)) return;
 const TWeakObjectPtr<AFGDockingStationIdentifier> Ref(Station);
 if(!DepotIndex.Contains(Ref)) { StationId(Station); DepotIndex.Add(Ref); DepotStations.Add(Station); }
 if(auto* B=Building(Station); IsValid(B) && !B->FindComponentByClass<UFILNStationMarker>()) NewObject<UFILNStationMarker>(B,NAME_None,RF_Transient)->RegisterComponent();
}
bool ALogisticsSubsystem::SetDepot(AFGDockingStationIdentifier* Station,bool Remove) {
 check(IsInGameThread());
 if(!HasAuthority() || !bRegistryValidated || bDispatchBlocked || bPreparingUninstall || bChangingFilters || !IsValid(Station) || Station->GetWorld()!=GetWorld()) return false;
 auto* B=Building(Station); if(!IsValid(B)) return false;
 const bool Claimed=HomeStations.Contains(TWeakObjectPtr<AFGDockingStationIdentifier>(Station));
 const bool Configured=StationConfigs.ContainsByPredicate([Station](const FFILNStationConfig& C){return C.Station==Station;});
 const bool Reserved=Reservations.StationBusy(Key(StationId(Station)));
 const filn::depot::Snapshot Facts{true,Station->GetPathNodeGUID().IsValid() && ValidHome(Station),Reserved,Configured,Claimed,B->GetDockedActor()!=nullptr};
 if(Remove) {
  if(!filn::depot::CanRemove(Facts)) return false;
  DepotIndex.Remove(TWeakObjectPtr<AFGDockingStationIdentifier>(Station));
  DepotStations.RemoveAll([Station](const auto& D){return D==Station;});
  if(auto* Marker=B->FindComponentByClass<UFILNStationMarker>()) Marker->DestroyComponent();
 } else {
  if(!filn::depot::CanDesignate(Facts)) return false;
  if(Claimed && B->GetDockedActor()) {
   const auto* R=Registrations.FindByPredicate([Station](const FFILNRegistration& E){return E.Home==Station;});
   if(!R || !IsValid(R->Vehicle) || R->Vehicle->GetOwnerVehicle()!=B->GetDockedActor()) return false;
  }
  TrackDepot(Station);
 }
 ResetScan(); return true;
}
bool ALogisticsSubsystem::ValidHome(AFGDockingStationIdentifier* Home) const {
 auto* B=Building(Home);
 if(!IsValid(B) || !B->GetDockingPathNode() || !B->GetIsInLoadMode() || !B->GetInventory() || !B->GetInventory()->IsEmpty()) return false;
 // Port identities are native reflected components, not connector names.
 TSet<const UFGFactoryConnectionComponent*> FuelPorts;
 const auto* FuelProperty=FindFProperty<FArrayProperty>(B->GetClass(),TEXT("mFuelConnections"));
 const auto* FuelInner=FuelProperty?CastField<FObjectPropertyBase>(FuelProperty->Inner):nullptr;
 if(FuelProperty && FuelInner) {
  FScriptArrayHelper Ports(FuelProperty,FuelProperty->ContainerPtrToValuePtr<void>(B));
  for(int32 I=0;I<Ports.Num();++I)
   if(const auto* Port=Cast<UFGFactoryConnectionComponent>(FuelInner->GetObjectPropertyValue(Ports.GetRawPtr(I)))) FuelPorts.Add(Port);
 }
 TArray<UFGFactoryConnectionComponent*> Belts; B->GetComponents(Belts);
 for(const auto* Connection:Belts) {
  if(!IsValid(Connection)) return false;
  if(!Connection->IsConnected()) continue;
  auto* Inventory=Connection->GetInventory();
  if(!filn::enrollment::ConnectedPortAllowed(Inventory==B->GetInventory(),
     Inventory && Inventory==B->GetFuelInventory(),FuelPorts.Contains(Connection))) return false;
 }
 TArray<UFGPipeConnectionComponent*> Pipes; B->GetComponents(Pipes);
 for(const auto* Connection:Pipes) if(Connection->IsConnected()) return false;
 return true;
}
bool ALogisticsSubsystem::Parked(const FFILNRegistration& R) const {
 auto* Home=Building(R.Home);
 return IsValid(R.Vehicle) && IsValid(R.Vehicle->GetOwnerVehicle()) && IsValid(Home) &&
  Home->GetDockedActor()==R.Vehicle->GetOwnerVehicle() && !R.Vehicle->IsAutopilotEnabled();
}
void ALogisticsSubsystem::Recover(FFILNRegistration& R,const FString& Why) {
 RetireJobToParking(R,Why);
}
bool ALogisticsSubsystem::Pair(AFGWheeledVehicleIdentifier* V,AFGDockingStationIdentifier* H) {
 using namespace filn::enrollment;
 check(IsInGameThread());
 if(!HasAuthority() || !bRegistryValidated || bDispatchBlocked || bPreparingUninstall || bChangingFilters || !IsValid(V) || !IsValid(H) ||
    V->GetWorld()!=GetWorld() || H->GetWorld()!=GetWorld() || !ValidHome(H) || !H->GetPathNodeGUID().IsValid()) return false;
 if(const auto* Existing=ByVehicle.Find(V)) {
  if(!Registrations.IsValidIndex(*Existing)) return false;
  const auto& R=Registrations[*Existing];
  return R.Home==H && StateOf(R)==S::Idle && R.Held && V->IsAutopilotEnabled() && V->GetVehicleRoute()==R.IdleRoute && HasTransferObserver(R);
 }
 for(const auto& R:Registrations) if(R.Home==H || (StateOf(R)!=S::Idle && (R.Provider==H || R.Requester==H))) return false;
 for(const auto& C:StationConfigs) if(C.Station==H) return false;
 auto* VehicleActor=V->GetOwnerVehicle(); auto* Home=Building(H);
 auto* VS=AFGVehicleSubsystem::Get(GetWorld());
 if(!IsValid(VehicleActor) || !VehicleActor->HasAuthority() || !VS || !VehicleActor->GetStorageInventory() || !Home->GetDockingPathNode() || !VehicleStationCompatible(VehicleActor,Home)) return false;
 const FGuid Node=H->GetPathNodeGUID();
 const FVector HomeLocation=Home->GetDockingPathNode()->GetActorLocation();
 FVector Position; FRotator Rotation; VehicleActor->GetVehicleLocationAndRotation(Position,Rotation);
 const auto Near=[](const FVector& A,const FVector& B) {
  return FMath::Abs(A.Z-B.Z)<=250.f && FVector::DistSquared(A,B)<=FMath::Square(800.f);
 };
 bool UniqueHome=true, UniqueTruck=true;
 // Explicit enrollment is a rare operator action, not a periodic world scan.
 // Reject duplicate GUIDs and any competing eligible nearby depot or truck.
 for(auto* Other:VS->GetDockingStations()) {
  if(!IsValid(Other) || Other==H) continue;
  if(Other->GetPathNodeGUID()==Node) { UniqueHome=false; break; }
  auto* B=Building(Other);
  if(IsValid(B) && IsDepot(Other) && ValidHome(Other) && B->GetDockingPathNode() && Near(Position,B->GetDockingPathNode()->GetActorLocation())) {
   UniqueHome=false; break;
  }
 }
 for(auto* Other:VS->GetAllVehicles()) {
  if(!IsValid(Other) || Other==V) continue;
  if(Other->GetVehicleRoute().Contains(Node)) { UniqueTruck=false; break; }
  auto* OtherOwner=Other->GetOwnerVehicle();
  if(IsValid(OtherOwner)) {
   FVector OtherPosition; FRotator OtherRotation; OtherOwner->GetVehicleLocationAndRotation(OtherPosition,OtherRotation);
   if(Near(OtherPosition,HomeLocation)) { UniqueTruck=false; break; }
  }
 }
 const TArray<FGuid> Before=V->GetVehicleRoute();
 const bool BeforeAP=V->IsAutopilotEnabled();
 const TArray<FGuid> Intended={Node};
 const Route ExistingRoute=Before.IsEmpty()?Route::Empty:Before==Intended?Route::HomeOnly:Route::Other;
 const Snapshot Facts{true,(!Home->GetDockedActor() || Home->GetDockedActor()==VehicleActor),UniqueHome,UniqueTruck,
  VehicleActor->GetDriver()!=nullptr,FMath::Abs(VehicleActor->GetForwardSpeed())<=1.f,VehicleActor->GetStorageInventory()->IsEmpty(),Near(Position,HomeLocation),BeforeAP,ExistingRoute};
 const auto Verdict=Evaluate(Facts);
 if(Verdict==Decision::Reject) return false;
 FFILNRegistration R; R.Vehicle=V; R.Home=H; R.TruckId=FGuid::NewGuid(); R.BayId=StationId(H);
 for(const auto& Other:Registrations) if(Other.BayId==R.BayId || Other.TruckId==R.TruckId) return false;
 if(Reservations.StationBusy(Key(R.BayId))) return false;
 // No native writes for adoption of an already opted-in home-only truck.
 const auto Rollback=[&]() {
  if(Verdict!=Decision::AssignHome) return;
  if(VehicleActor->GetDriver() || V->GetVehicleRoute()!=Intended) {
   RequestRegistryRepair();
   UE_LOG(LogFILN,Error,TEXT("Enrollment rollback conflict; rediscovering vehicle"));
   return;
  }
  V->SetAutopilotEnabled(BeforeAP);
  V->SetVehicleRoute(Before);
  if(V->GetVehicleRoute()!=Before || V->IsAutopilotEnabled()!=BeforeAP) {
   RequestRegistryRepair();
   UE_LOG(LogFILN,Error,TEXT("Enrollment rollback readback failed; rediscovering vehicle"));
  }
 };
 if(VehicleActor->GetDriver() || V->GetVehicleRoute()!=Before || V->IsAutopilotEnabled()!=BeforeAP ||
    !VehicleActor->GetStorageInventory()->IsEmpty() || !ValidHome(H) || !VehicleStationCompatible(VehicleActor,Home)) return false;
 if(Verdict==Decision::AssignHome) {
  V->SetVehicleRoute(Intended);
  if(V->GetVehicleRoute()!=Intended) {
   if(V->GetVehicleRoute()!=Before || V->IsAutopilotEnabled()!=BeforeAP) Rollback();
   return false;
  }
  if(VehicleActor->GetDriver()) { Rollback(); return false; }
  V->SetAutopilotEnabled(true);
 }
 if(VehicleActor->GetDriver() || V->GetVehicleRoute()!=Intended || !V->IsAutopilotEnabled()) { Rollback(); return false; }
 if(!Ownership.Pair(Key(R.TruckId),Key(R.BayId))) { Rollback(); return false; }
 if(!WatchTransfers(R)) {
  Ownership.Unpair(Key(R.TruckId),Key(R.BayId)); Rollback();
  UE_LOG(LogFILN,Error,TEXT("Pair rejected: native cargo transfer observer unavailable")); return false;
 }
 R.Held=true; R.IdleRoute=Intended;
 R.Diagnostic=TEXT("Explicit home-only enrollment; native route and autopilot read back");
 HomeStations.Add(H); TrackDepot(H);
 ByVehicle.Add(V,Registrations.Add(R)); ResetScan();
 UE_LOG(LogFILN,Log,TEXT("PAIRED truck=%s bay=%s home-only/AP enabled"),*R.TruckId.ToString(),*R.BayId.ToString());
 return true;
}
bool ALogisticsSubsystem::Dispatch(FFILNRegistration& R,AFGDockingStationIdentifier* P,AFGDockingStationIdentifier* Q) {
 check(IsInGameThread());
 if(!HasAuthority() || bDispatchBlocked || !IsValid(P) || !IsValid(Q) || !IsValid(R.Vehicle)) return false;
 const auto* Index=ByVehicle.Find(R.Vehicle);
 const int32 Request=StationConfigs.IndexOfByPredicate([Q](const FFILNStationConfig& C){return C.Station==Q;});
 if(!Index || !Registrations.IsValidIndex(*Index) || Request==INDEX_NONE) return false;
 // Explicit commands use the same AP-on eligibility, measured ledger and leases
 // as automatic scheduling. No second dispatcher and no AP override.
 ResetScan(); ProviderCandidates.Add(P); ScanRoadRevision=RoadCache.Invalidations;
 const bool Accepted=CommitOffer(*Index,Request,0);
 ResetScan();
 if(!Accepted) UE_LOG(LogFILN,Log,TEXT("Explicit dispatch rejected or road calculation pending; inspect filn.status and retry only if eligible"));
 return Accepted;
}
void ALogisticsSubsystem::Observe() {
 check(IsInGameThread());
 if(!HasAuthority() || !bRegistryValidated || bPreparingUninstall || bChangingFilters) return;
 ConsumeDockReceipts();
 RetryStationFilters();
 EnsureDepotRecipe();
 AuditLostRegistrations();
 DiscoverHomeVehicles();
 AuditIdleFleet();
 ServiceFuel();
 // Bounded round-robin across active jobs only. Recovery and idle records are not scanned.
 const int32 Budget=FMath::Min(16,Active.Num());
 for(int32 Count=0;Count<Budget && Active.Num()>0;++Count) {
  Cursor%=Active.Num(); const int32 Index=Active[Cursor];
  if(!Registrations.IsValidIndex(Index)) { Active.RemoveAtSwap(Cursor); RequestRegistryRepair(); continue; }
  auto& R=Registrations[Index];
  if(StateOf(R)==S::Recovery || StateOf(R)==S::Idle) { Active.RemoveAtSwap(Cursor); continue; }
  const double Now=GetWorld()->GetTimeSeconds();
  R.Elapsed+=static_cast<float>(FMath::Max(0.0,Now-R.LastObserved)); R.LastObserved=Now;
  auto* PB=Building(R.Provider); auto* QB=Building(R.Requester); auto* HB=Building(R.Home);
  if(!IsValid(R.Vehicle) || !IsValid(R.Vehicle->GetOwnerVehicle()) || !IsValid(HB) || !ValidHome(R.Home) || !VehicleStationCompatible(R.Vehicle->GetOwnerVehicle(),HB)) {
   Recover(R,TEXT("Vehicle or home destroyed/unavailable")); ++Cursor; continue;
  }
  if(!HasTransferObserver(R) && (!WatchTransfers(R) || (!R.AbortReturn && !ArmDockReceipts(R)))) {
   R.CargoOriginKnown=false; R.CargoOriginStorage=false;
   Recover(R,TEXT("Cargo observer no longer matches the native vehicle inventory")); ++Cursor; continue;
  }
  if(!R.AbortReturn && !EndpointConfigurationValid(R)) {
   if(!R.Automatic || !TryAbortToHome(R,Now)) Recover(R,TEXT("Endpoint unavailable; safe return not established"));
   ++Cursor; continue;
  }
  if(R.Vehicle->GetVehicleRoute()!=R.RouteIntent) {
   Recover(R,TEXT("Native route changed outside FILN")); ++Cursor; continue;
  }
  auto* VehicleActor=R.Vehicle->GetOwnerVehicle();
  if(VehicleActor->GetDriver() || !R.Vehicle->IsAutopilotEnabled()) { Recover(R,TEXT("Player control or autopilot disabled; leave vehicle state untouched")); ++Cursor; continue; }
  const bool AtP=IsValid(PB) && PB->GetDockedActor()==VehicleActor, AtQ=IsValid(QB) && QB->GetDockedActor()==VehicleActor, AtH=HB->GetDockedActor()==VehicleActor;
  const S Before=StateOf(R); S Next=Before;
  if(AtP || AtQ || (!AtH && HB->GetDockingPathNode() && FVector::DistSquared(VehicleActor->GetVehicleLocation(),HB->GetDockingPathNode()->GetActorLocation())>FMath::Square(800.f))) R.DepartedHome=true;
  if(R.Automatic) {
   // Do not advance or finalize a job while a positive native receipt is
   // awaiting readback. In particular do not reset its bounded retry clock.
   if(HasPendingDockReceipts(R.JobId)) { ++Cursor; continue; }
   auto* Cargo=VehicleActor->GetStorageInventory();
   if(!Cargo || !R.Resource) { Recover(R,TEXT("Cargo inventory/resource missing")); ++Cursor; continue; }
   TArray<FInventoryStack> Stacks; Cargo->GetInventoryStacks(Stacks); bool Mixed=false;
   for(const auto& Stack:Stacks) if(Stack.NumItems>0 && Stack.Item.GetItemClass()!=R.Resource) Mixed=true;
   const int32 Quantity=Cargo->GetNumItems(R.Resource);
   if(!filn::transfer::Reconciled({R.LastCargo,R.LoadedCargo,R.DeliveredCargo},Quantity,Mixed)) {
    // Only a pending native operation may explain a temporary mismatch. Give
    // its after-hook one bounded interval to report, never infer a delivery.
    if(!Mixed && (R.ReceiptMismatchSince<0 || Now-R.ReceiptMismatchSince<1.0)) {
     if(R.ReceiptMismatchSince<0) R.ReceiptMismatchSince=Now;
     ++Cursor; continue;
    }
    R.CargoOriginKnown=false; R.CargoOriginStorage=false;
    Recover(R,TEXT("Cargo reconciliation disagrees with native docking receipts; no inferred transfer credited")); ++Cursor; continue;
   }
   R.ReceiptMismatchSince=-1; R.LastCargo=Quantity;
   if((Before==S::Loading && !AtP && !filn::recovery::ProviderDepartureProven(true,false,R.LoadedCargo,Quantity,R.ExpectedCargo)) ||
      (Before==S::Unloading && !AtQ && R.DeliveredCargo<=0)) {
    Recover(R,TEXT("Station departed without measured matching cargo transfer")); ++Cursor; continue;
   }
  }
  const bool Travelling=Before==S::ToProvider || Before==S::ToRequester || Before==S::Returning;
  const FVector Location=VehicleActor->GetVehicleLocation();
  const int32 Waypoint=R.Vehicle->GetCurrentTargetWaypointIndex();
  const bool Moved=!R.TravelProgress.initialized || Waypoint!=R.ProgressWaypoint ||
    FVector::DistSquared(Location,R.ProgressLocation)>=FMath::Square(200.f);
  const bool Docking=!Travelling || AtP || AtQ || AtH || R.Vehicle->IsCurrentlyDocking();
  if(Moved || Docking) { R.ProgressLocation=Location; R.ProgressWaypoint=Waypoint; }
  const bool Stalled=R.TravelProgress.Sample(Now,Moved,Docking);
  const auto Error=R.Vehicle->GetAutopilotErrorStatus();
  const bool Grace=filn::recovery::ActivationGrace(Error==EVehicleAutopilotErrorStatus::TooFewStations,Now,R.RouteActivationDeadline);
  if(Error==EVehicleAutopilotErrorStatus::None) R.RouteActivationDeadline=0;
  if((Stalled || (Error!=EVehicleAutopilotErrorStatus::None && !Grace)) && !Docking) {
   if(!R.Automatic || !RetryNavigation(R,Now,Stalled)) Recover(R,FString::Printf(TEXT("Native autopilot error=%d; automatic retry is unsafe"),static_cast<int32>(R.Vehicle->GetAutopilotErrorStatus())));
   ++Cursor; continue;
  }
  if(R.NavigationRetry.waiting && (Moved || Docking)) {
   R.NavigationRetry.Reset(); R.Elapsed=0;
   R.Diagnostic=TEXT("Native navigation fault cleared; continuing reserved journey, no transfer inferred");
  }
  if(R.Elapsed>1800.f) { Recover(R,TEXT("Journey timeout without a native navigation error")); ++Cursor; continue; }
  if(Before==S::ToProvider && AtP) Next=filn::Advance(Before,filn::Event::ProviderDock);
  else if(Before==S::Loading && !AtP) Next=filn::Advance(Before,filn::Event::ProviderLeave);
  else if(Before==S::ToRequester && AtQ) Next=filn::Advance(Before,filn::Event::RequesterDock);
  else if(Before==S::Unloading && !AtQ && !AtH) Next=filn::Advance(Before,filn::Event::RequesterLeave);
  else if((Before==S::Returning || (Before==S::Unloading && !AtQ && R.DeliveredCargo>0)) && AtH) {
   if(!R.Automatic) R.Vehicle->SetAutopilotEnabled(false);
   auto* Cargo=VehicleActor->GetStorageInventory();
   if(!Cargo || (!R.Automatic && !Cargo->IsEmpty()) || (R.Automatic && R.DeliveredCargo<=0 && !R.AbortReturn)) { Recover(R,TEXT("Returned with cargo: incomplete unload requires operator recovery")); ++Cursor; continue; }
   if(R.Automatic && FMath::Abs(VehicleActor->GetForwardSpeed())<=1.f) {
    if(!RestoreHomeRoute(R)) { Recover(R,TEXT("Home route restoration failed; reservations retained")); ++Cursor; continue; }
    Next=filn::Advance(S::Returning,filn::Event::HomeStopped);
   } else if(!R.Automatic && Parked(R)) Next=filn::Advance(Before,filn::Event::HomeStopped);
  }
  else if((Before==S::ToProvider && AtQ) || (Before==S::ToRequester && AtH && R.DepartedHome) || (Before==S::Returning && AtP)) Next=S::Recovery;
  if(Next==S::Recovery) Recover(R,TEXT("Out-of-order docking observed; journey cannot be proven"));
  else if(Next!=Before) {
   SetState(R,Next);
   UE_LOG(LogFILN,Log,TEXT("job=%s state=%d planned=%d loaded=%d delivered=%d cargo=%d"),*R.JobId.ToString(),R.State,R.ExpectedCargo,R.LoadedCargo,R.DeliveredCargo,R.LastCargo);
   if(Next==S::Completed) {
    if(!Reservations.OwnsJob(Key(R.JobId),Key(R.TruckId))) { RetireJobToParking(R,TEXT("Completion lease ownership mismatch; retry parking")); RequestRegistryRepair(); ++Cursor; continue; }
    DisarmDockReceipts(R.JobId); Reservations.Release(Key(R.JobId)); SetState(R,S::Idle); R.Diagnostic=R.Automatic?FString::Printf(TEXT("Home stopped; loaded=%d delivered=%d retained=%d; native autopilot tick held at home"),R.LoadedCargo,R.DeliveredCargo,R.LastCargo):TEXT("Home stopped; transfers not measured");
    if(R.AbortReturn) R.Diagnostic=FString::Printf(TEXT("Aborted shipment returned home; delivered=%d retained=%d; no delivery success inferred"),R.DeliveredCargo,R.LastCargo);
    R.JobId.Invalidate(); R.Provider=nullptr; R.Requester=nullptr; R.RouteIntent.Empty(); R.AbortReturn=false; R.DepartedHome=false; R.RouteActivationDeadline=0;
    R.ProviderLease.Invalidate(); R.RequesterLease.Invalidate();
    R.NavigationRetry.Reset(); R.TravelProgress.Reset();
    Active.RemoveAtSwap(Cursor); continue;
   }
  }
  if(!R.Vehicle->IsAutopilotEnabled() && StateOf(R)!=S::Idle && StateOf(R)!=S::Recovery)
   Recover(R,TEXT("Autopilot stopped before confirmed return"));
  ++Cursor;
 }
}
// Record explicit destruction before GC can erase the saved actor pointer.
 // A surviving owner, streaming/unresolved reference or world shutdown is not
 // sufficient evidence for releasing cargo endpoint leases.
void ALogisticsSubsystem::NoteVehicleDestroyed(const FGuid& Truck,AFGWheeledVehicleIdentifier* Vehicle,TWeakObjectPtr<AFGWheeledVehicle> BoundOwner) {
 if(!HasAuthority() || !IsInGameThread() || !bRegistryValidated || !GetWorld() || GetWorld()->bIsTearingDown || !Vehicle) return;
 const auto* Index=ByVehicle.Find(Vehicle);
 if(!Index || !Registrations.IsValidIndex(*Index) || Registrations[*Index].TruckId!=Truck) return;
 DestroyedVehicles.Add(Truck,{BoundOwner,TWeakObjectPtr<AFGWheeledVehicle>(Vehicle->GetOwnerVehicle())});
 Registrations[*Index].Held=false;
 ResetScan();
}
void ALogisticsSubsystem::AuditLostRegistrations() {
 if(!HasAuthority() || bDispatchBlocked || Registrations.IsEmpty()) return;
 const int32 Budget=FMath::Min(4,Registrations.Num());
 for(int32 Count=0;Count<Budget && !Registrations.IsEmpty();++Count) {
  LostCursor%=Registrations.Num();
  if(!RemoveLostRegistration(LostCursor)) ++LostCursor;
 }
}
bool ALogisticsSubsystem::RemoveLostRegistration(int32 Index) {
 if(!Registrations.IsValidIndex(Index)) return false;
 auto& R=Registrations[Index];
 const auto* Proof=DestroyedVehicles.Find(R.TruckId);
 // Wait until both the owner observed at binding and the owner at destruction
 // are gone. The explicit identifier event is required even if the saved
 // reference later becomes null. Never reclaim a lease on pointer absence alone.
 if(!filn::registry::MayReclaimDestroyed(Proof!=nullptr,Proof && Proof->BoundOwner.IsValid(),Proof && Proof->LastOwner.IsValid())) return false;
 if(!RemoveRegistrationAt(Index)) return false;
 UE_LOG(LogFILN,Warning,TEXT("Destroyed vehicle registration removed; its leases and home ownership released"));
 return true;
}
bool ALogisticsSubsystem::UnpairParkedVehicle(int32 Index,AFGDockingStationIdentifier* Home) {
 check(IsInGameThread());
 if(!HasAuthority() || !bRegistryValidated || bDispatchBlocked || bPreparingUninstall || bChangingFilters || !Registrations.IsValidIndex(Index)) return false;
 auto& R=Registrations[Index];
 if(R.Home!=Home || StateOf(R)!=S::Idle || R.JobId.IsValid() || R.FuelPending || !IsValid(R.Vehicle) || !IsValid(Home)) return false;
 auto* V=R.Vehicle.Get(); auto* O=V->GetOwnerVehicle(); auto* B=Building(Home);
 if(!IsValid(O) || !IsValid(B) || !B->GetDockingPathNode() || V->IsAutopilotEnabled() || O->GetDriver()) return false;
 const FVector Delta=O->GetVehicleLocation()-B->GetDockingPathNode()->GetActorLocation();
 if(!filn::dispatch::HomePositionReady(Delta.SizeSquared(),Delta.Z,O->GetForwardSpeed(),B->GetDockedActor() && B->GetDockedActor()!=O)) return false;
 // Explicitly release an idle, AP-off truck without changing route or cargo.
 // The station remains a designated rest stop for its next owner.
 return RemoveRegistrationAt(Index);
}
bool ALogisticsSubsystem::RemoveRegistrationAt(int32 Index) {
 if(!Registrations.IsValidIndex(Index)) return false;
 auto& R=Registrations[Index];
 if(R.JobId.IsValid() && !Reservations.OwnsJob(Key(R.JobId),Key(R.TruckId))) { RetireJobToParking(R,TEXT("Job lease mismatch during removal")); RequestRegistryRepair(); return false; }
 if(!Ownership.Unpair(Key(R.TruckId),Key(R.BayId))) { RequestRegistryRepair(); return false; }
 DisarmDockReceipts(R.JobId);
 if(R.JobId.IsValid()) Reservations.Release(Key(R.JobId));
 if(IsValid(R.Home)) HomeStations.Remove(TWeakObjectPtr<AFGDockingStationIdentifier>(R.Home.Get()));
 for(auto It=ByVehicle.CreateIterator();It;++It) if(It.Value()==Index) It.RemoveCurrent();
 for(int32 I=TransferObservers.Num()-1;I>=0;--I) if(!TransferObservers[I] || TransferObservers[I]->IsBoundToTruck(R.TruckId)) {
  if(TransferObservers[I]) TransferObservers[I]->Unbind();
  TransferObservers.RemoveAtSwap(I);
 }
 DestroyedVehicles.Remove(R.TruckId); TransferByTruck.Remove(R.TruckId);
 Active.Remove(Index);
 Registrations.RemoveAt(Index);
 for(auto& Entry:ByVehicle) if(Entry.Value>Index) --Entry.Value;
 for(auto& I:Active) if(I>Index) --I;
 ResetScan();
 return true;
}
void ALogisticsSubsystem::Execute(const FString& Command,const TArray<FString>& Args) {
 check(IsInGameThread()); if(!HasAuthority()) return;
 auto* VS=AFGVehicleSubsystem::Get(GetWorld()); if(!VS) return;
 if(Command==TEXT("swatch")) { ExecuteNetworkSwatch(Args); return; }
 if(Command==TEXT("list")) {
  for(auto* V:VS->GetAllVehicles()) if(IsValid(V)) UE_LOG(LogFILN,Log,TEXT("vehicle %s name=%s"),*V->GetPathName(),*V->GetVehicleName().ToString());
  for(auto* H:VS->GetDockingStations()) if(IsValid(H)) UE_LOG(LogFILN,Log,TEXT("station %s name=%s node=%s"),*H->GetPathName(),*H->GetStationName().ToString(),*H->GetPathNodeGUID().ToString());
  return;
 }
 if(Command==TEXT("depot") && Args.Num()==2 && (Args[0]==TEXT("register") || Args[0]==TEXT("remove"))) {
  for(auto* Station:VS->GetDockingStations()) if(IsValid(Station) && Station->GetPathName()==Args[1]) {
   const bool Ok=SetDepot(Station,Args[0]==TEXT("remove"));
   UE_LOG(LogFILN,Log,TEXT("Depot role: %s"),Ok?TEXT("accepted"):TEXT("rejected")); return;
  }
 }
 if(Command==TEXT("removeStation") && Args.Num()==1) {
  for(auto* Station:VS->GetDockingStations()) if(IsValid(Station) && Station->GetPathName()==Args[0]) { RemoveStation(Station); return; }
 }
 if(Command==TEXT("enable") && Args.Num()==1 && (Args[0]==TEXT("0") || Args[0]==TEXT("1"))) {
  const bool Accepted=SetDispatchEnabled(Args[0]==TEXT("1"));
  UE_LOG(LogFILN,Log,TEXT("Automatic dispatch=%d request accepted=%d; existing journeys continue"),bAutoDispatchEnabled,Accepted); return;
 }
 if(Command==TEXT("status")) {
  UE_LOG(LogFILN,Log,TEXT("automatic=%d blocked=%d stations=%d cache hits=%llu misses=%llu invalidations=%llu work=%llu"),bAutoDispatchEnabled,bDispatchBlocked,StationConfigs.Num(),RoadCache.Hits,RoadCache.Misses,RoadCache.Invalidations,RoadCache.Work);
  for(const auto& R:Registrations) UE_LOG(LogFILN,Log,TEXT("truck=%s bay=%s job=%s state=%d diagnostic=%s"),*R.TruckId.ToString(),*R.BayId.ToString(),*R.JobId.ToString(),R.State,*R.Diagnostic);
  return;
 }
 AFGWheeledVehicleIdentifier* V=nullptr;
 if(Args.Num()>0) for(auto* Candidate:VS->GetAllVehicles()) if(IsValid(Candidate) && Candidate->GetPathName()==Args[0]) { V=Candidate; break; }
 auto FindStation=[VS](const FString& Path)->AFGDockingStationIdentifier* {
  for(auto* H:VS->GetDockingStations()) if(IsValid(H) && H->GetPathName()==Path) return H;
  return nullptr;
 };
 if(Command==TEXT("station") && Args.Num()==2) {
  auto Resource=LoadClass<UFGItemDescriptor>(nullptr,*Args[1]);
  const bool Ok=ConfigureStation(FindStation(Args[0]),Resource);
  UE_LOG(LogFILN,Log,TEXT("Station resource configuration: %s"),Ok?TEXT("accepted"):TEXT("rejected")); return;
 }
 if(Command==TEXT("prepareUninstall") && Args.Num()==0) {
  const bool Ok=PrepareStationsForUninstall();
  UE_LOG(LogFILN,Log,TEXT("Prepare uninstall: %s. Independent save/load without FILN remains unverified."),Ok?TEXT("station restrictions restored; save required"):TEXT("rejected; resolve active jobs/filter conflicts")); return;
 }
 bool Ok=false;
 if(V && Command==TEXT("pair") && Args.Num()==2) Ok=Pair(V,FindStation(Args[1]));
 else if(V) if(const auto* I=ByVehicle.Find(V); I && Registrations.IsValidIndex(*I)) {
  auto& R=Registrations[*I];
  if(Command==TEXT("dispatch") && Args.Num()==3) Ok=Dispatch(R,FindStation(Args[1]),FindStation(Args[2]));
  if(Command==TEXT("unpair") && Args.Num()==1) Ok=UnpairParkedVehicle(*I,R.Home);
  if(Command==TEXT("reset") && Args.Num()==1) Ok=RehomeParkedVehicle(*I,R.Home);
  if(Command==TEXT("rehome") && Args.Num()==2) Ok=RehomeParkedVehicle(*I,FindStation(Args[1]));
 }
 UE_LOG(LogFILN,Log,TEXT("%s: %s"),*Command,Ok?TEXT("accepted"):TEXT("rejected; inspect filn.status and preconditions"));
}
