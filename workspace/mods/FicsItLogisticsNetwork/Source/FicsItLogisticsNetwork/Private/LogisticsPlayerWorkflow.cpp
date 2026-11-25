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
#include "FGDockableInterface.h"
#include "Resources/FGItemDescriptor.h"
#include "Engine/World.h"

namespace {
std::string FILNPlayerGuidKey(const FGuid& G) { return TCHAR_TO_UTF8(*G.ToString()); }
filn::player::CargoMedium FILNPlayerMedium(EResourceForm Form) {
 using M=filn::player::CargoMedium;
 if(Form==EResourceForm::RF_SOLID) return M::Solid;
 if(Form==EResourceForm::RF_LIQUID || Form==EResourceForm::RF_GAS) return M::Fluid;
 return M::Unknown;
}
}
bool ALogisticsSubsystem::StationAcceptsResource(AFGBuildableDockingStation* Station,TSubclassOf<UFGItemDescriptor> Resource) {
 if(!IsValid(Station) || !Resource) return false;
 const auto Medium=FILNPlayerMedium(Station->GetDockingStationResourceForm());
 if(!filn::player::Compatible(Medium,FILNPlayerMedium(UFGItemDescriptor::GetForm(Resource)))) return false;
 return Medium!=filn::player::CargoMedium::Fluid || !Station->GetCurrentFluidDescriptor() || Station->GetCurrentFluidDescriptor()==Resource;
}
bool ALogisticsSubsystem::VehicleStationCompatible(AFGWheeledVehicle* Vehicle,AFGBuildableDockingStation* Station) {
 if(!IsValid(Vehicle) || !IsValid(Station) || !Vehicle->GetClass()->ImplementsInterface(UFGDockableInterface::StaticClass())) return false;
 return filn::player::Compatible(FILNPlayerMedium(IFGDockableInterface::Execute_GetDockInventoryResourceForm(Vehicle)),
   FILNPlayerMedium(Station->GetDockingStationResourceForm()));
}
bool ALogisticsSubsystem::StationResourceMatches(const FFILNStationConfig& C) const {
 auto* B=IsValid(C.Station)?C.Station->GetStation():nullptr;
 auto* I=IsValid(B)?B->GetInventory():nullptr;
 if(!IsValid(I) || !StationAcceptsResource(B,C.Resource) || I->GetSizeLinear()<=0 || I->GetSizeLinear()>4096) return false;
 // Retired external records are not owned filters. Never infer or restore their
 // configuration from another actor's fields or from current stock.
 if(C.ExternalFilter || C.OriginalFilters.Num()!=I->GetSizeLinear()) return false;
 for(int32 Slot=0;Slot<I->GetSizeLinear();++Slot) if(I->GetAllowedItemOnIndex(Slot)!=C.Resource) return false;
 return true;
}
void ALogisticsSubsystem::DiscoverDepots() {
 if(!HasAuthority() || !bRegistryValidated || bDispatchBlocked || bPreparingUninstall || bUninstallPrepared || bChangingFilters) return;
 const double Now=GetWorld()->GetTimeSeconds();
 if(PlayerStationCursor>=PlayerStationScan.Num()) {
  if(Now<NextPlayerStationScan) return;
  auto* VS=AFGVehicleSubsystem::Get(GetWorld()); if(!VS) return;
  PlayerStationScan.Reset(); for(auto* ID:VS->GetDockingStations()) if(IsValid(ID)) PlayerStationScan.Add(ID);
  PlayerStationCursor=0; NextPlayerStationScan=Now+10.;
 }
 for(int32 Work=0; Work<2 && PlayerStationCursor<PlayerStationScan.Num(); ++Work) {
  auto* ID=PlayerStationScan[PlayerStationCursor++].Get();
  if(!IsValid(ID)) continue;
  if(IsAuthoredDepot(ID) && !DepotIndex.Contains(TWeakObjectPtr<AFGDockingStationIdentifier>(ID))) {
   TrackDepot(ID);
   if(auto* B=ID->GetStation(); IsValid(B)) UE_LOG(LogFILN,Log,TEXT("FILN_DEPOT_DISCOVERED node=%s actor=%s class=%s valid_home=%d"),*ID->GetPathNodeGUID().ToString(),*B->GetPathName(),*B->GetClass()->GetPathName(),ValidHome(ID));
  }
 }
}
void ALogisticsSubsystem::DiscoverHomeVehicles() {
 if(!HasAuthority() || !bRegistryValidated || !bAutoDispatchEnabled || bDispatchBlocked || bPreparingUninstall) return;
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
  if(Route.Num()!=1 || !Route[0].IsValid()) {
   PendingHomeReason.Remove(TWeakObjectPtr<AFGWheeledVehicleIdentifier>(V));
   OverflowCursor.Remove(TWeakObjectPtr<AFGWheeledVehicleIdentifier>(V));
   continue; // multi-stop vanilla routes are intentionally not opted in
  }
  AFGDockingStationIdentifier* Requested=nullptr; bool Ambiguous=false;
  for(const auto& D:DepotStations) if(IsValid(D) && D->GetPathNodeGUID()==Route[0]) {
   if(Requested) Ambiguous=true; Requested=D.Get();
  }
  if(Requested && !Ambiguous) AdoptPlayerHome(V,Requested);
  else {
   OverflowCursor.Remove(V);
   const FString Why=Ambiguous?TEXT("Assigned home node matches multiple FILN depots"):
    FString::Printf(TEXT("Assigned home node %s is not a tracked FILN depot"),*Route[0].ToString());
   auto& Previous=PendingHomeReason.FindOrAdd(TWeakObjectPtr<AFGWheeledVehicleIdentifier>(V));
   if(Previous!=Why) {
    Previous=Why;
    UE_LOG(LogFILN,Warning,TEXT("AUTO_HOME_PENDING vehicle=%s reason=%s"),*V->GetPathName(),*Why);
   }
  }
 }
}
bool ALogisticsSubsystem::AdoptPlayerHome(AFGWheeledVehicleIdentifier* V,AFGDockingStationIdentifier* Requested) {
 check(IsInGameThread());
 if(!IsValid(V)) return false;
 const TWeakObjectPtr<AFGWheeledVehicleIdentifier> VehicleKey(V);
 const auto Pending=[&](const FString& Why) {
  auto& Previous=PendingHomeReason.FindOrAdd(VehicleKey);
  if(Previous!=Why) { Previous=Why; UE_LOG(LogFILN,Warning,TEXT("AUTO_HOME_PENDING vehicle=%s reason=%s"),*V->GetPathName(),*Why); }
 };
 if(!IsValid(Requested) || !IsDepot(Requested)) { Pending(TEXT("Assigned station is not a recognized FILN depot")); return false; }
 if(!ValidHome(Requested)) { Pending(TEXT("Depot requires LOAD mode, empty cargo, docking node and fuel-only connections")); return false; }
 auto* O=V->GetOwnerVehicle(); auto* VS=AFGVehicleSubsystem::Get(GetWorld());
 if(!IsValid(O) || !VS || !O->GetStorageInventory()) { Pending(TEXT("Native vehicle owner, subsystem or cargo inventory unavailable")); return false; }
 if(!VehicleStationCompatible(O,Requested->GetStation())) { Pending(TEXT("Vehicle and depot cargo media are incompatible")); return false; }
 const auto Before=V->GetVehicleRoute();
 const FGuid Wanted=Requested->GetPathNodeGUID();
 if(!filn::player::OptedIn(O->HasAuthority(),V->IsAutopilotEnabled(),O->GetDriver()!=nullptr,Before.Num()==1 && Before[0]==Wanted,ByVehicle.Contains(V))) { Pending(TEXT("Enrollment requires authority, autopilot enabled, no driver and an exclusive home-only route")); return false; }
 if(!Wanted.IsValid()) { Pending(TEXT("Depot path node invalid")); return false; }
 // A stopped truck actually docked at its requested home can be adopted
 // without route writes; a truck docking elsewhere is never commandeered.
 const bool Docking=V->IsCurrentlyDocking();
 if(Docking && Requested->GetStation()->GetDockedActor()!=O) { Pending(TEXT("Vehicle docking away from requested depot")); return false; }
 // Cheap rejection before any roster work. For an enrollment attempt, build
 // route/node indexes once, not once per candidate depot. Game-thread only.
 TMap<FGuid,int32> NodeCounts;
 TSet<FGuid> AllForeignRoutes, BlockingRequestedRoutes;
 bool Indexed=false;
 const auto IndexRoster=[&]() {
  if(Indexed) return;
  Indexed=true;
  for(auto* Other:VS->GetDockingStations()) if(IsValid(Other)) ++NodeCounts.FindOrAdd(Other->GetPathNodeGUID());
  for(auto* Other:VS->GetAllVehicles()) {
   if(!IsValid(Other) || Other==V) continue;
   const auto& Route=Other->GetVehicleRoute();
   for(const auto& N:Route) AllForeignRoutes.Add(N);
   if(!Route.Contains(Wanted)) continue;
   const auto* OtherOwner=Other->GetOwnerVehicle();
   if(filn::player::ForeignRoute(true,Route.Num()==1 && Route[0]==Wanted,Other->IsAutopilotEnabled(),
       !IsValid(OtherOwner) || OtherOwner->GetDriver()!=nullptr,ByVehicle.Contains(Other))) BlockingRequestedRoutes.Add(Wanted);
  }
 };
 auto Free=[&](AFGDockingStationIdentifier* D) {
  if(!IsValid(D) || !IsValid(D->GetStation()) || !IsDepot(D) ||
     HomeStations.Contains(TWeakObjectPtr<AFGDockingStationIdentifier>(D))) return false;
  auto* B=D->GetStation(); const auto Node=D->GetPathNodeGUID();
  if(!Node.IsValid() || (B->GetDockedActor() && B->GetDockedActor()!=O) ||
     !B->HasPower() || B->IsProductionPaused() || !ValidHome(D) || !VehicleStationCompatible(O,B)) return false;
  const auto* SavedId=StationIds.Find(TWeakObjectPtr<AFGDockingStationIdentifier>(D));
  if(SavedId && Reservations.StationBusy(FILNPlayerGuidKey(*SavedId))) return false;
  IndexRoster();
  const bool Foreign=D==Requested?BlockingRequestedRoutes.Contains(Node):AllForeignRoutes.Contains(Node);
  return filn::player::Free({true,true,true,false,false,false,NodeCounts.FindRef(Node)==1,Foreign});
 };
 AFGDockingStationIdentifier* Destination=nullptr;
 const bool RequestedClaimed=HomeStations.Contains(TWeakObjectPtr<AFGDockingStationIdentifier>(Requested));
 if(Free(Requested)) Destination=Requested;
 else if(RequestedClaimed) {
  if(Docking) { Pending(TEXT("Occupied depot: wait until docking finishes before relocation")); return false; }
  // Do not displace the existing owner. A two-node native route is necessary
  // for movement; a home-only route is only the final parked intent.
  if(!O->HasFuel() || !O->GetStorageInventory()->IsEmpty()) { Pending(TEXT("Occupied depot: overflow needs fuel and empty cargo")); return false; }
  const auto From=V->GetCurrentToPathNodeGUID(); auto* Preset=O->GetVehiclePathPreset();
  if(!From.IsValid() || !Preset) { Pending(TEXT("Occupied depot: no native road segment or vehicle preset")); return false; }
  int32& DepotSearchCursor=OverflowCursor.FindOrAdd(VehicleKey);
  int32 Queried=0;
  // Retain cursor across bounded passes: an unreachable early depot must
  // not starve a later free one. Pending cache work resumes on that candidate.
  for(int32 Visited=0;Visited<DepotStations.Num() && Queried<4;++Visited) {
   DepotSearchCursor%=DepotStations.Num();
   auto* Candidate=DepotStations[DepotSearchCursor].Get();
   if(Candidate==Requested || !Free(Candidate)) { ++DepotSearchCursor; continue; }
   ++Queried;
   const auto Node=Candidate->GetPathNodeGUID(); double A=-1,B=-1,C=-1;
   if(!RoadCache.Distance(VS,From,Node,Preset,A) || !RoadCache.Distance(VS,Node,Wanted,Preset,B) || !RoadCache.Distance(VS,Wanted,Node,Preset,C)) { Pending(TEXT("Occupied depot: directed road calculation pending")); return false; }
   ++DepotSearchCursor;
   if(A>=0 && B>=0 && C>=0) { Destination=Candidate; break; }
  }
  if(!Destination) Pending(TEXT("Occupied depot: no available reachable alternative in this pass; retrying"));
 }
 if(!Destination) { if(!RequestedClaimed) Pending(TEXT("Depot unavailable: power, standby, occupied, reservation or conflicting route")); return false; }
 // Rebuild the small index once immediately before mutation; a native road
 // change or second claimant must not sneak through the earlier search.
 Indexed=false; NodeCounts.Reset(); AllForeignRoutes.Reset(); BlockingRequestedRoutes.Reset();
 if(!Free(Destination) || O->GetDriver() || !V->IsAutopilotEnabled() || V->GetVehicleRoute()!=Before) return false;
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
 if(!Ownership.Pair(FILNPlayerGuidKey(R.TruckId),FILNPlayerGuidKey(R.BayId))) { Pending(TEXT("Exclusive depot ownership rejected")); return false; }
 if(!WatchTransfers(R)) {
  Ownership.Unpair(FILNPlayerGuidKey(R.TruckId),FILNPlayerGuidKey(R.BayId));
  Pending(TEXT("Native cargo transfer observer unavailable; enrollment rejected")); return false;
 }
 if(Move) {
  V->SetVehicleRoute(R.IdleRoute);
  if(V->GetVehicleRoute()==R.IdleRoute && V->IsAutopilotEnabled() && !O->GetDriver()) V->SetCurrentTargetWaypoint(0);
  if(V->GetVehicleRoute()!=R.IdleRoute || !V->IsAutopilotEnabled() || O->GetDriver() || V->GetCurrentTargetWaypointIndex()!=0) {
   if(!O->GetDriver() && V->IsAutopilotEnabled() && V->GetVehicleRoute()==R.IdleRoute) V->SetVehicleRoute(Before);
   if(V->GetVehicleRoute()!=Before || !V->IsAutopilotEnabled()) bDispatchBlocked=true;
   ForgetTransferObserver(R.TruckId);
   Ownership.Unpair(FILNPlayerGuidKey(R.TruckId),FILNPlayerGuidKey(R.BayId));
   Pending(TEXT("Native overflow route or waypoint readback failed")); return false;
  }
 }
 R.Diagnostic=Move?TEXT("Relocating to unclaimed depot; arrival pending"):TEXT("Automatically adopted player home-only route");
 HomeStations.Add(Destination); TrackDepot(Destination); ByVehicle.Add(V,Registrations.Add(R)); ResetScan();
 OverflowCursor.Remove(VehicleKey); PendingHomeReason.Remove(VehicleKey);
 UE_LOG(LogFILN,Log,TEXT("AUTO_HOME_ADOPT moving=%d truck=%s bay=%s"),Move,*R.TruckId.ToString(),*R.BayId.ToString());
 return true;
}
