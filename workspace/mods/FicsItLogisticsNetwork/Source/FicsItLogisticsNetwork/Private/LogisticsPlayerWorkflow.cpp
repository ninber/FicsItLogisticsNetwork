// SPDX-License-Identifier: GPL-3.0-only
#include "LogisticsSubsystem.h"
#include "FILNPlayerWorkflow.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "WheeledVehicles/FGVehicleSubsystem.h"
#include "WheeledVehicles/FGWheeledVehicleIdentifier.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "WheeledVehicles/FGVehiclePathNode.h"
#include "FGInventoryComponent.h"
#include "Resources/FGItemDescriptor.h"
#include "UObject/UnrealType.h"
#include "Engine/World.h"

namespace {
std::string Key(const FGuid& G) { return TCHAR_TO_UTF8(*G.ToString()); }
// Required live integration with the same programmable station that the Lua
// dispatcher reads. No legacy-save import and no copying third-party UI/code.
TSubclassOf<UFGItemDescriptor> PlayerFilter(AFGBuildableDockingStation* B) {
 if(!IsValid(B) || B->GetClass()->GetPathName()!=TEXT("/TruckStationSettings/Build_ProgrammableTruckStation.Build_ProgrammableTruckStation_C")) return nullptr;
 const auto* P=FindFProperty<FArrayProperty>(B->GetClass(),TEXT("FilterItem"));
 const auto* Inner=P?CastField<FObjectPropertyBase>(P->Inner):nullptr;
 if(!P || !Inner) return nullptr;
 FScriptArrayHelper Items(P,P->ContainerPtrToValuePtr<void>(B));
 TSubclassOf<UFGItemDescriptor> Selected;
 for(int32 I=0; I<Items.Num(); ++I) {
  auto* C=Cast<UClass>(Inner->GetObjectPropertyValue(Items.GetRawPtr(I)));
  if(!IsValid(C)) continue;
  if(!C->IsChildOf(UFGItemDescriptor::StaticClass()) || (Selected && Selected.Get()!=C)) return nullptr;
  Selected=C;
 }
 return Selected && UFGItemDescriptor::GetForm(Selected)==EResourceForm::RF_SOLID ? Selected : TSubclassOf<UFGItemDescriptor>();
}
}
bool ALogisticsSubsystem::SupportsSolidCargo(const UObject* Object) {
 const auto* Property=IsValid(Object)?FindFProperty<FBoolProperty>(Object->GetClass(),TEXT("mIsFluidStorageInventory")):nullptr;
 return Property && !Property->GetPropertyValue_InContainer(Object);
}
bool ALogisticsSubsystem::StationResourceMatches(const FFILNStationConfig& C) const {
 auto* B=IsValid(C.Station)?C.Station->GetStation():nullptr;
 auto* I=IsValid(B)?B->GetInventory():nullptr;
 if(!IsValid(I) || !C.Resource || I->GetSizeLinear()<=0 || I->GetSizeLinear()>4096) return false;
 if(C.ExternalFilter) return PlayerFilter(B)==C.Resource;
 for(int32 Slot=0;Slot<I->GetSizeLinear();++Slot) if(I->GetAllowedItemOnIndex(Slot)!=C.Resource) return false;
 return true;
}
void ALogisticsSubsystem::ReconcilePlayerConfiguration() {
 if(!HasAuthority() || !bRegistryValidated || bDispatchBlocked || bPreparingUninstall || bChangingFilters) return;
 const double Now=GetWorld()->GetTimeSeconds();
 if(PlayerStationCursor>=PlayerStationScan.Num()) {
  if(Now<NextPlayerStationScan) return;
  auto* VS=AFGVehicleSubsystem::Get(GetWorld()); if(!VS) return;
  PlayerStationScan.Reset(); for(auto* ID:VS->GetDockingStations()) if(IsValid(ID)) PlayerStationScan.Add(ID);
  PlayerStationCursor=0; NextPlayerStationScan=Now+10.;
 }
 for(int32 Work=0; Work<2 && PlayerStationCursor<PlayerStationScan.Num(); ++Work) {
  auto* ID=PlayerStationScan[PlayerStationCursor++].Get();
  if(!IsValid(ID) || IsDepot(ID)) continue;
  auto* B=ID->GetStation();
  auto Resource=SupportsSolidCargo(B)?PlayerFilter(B):TSubclassOf<UFGItemDescriptor>();
  const int32 Index=StationConfigs.IndexOfByPredicate([ID](const FFILNStationConfig& C){return C.Station==ID;});
  if(Index!=INDEX_NONE && !StationConfigs[Index].ExternalFilter) continue;
  if(!Resource) {
   if(Index!=INDEX_NONE) { StationConfigs.RemoveAt(Index); ResetScan(); }
   continue;
  }
  if(Index!=INDEX_NONE && StationConfigs[Index].Resource==Resource) continue;
  // Selection is owned by the original widget. Do not write slots, infer
  // demand from stock or require a second Apply/Register action.
  FFILNStationConfig C; C.Station=ID; C.Resource=Resource; C.ExternalFilter=true;
  StationId(ID);
  if(Index==INDEX_NONE) StationConfigs.Add(C); else StationConfigs[Index]=C;
  ResetScan();
 }
}
void ALogisticsSubsystem::DiscoverHomeVehicles() {
 if(!HasAuthority() || !bRegistryValidated || !bAutoDispatchEnabled || bDispatchBlocked || bPreparingUninstall || DepotStations.IsEmpty()) return;
 const double Now=GetWorld()->GetTimeSeconds();
 auto* VS=AFGVehicleSubsystem::Get(GetWorld()); if(!VS) return;
 if(PlayerVehicleCursor>=PlayerVehicleScan.Num()) {
  if(Now<NextPlayerVehicleScan) return;
  PlayerVehicleScan.Reset(); for(auto* V:VS->GetAllVehicles()) if(IsValid(V)) PlayerVehicleScan.Add(V);
  for(auto It=OverflowCursor.CreateIterator();It;++It) if(!It.Key().IsValid()) It.RemoveCurrent();
  for(auto It=PendingHomeReason.CreateIterator();It;++It) if(!It.Key().IsValid()) It.RemoveCurrent();
  PlayerVehicleCursor=0; NextPlayerVehicleScan=Now+5.;
 }
 for(int32 Work=0;Work<4 && PlayerVehicleCursor<PlayerVehicleScan.Num();++Work) {
  auto* V=PlayerVehicleScan[PlayerVehicleCursor++].Get();
  if(!IsValid(V) || ByVehicle.Contains(V)) continue;
  const auto Route=V->GetVehicleRoute();
  if(Route.Num()!=1 || !Route[0].IsValid()) continue;
  AFGDockingStationIdentifier* Requested=nullptr; bool Ambiguous=false;
  for(const auto& D:DepotStations) if(IsValid(D) && D->GetPathNodeGUID()==Route[0]) {
   if(Requested) Ambiguous=true; Requested=D.Get();
  }
  if(Requested && !Ambiguous) AdoptPlayerHome(V,Requested);
  else { OverflowCursor.Remove(V); PendingHomeReason.Remove(V); }
 }
}
bool ALogisticsSubsystem::AdoptPlayerHome(AFGWheeledVehicleIdentifier* V,AFGDockingStationIdentifier* Requested) {
 check(IsInGameThread());
 if(!IsValid(V) || !IsValid(Requested) || !IsDepot(Requested) || !ValidHome(Requested)) return false;
 auto* O=V->GetOwnerVehicle(); auto* VS=AFGVehicleSubsystem::Get(GetWorld());
 if(!IsValid(O) || !VS || !O->GetStorageInventory() || !SupportsSolidCargo(O)) return false;
 const auto Before=V->GetVehicleRoute();
 const FGuid Wanted=Requested->GetPathNodeGUID();
 if(!filn::player::OptedIn(O->HasAuthority(),V->IsAutopilotEnabled(),O->GetDriver()!=nullptr,Before.Num()==1 && Before[0]==Wanted,ByVehicle.Contains(V))) return false;
 if(!Wanted.IsValid() || V->IsCurrentlyDocking()) return false;
 const TWeakObjectPtr<AFGWheeledVehicleIdentifier> VehicleKey(V);
 const auto Pending=[&](const FString& Why) {
  auto& Previous=PendingHomeReason.FindOrAdd(VehicleKey);
  if(Previous!=Why) { Previous=Why; UE_LOG(LogFILN,Log,TEXT("AUTO_HOME_PENDING vehicle=%s reason=%s"),*V->GetPathName(),*Why); }
 };
 auto Free=[&](AFGDockingStationIdentifier* D) {
  if(!IsValid(D) || !IsValid(D->GetStation())) return false;
  auto* B=D->GetStation(); const auto Node=D->GetPathNodeGUID(); bool Unique=Node.IsValid(),Foreign=false;
  for(auto* Other:VS->GetDockingStations()) if(IsValid(Other) && Other!=D && Other->GetPathNodeGUID()==Node) Unique=false;
  for(auto* Other:VS->GetAllVehicles()) if(IsValid(Other) && Other!=V && Other->GetVehicleRoute().Contains(Node)) Foreign=true;
  return filn::player::Free({IsDepot(D),ValidHome(D),B->HasPower() && !B->IsProductionPaused(),
   HomeStations.Contains(TWeakObjectPtr<AFGDockingStationIdentifier>(D)),B->GetDockedActor() && B->GetDockedActor()!=O,
   Reservations.StationBusy(Key(StationId(D))),Unique,Foreign});
 };
 AFGDockingStationIdentifier* Destination=nullptr;
 const bool RequestedClaimed=HomeStations.Contains(TWeakObjectPtr<AFGDockingStationIdentifier>(Requested));
 if(Free(Requested)) Destination=Requested;
 else if(RequestedClaimed) {
  // Do not displace the existing owner. A two-node native route is necessary
  // for movement; a home-only route is only the final parked intent.
  if(!O->HasFuel() || !O->GetStorageInventory()->IsEmpty()) { Pending(TEXT("Occupied depot: overflow needs fuel and empty cargo")); return false; }
  const auto From=V->GetCurrentToPathNodeGUID(); auto* Preset=O->GetVehiclePathPreset();
  if(!From.IsValid() || !Preset) { Pending(TEXT("Occupied depot: no native road segment or vehicle preset")); return false; }
  int32& Cursor=OverflowCursor.FindOrAdd(VehicleKey);
  int32 Queried=0;
  // Retain cursor across bounded passes: an unreachable early depot must
  // not starve a later free one. Pending cache work resumes on that candidate.
  for(int32 Visited=0;Visited<DepotStations.Num() && Queried<4;++Visited) {
   Cursor%=DepotStations.Num();
   auto* Candidate=DepotStations[Cursor].Get();
   if(Candidate==Requested || !Free(Candidate)) { ++Cursor; continue; }
   ++Queried;
   const auto Node=Candidate->GetPathNodeGUID(); double A=-1,B=-1,C=-1;
   if(!RoadCache.Distance(VS,From,Node,Preset,A) || !RoadCache.Distance(VS,Node,Wanted,Preset,B) || !RoadCache.Distance(VS,Wanted,Node,Preset,C)) { Pending(TEXT("Occupied depot: directed road calculation pending")); return false; }
   ++Cursor;
   if(A>=0 && B>=0 && C>=0) { Destination=Candidate; break; }
  }
  if(!Destination) Pending(TEXT("Occupied depot: no available reachable alternative in this pass; retrying"));
 }
 if(!Destination || !Free(Destination) || O->GetDriver() || !V->IsAutopilotEnabled() || V->GetVehicleRoute()!=Before) return false;
 const auto Node=Destination->GetPathNodeGUID();
 const bool Move=Destination!=Requested;
 auto* H=Destination->GetStation();
 const FVector Delta=O->GetVehicleLocation()-H->GetDockingPathNode()->GetActorLocation();
 const bool AtHome=filn::dispatch::HomePositionReady(Delta.SizeSquared(),Delta.Z,O->GetForwardSpeed(),H->GetDockedActor() && H->GetDockedActor()!=O);
 // For an unclaimed depot, adopt the same parked home-only route as Lua;
 // do not assert that a single-node route can drive from an arbitrary road.
 if(!Move && !AtHome) { Pending(TEXT("Park at the selected free depot to establish home-only route")); return false; }
 FFILNRegistration R; R.Vehicle=V; R.Home=Destination; R.TruckId=FGuid::NewGuid(); R.BayId=StationId(Destination);
 R.IdleRoute=Move?TArray<FGuid>{Node,Wanted}:Before; R.Relocating=Move; R.Held=!Move;
 if(!Ownership.Pair(Key(R.TruckId),Key(R.BayId))) return false;
 if(Move) {
  V->SetVehicleRoute(R.IdleRoute);
  if(V->GetVehicleRoute()==R.IdleRoute && V->IsAutopilotEnabled() && !O->GetDriver()) V->SetCurrentTargetWaypoint(0);
  if(V->GetVehicleRoute()!=R.IdleRoute || !V->IsAutopilotEnabled() || O->GetDriver() || V->GetCurrentTargetWaypointIndex()!=0) {
   if(!O->GetDriver() && V->IsAutopilotEnabled() && V->GetVehicleRoute()==R.IdleRoute) V->SetVehicleRoute(Before);
   if(V->GetVehicleRoute()!=Before || !V->IsAutopilotEnabled()) bDispatchBlocked=true;
   Ownership.Unpair(Key(R.TruckId),Key(R.BayId)); return false;
  }
 }
 R.Diagnostic=Move?TEXT("Relocating to unclaimed depot; arrival pending"):TEXT("Automatically adopted player home-only route");
 HomeStations.Add(Destination); TrackDepot(Destination); ByVehicle.Add(V,Registrations.Add(R)); WatchTransfers(Registrations.Last()); ResetScan();
 OverflowCursor.Remove(VehicleKey); PendingHomeReason.Remove(VehicleKey);
 UE_LOG(LogFILN,Log,TEXT("AUTO_HOME_ADOPT moving=%d truck=%s bay=%s"),Move,*R.TruckId.ToString(),*R.BayId.ToString());
 return true;
}
