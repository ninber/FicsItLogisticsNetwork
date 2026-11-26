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
 for(int32 Slot=0;Slot<I->GetSizeLinear();++Slot) if(I->GetAllowedItemOnIndex(Slot)!=C.OriginalFilters[Slot]) return false;
 return HasDockFilter(C);
}
void ALogisticsSubsystem::DiscoverDepots() {
 if(!HasAuthority() || !bRegistryValidated || bDispatchBlocked || bPreparingUninstall || bUninstallPrepared || bChangingFilters) return;
 const double Now=GetWorld()->GetTimeSeconds();
 if(PlayerStationCursor>=PlayerStationScan.Num()) {
  if(Now<NextPlayerStationScan) return;
  auto* VS=AFGVehicleSubsystem::Get(GetWorld()); if(!VS) return;
  PruneNetworkIdentity();
  PlayerStationScan.Reset(); for(auto* ID:VS->GetDockingStations()) if(IsValid(ID)) PlayerStationScan.Add(ID);
  PlayerStationCursor=0; NextPlayerStationScan=Now+10.;
 }
 for(int32 Work=0; Work<2 && PlayerStationCursor<PlayerStationScan.Num(); ++Work) {
  auto* ID=PlayerStationScan[PlayerStationCursor++].Get();
  if(!IsValid(ID)) continue;
  const bool Network=IsNetworkStation(ID);
  if(Network) NameNetworkStation(ID);
  UpdateNetworkPaint(ID,ID->GetStation(),Network);
  if(IsAuthoredDepot(ID) && !DepotIndex.Contains(TWeakObjectPtr<AFGDockingStationIdentifier>(ID))) {
   TrackDepot(ID);
   if(auto* B=ID->GetStation(); IsValid(B)) UE_LOG(LogFILN,Log,TEXT("FILN_DEPOT_DISCOVERED node=%s actor=%s class=%s valid_home=%d"),*ID->GetPathNodeGUID().ToString(),*B->GetPathName(),*B->GetClass()->GetPathName(),ValidHome(ID));
  }
 }
}
void ALogisticsSubsystem::DiscoverHomeVehicles() {
 if(!HasAuthority() || !bRegistryValidated || bDispatchBlocked || bPreparingUninstall || bUninstallPrepared || bChangingFilters) return;
 const double Now=GetWorld()->GetTimeSeconds();
 auto* VS=AFGVehicleSubsystem::Get(GetWorld()); if(!VS) return;
 if(PlayerVehicleCursor>=PlayerVehicleScan.Num()) {
  if(Now<NextPlayerVehicleScan) return;
  PlayerVehicleScan.Reset(); for(auto* V:VS->GetAllVehicles()) if(IsValid(V)) PlayerVehicleScan.Add(V);
  for(auto It=OverflowCursor.CreateIterator();It;++It) if(!It.Key().IsValid()) It.RemoveCurrent();
  for(auto It=PendingHomeReason.CreateIterator();It;++It) if(!It.Key().IsValid()) It.RemoveCurrent();
  for(auto It=DepotCapacityWarnings.CreateIterator();It;++It) if(!(*It).IsValid()) It.RemoveCurrent();
  PendingDepotMembership.RemoveAll([](const auto& V){return !IsValid(V);});
  PendingDepotRouteMembership.RemoveAll([](const auto& V){return !IsValid(V);});
  DepotArrivalSuppressed.RemoveAll([](const auto& V){return !IsValid(V);});
  PlayerVehicleCursor=0; NextPlayerVehicleScan=Now+5.;
 }
 for(int32 Work=0;Work<4 && PlayerVehicleCursor<PlayerVehicleScan.Num();++Work) {
  auto* V=PlayerVehicleScan[PlayerVehicleCursor++].Get();
  auto* O=IsValid(V)?V->GetOwnerVehicle():nullptr;
  if(!IsValid(O) || !O->HasAuthority()) continue;
  const auto Route=V->GetVehicleRoute();
  AFGDockingStationIdentifier* Requested=nullptr; AFGDockingStationIdentifier* Arrived=nullptr;
  bool Ambiguous=false, ArrivalAmbiguous=false;
  for(const auto& D:DepotStations) {
   if(!IsValid(D) || !IsValid(D->GetStation()) || !D->GetPathNodeGUID().IsValid()) continue;
   if(Route.Contains(D->GetPathNodeGUID())) {
    if(Requested && Requested!=D.Get()) Ambiguous=true;
    Requested=D.Get();
   }
   if(D->GetStation()->GetDockedActor()==O) {
    if(Arrived && Arrived!=D.Get()) ArrivalAmbiguous=true;
    Arrived=D.Get();
   }
  }
  const bool HasDepot=Requested!=nullptr;
  if(const auto* Index=ByVehicle.Find(V)) {
   if(!Registrations.IsValidIndex(*Index)) { RequestRegistryRepair(); continue; }
   SetDepotCapacityWarning(V,false);
   auto& R=Registrations[*Index];
   const bool OptOut=filn::player::RemovedDepot(HasDepot,R.AwaitingHomeIntent,Route==R.IdleRoute);
   const bool Reassigned=HasDepot && (!IsValid(R.Home) || (!Route.Contains(R.Home->GetPathNodeGUID()) && !R.AwaitingHomeIntent));
   if(OptOut || Reassigned) {
    const FGuid Truck=R.TruckId;
    if(RemoveRegistrationAt(*Index)) {
     PendingDepotMembership.Remove(V); PendingDepotRouteMembership.Remove(V); PendingHomeReason.Remove(V); OverflowCursor.Remove(V);
     if(!HasDepot && Arrived) DepotArrivalSuppressed.AddUnique(V);
     UE_LOG(LogFILN,Log,TEXT("FILN_ROUTE_UNREGISTER truck=%s; native route, AP and cargo unchanged"),*Truck.ToString());
    }
   } else if(!R.JobId.IsValid() && R.State==static_cast<uint8>(filn::State::Idle) && HasDepot && Route!=R.IdleRoute) {
    // The player kept this home but changed the idle route. Reconcile it
    // rather than leaving a permanently stuck registered vehicle.
    R.IdleRoute=Route; R.Held=false; R.Relocating=true; R.AwaitingHomeIntent=false;
    R.NavigationRetry.Reset(); R.TravelProgress.Reset(); ResetScan();
   }
   if(!OptOut && !Reassigned) NameNetworkVehicle(V);
   UpdateNetworkPaint(V,O,ByVehicle.Contains(V));
   continue;
  }
  if(!HasDepot && PendingDepotRouteMembership.Contains(V)) {
   PendingDepotMembership.Remove(V); PendingDepotRouteMembership.Remove(V);
   if(Arrived) DepotArrivalSuppressed.AddUnique(V);
  }
  if(!Arrived || HasDepot) DepotArrivalSuppressed.Remove(V);
  if(!filn::player::Membership(true,HasDepot,Arrived!=nullptr,DepotArrivalSuppressed.Contains(V))) {
   PendingDepotMembership.Remove(V); PendingHomeReason.Remove(V); OverflowCursor.Remove(V); SetDepotCapacityWarning(V,false); UpdateNetworkPaint(V,O,false); continue;
  }
  PendingDepotMembership.AddUnique(V);
  NameNetworkVehicle(V);
  UpdateNetworkPaint(V,O,true);
  if(HasDepot) PendingDepotRouteMembership.AddUnique(V);
  if(ArrivalAmbiguous || (Ambiguous && (!Arrived || !Route.Contains(Arrived->GetPathNodeGUID())))) {
   SetDepotCapacityWarning(V,false);
   PendingHomeReason.Add(V,TEXT("Multiple FILN depots in route; no unique arrived home")); continue;
  }
  if(Arrived && (!HasDepot || Route.Contains(Arrived->GetPathNodeGUID()))) Requested=Arrived;
  if(Requested && AdoptPlayerHome(V,Requested)) { PendingDepotMembership.Remove(V); PendingDepotRouteMembership.Remove(V); }
 }
}
bool ALogisticsSubsystem::TransitionUsesNode(const FGuid& Node) const {
 if(!Node.IsValid()) return false;
 for(const auto& V:PendingDepotMembership)
  if(IsValid(V) && V->GetVehicleRoute().Contains(Node)) return true;
 for(const auto& R:Registrations)
  if(R.Relocating && IsValid(R.Vehicle) && R.Vehicle->GetVehicleRoute().Contains(Node)) return true;
 return false;
}
bool ALogisticsSubsystem::AdoptPlayerHome(AFGWheeledVehicleIdentifier* V,AFGDockingStationIdentifier* Requested) {
 check(IsInGameThread());
 if(!IsValid(V)) return false;
 const TWeakObjectPtr<AFGWheeledVehicleIdentifier> VehicleKey(V);
 const auto Pending=[&](const FString& Why) {
  auto& Previous=PendingHomeReason.FindOrAdd(VehicleKey);
  if(Previous!=Why) { Previous=Why; UE_LOG(LogFILN,Warning,TEXT("AUTO_HOME_PENDING vehicle=%s reason=%s"),*V->GetPathName(),*Why); }
 };
 if(!IsValid(Requested) || !IsDepot(Requested)) { SetDepotCapacityWarning(V,false); Pending(TEXT("Assigned station is not a recognized FILN depot")); return false; }
 if(!ValidHome(Requested)) { SetDepotCapacityWarning(V,false); Pending(TEXT("Depot requires LOAD mode, empty cargo, docking node and fuel-only connections")); return false; }
 auto* O=V->GetOwnerVehicle(); auto* VS=AFGVehicleSubsystem::Get(GetWorld());
 if(!IsValid(O) || !VS || !O->GetStorageInventory()) { SetDepotCapacityWarning(V,false); Pending(TEXT("Native vehicle owner, subsystem or cargo inventory unavailable")); return false; }
 if(!VehicleStationCompatible(O,Requested->GetStation())) { SetDepotCapacityWarning(V,false); Pending(TEXT("Vehicle and depot cargo media are incompatible")); return false; }
 // Capacity is independent of AP, fuel and reachability. A physically free
 // compatible bay may still be unpowered or unreachable; report those separately.
 bool HasVacantBay=false;
 for(const auto& D:DepotStations) {
  auto* B=IsValid(D)?D->GetStation():nullptr;
  if(!IsValid(B)) continue;
  if(filn::player::VacantBay(VehicleStationCompatible(O,B),ValidHome(D),HomeStations.Contains(TWeakObjectPtr<AFGDockingStationIdentifier>(D.Get())),B->GetDockedActor() && B->GetDockedActor()!=O)) { HasVacantBay=true; break; }
 }
 SetDepotCapacityWarning(V,!HasVacantBay);
 if(!HasVacantBay) { Pending(TEXT("No free compatible depot; build or free a bay. Automatic retry active.")); return false; }
 const auto Before=V->GetVehicleRoute();
 const FGuid Wanted=Requested->GetPathNodeGUID();
 const bool Arrived=Requested->GetStation()->GetDockedActor()==O;
 if(!O->HasAuthority() || ByVehicle.Contains(V) || (!Before.Contains(Wanted) && !Arrived)) return false;
 if(!Wanted.IsValid()) { Pending(TEXT("Depot path node invalid")); return false; }
 // A stopped truck actually docked at its requested home can be adopted
 // without route writes; a truck docking elsewhere is never commandeered.
 const bool Docking=V->IsCurrentlyDocking();
 // Membership is independent of docking and motion; park only after a safe transition.
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
   if(ByVehicle.Contains(Other) || !IsValid(OtherOwner)) BlockingRequestedRoutes.Add(Wanted);
  }
 };
 auto Free=[&](AFGDockingStationIdentifier* D) {
  if(!IsValid(D) || !IsValid(D->GetStation()) || !IsDepot(D) ||
     HomeStations.Contains(TWeakObjectPtr<AFGDockingStationIdentifier>(D))) return false;
  auto* B=D->GetStation(); const auto Node=D->GetPathNodeGUID();
  if(!Node.IsValid() || (B->GetDockedActor() && B->GetDockedActor()!=O) ||
     (D!=Requested && (!B->HasPower() || B->IsProductionPaused())) || !ValidHome(D) || !VehicleStationCompatible(O,B)) return false;
  const auto* SavedId=StationIds.Find(TWeakObjectPtr<AFGDockingStationIdentifier>(D));
  if(SavedId && Reservations.StationBusy(FILNPlayerGuidKey(*SavedId))) return false;
  IndexRoster();
  const bool Foreign=D==Requested?BlockingRequestedRoutes.Contains(Node):AllForeignRoutes.Contains(Node);
  return filn::player::Free({true,true,true,false,false,false,NodeCounts.FindRef(Node)==1,Foreign});
 };
 AFGDockingStationIdentifier* Destination=nullptr;
 const bool RequestedClaimed=HomeStations.Contains(TWeakObjectPtr<AFGDockingStationIdentifier>(Requested));
 if(Free(Requested)) Destination=Requested;
 else if(RequestedClaimed || (Requested->GetStation()->GetDockedActor() && Requested->GetStation()->GetDockedActor()!=O)) {
  if(Docking) { Pending(TEXT("Occupied depot: wait until docking finishes before relocation")); return false; }
  // Do not displace the existing owner. A two-node native route is necessary
  // for movement; a home-only route is only the final parked intent.
  if(!filn::player::MayMove(true,V->IsAutopilotEnabled(),O->GetDriver()!=nullptr) || !O->HasFuel()) { Pending(TEXT("Occupied depot: waiting for native AP and fuel to reach a free bay")); return false; }
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
 if(!Free(Destination) || V->GetVehicleRoute()!=Before) return false;
 const auto Node=Destination->GetPathNodeGUID();
 auto* H=Destination->GetStation();
 const FVector Delta=O->GetVehicleLocation()-H->GetDockingPathNode()->GetActorLocation();
 const bool AtHome=H->GetDockedActor()==O && filn::dispatch::HomePositionReady(Delta.SizeSquared(),Delta.Z,O->GetForwardSpeed(),false);
 FFILNRegistration R; R.Vehicle=V; R.Home=Destination; R.TruckId=FGuid::NewGuid(); R.BayId=StationId(Destination);
 R.IdleRoute=Before; R.Relocating=true; R.Held=false;
 // Arrival-only or overflow membership is valid before a home-containing route is committed.
 R.AwaitingHomeIntent=!Before.Contains(Node);
 if(!Ownership.Pair(FILNPlayerGuidKey(R.TruckId),FILNPlayerGuidKey(R.BayId))) { Pending(TEXT("Exclusive depot ownership rejected")); return false; }
 if(!WatchTransfers(R)) {
  Ownership.Unpair(FILNPlayerGuidKey(R.TruckId),FILNPlayerGuidKey(R.BayId));
  Pending(TEXT("Native cargo transfer observer unavailable; enrollment rejected")); return false;
 }
 R.Diagnostic=AtHome?TEXT("FILN membership recorded; awaiting parked home route"):TEXT("FILN membership recorded; automatic parking pending");
 HomeStations.Add(Destination); TrackDepot(Destination); ByVehicle.Add(V,Registrations.Add(R)); ResetScan();
 OverflowCursor.Remove(VehicleKey); PendingHomeReason.Remove(VehicleKey); SetDepotCapacityWarning(V,false);
 UE_LOG(LogFILN,Log,TEXT("AUTO_HOME_ADOPT parked=%d truck=%s bay=%s"),AtHome,*R.TruckId.ToString(),*R.BayId.ToString());
 return true;
}
