// SPDX-License-Identifier: GPL-3.0-only
#include "LogisticsSubsystem.h"
#include "FILNRehomePolicy.h"
#include "WheeledVehicles/FGWheeledVehicleIdentifier.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "WheeledVehicles/FGVehiclePathNode.h"
#include "FGInventoryComponent.h"
#include "Engine/World.h"

bool ALogisticsSubsystem::RetryNavigation(FFILNRegistration& R,double Now,bool Stalled) {
 using S=filn::State;
 using A=filn::recovery::Action;
 check(IsInGameThread());
 if(!HasAuthority() || bDispatchBlocked || !R.Automatic || !R.JobId.IsValid() || !IsValid(R.Vehicle)) return false;
 auto* V=R.Vehicle.Get(); auto* VehicleActor=V->GetOwnerVehicle();
 if(!IsValid(VehicleActor) || !VehicleActor->GetStorageInventory() || !R.Resource) return false;
 const auto Phase=static_cast<S>(R.State);
 auto* Target=Phase==S::ToProvider?R.Provider.Get():Phase==S::ToRequester?R.Requester.Get():Phase==S::Returning?R.Home.Get():nullptr;
 if(!IsValid(Target) || !IsValid(Target->GetStation())) return false;
 const FGuid Destination=Target->GetPathNodeGUID();
 const int32 Waypoint=V->GetCurrentTargetWaypointIndex();
 const auto Matches=[&]() {
  return V->GetVehicleRoute()==R.RouteIntent && R.RouteIntent.IsValidIndex(Waypoint) &&
   Destination.IsValid() && R.RouteIntent[Waypoint]==Destination &&
   V->GetCurrentTargetWaypointIndex()==Waypoint;
 };
 const auto Docking=[&]() {
  if(V->IsCurrentlyDocking()) return true;
  for(auto* Endpoint:{R.Provider.Get(),R.Requester.Get(),R.Home.Get()})
   if(IsValid(Endpoint) && IsValid(Endpoint->GetStation()) && Endpoint->GetStation()->GetDockedActor()==VehicleActor) return true;
  return false;
 };
 // Observe() reconciles mixed cargo and measured transfer events before calling
 // us. Preserve that phase and reservation; never invent a pickup or delivery.
 auto& Clock=R.NavigationRetry;
 const filn::recovery::Snapshot Snapshot{
  V->IsAutopilotEnabled(),VehicleActor->GetDriver()!=nullptr,Matches(),true,Docking(),
  VehicleActor->GetStorageInventory()->GetNumItems(R.Resource)==R.LastCargo,
  VehicleActor->HasFuel(),Stalled || V->GetAutopilotErrorStatus()!=EVehicleAutopilotErrorStatus::None};
 const auto Action=filn::recovery::Decide(Snapshot,Clock,Now);
 if(Action==A::Stop || Action==A::DriverWait) return false;
 Clock.waiting=true;
 if(Action==A::FuelWait) { R.Diagnostic=TEXT("Navigation recovery: awaiting native fuel; no fuel synthesized"); return true; }
 if(Action==A::RetryWait || Action==A::Resume) return true;
 auto* VS=AFGVehicleSubsystem::Get(GetWorld());
 auto* Preset=VehicleActor->GetVehiclePathPreset();
 const FGuid From=V->GetCurrentToPathNodeGUID();
 if(!From.IsValid() || !VS || !Preset) {
  Clock.Attempt(Now); R.Diagnostic=TEXT("Navigation recovery: no current road segment; manual placement may be required"); return true;
 }
 double Cost=-1;
 if(!RoadCache.Distance(VS,From,Destination,Preset,Cost)) {
  R.Diagnostic=TEXT("Navigation recovery: directed road calculation pending"); return true;
 }
 Clock.Attempt(Now);
 if(Cost<0) { R.Diagnostic=TEXT("Navigation recovery: destination unreachable; retry scheduled"); return true; }
 // Revalidate immediately before the native write. Never opt an AP-off vehicle
 // back in, change the destination, or reset a vehicle to a guessed waypoint.
 if(VehicleActor->GetDriver() || !V->IsAutopilotEnabled() || !VehicleActor->HasFuel() || !Matches() || Docking()) return false;
 V->SetVehicleRoute(R.RouteIntent);
 if(VehicleActor->GetDriver() || !V->IsAutopilotEnabled() || V->GetVehicleRoute()!=R.RouteIntent) return false;
 V->SetCurrentTargetWaypoint(Waypoint);
 if(!Matches() || !V->IsAutopilotEnabled() || VehicleActor->GetDriver()) return false;
 R.Diagnostic=TEXT("Navigation recovery: native route/target reapplied and read back; awaiting actual fault clearance");
 return true;
}

bool ALogisticsSubsystem::RestoreHomeRoute(FFILNRegistration& R) {
 check(IsInGameThread());
 if(!HasAuthority() || !IsValid(R.Vehicle) || !IsValid(R.Home)) return false;
 auto* V=R.Vehicle.Get(); auto* O=V->GetOwnerVehicle(); auto* H=R.Home->GetStation();
 if(!IsValid(O) || !IsValid(H) || !O->GetStorageInventory() || !ValidHome(R.Home)) return false;
 if(!filn::recovery::SafeHomeCompletion(true,V->GetVehicleRoute()==R.RouteIntent,V->IsAutopilotEnabled(),
    O->GetDriver()!=nullptr,H->GetDockedActor()==O,FMath::Abs(O->GetForwardSpeed())<=1.f,
    R.Resource && O->GetStorageInventory()->GetNumItems(R.Resource)==R.LastCargo)) return false;
 const TArray<FGuid> HomeRoute={R.Home->GetPathNodeGUID()};
 if(!HomeRoute[0].IsValid()) return false;
 // Hold only after native route and AP readback; no repeated pickup on a completed trip.
 V->SetVehicleRoute(HomeRoute);
 if(V->GetVehicleRoute()!=HomeRoute || !V->IsAutopilotEnabled() || O->GetDriver()) {
  // Preserve the observed intended route if the native write succeeded but AP did not.
  if(V->GetVehicleRoute()==HomeRoute) R.RouteIntent=HomeRoute;
  return false;
 }
 R.RouteIntent=HomeRoute; R.IdleRoute=HomeRoute; R.Held=true;
 return true;
}

bool ALogisticsSubsystem::TryAbortToHome(FFILNRegistration& R,double Now) {
 check(IsInGameThread());
 if(!HasAuthority() || bDispatchBlocked || !R.Automatic || !R.JobId.IsValid() ||
    !IsValid(R.Vehicle) || !IsValid(R.Home) || !R.Resource || !ValidHome(R.Home)) return false;
 auto* V=R.Vehicle.Get(); auto* O=V->GetOwnerVehicle(); auto* H=R.Home->GetStation();
 if(!IsValid(O) || !IsValid(H) || !O->GetStorageInventory()) return false;
 auto* Cargo=O->GetStorageInventory();
 bool Verified=Cargo->GetNumItems(R.Resource)==R.LastCargo;
 TArray<FInventoryStack> Stacks; Cargo->GetInventoryStacks(Stacks);
 for(const auto& Stack:Stacks) if(Stack.NumItems>0 && Stack.Item.GetItemClass()!=R.Resource) Verified=false;
 bool Docking=V->IsCurrentlyDocking();
 for(auto* ID:{R.Provider.Get(),R.Requester.Get(),R.Home.Get()})
  if(IsValid(ID) && IsValid(ID->GetStation()) && ID->GetStation()->GetDockedActor()==O) Docking=true;
 if(!filn::recovery::CanAbortToHome(V->GetVehicleRoute()==R.RouteIntent,V->IsAutopilotEnabled(),O->GetDriver()!=nullptr,Docking,Verified,true)) return false;
 if(!Reservations.OwnsJob(TCHAR_TO_UTF8(*R.JobId.ToString()),TCHAR_TO_UTF8(*R.TruckId.ToString()))) return false;
 if(!R.ProviderLease.IsValid() && IsValid(R.Provider)) R.ProviderLease=StationId(R.Provider);
 if(!R.RequesterLease.IsValid() && IsValid(R.Requester)) R.RequesterLease=StationId(R.Requester);
 if(!R.RequesterLease.IsValid() || (!R.RetainedCargo && !R.ProviderLease.IsValid())) return false;
 // An invalid endpoint must not remain the target of an AP-on route while
 // a cache search is pending. For this exceptional recovery use the native
 // directed path query, or fail closed to explicit parked recovery.
 if(!O->HasFuel()) return false;
 auto* VS=AFGVehicleSubsystem::Get(GetWorld()); auto* Preset=O->GetVehiclePathPreset();
 const auto From=V->GetCurrentToPathNodeGUID(), Home=R.Home->GetPathNodeGUID();
 const auto Reachable=[&](const FGuid& A,const FGuid& B) {
  if(!VS || !Preset || !A.IsValid() || !B.IsValid()) return false;
  auto* Net=VS->FindNetworkByPathNodeGuid(A);
  if(!Net || Net!=VS->FindNetworkByPathNodeGuid(B)) return false;
  TArray<FGuid> Path;
  return A==B || Net->FindVehiclePath(A,B,Preset,Path);
 };
 if(!Reachable(From,Home)) return false;
 AFGDockingStationIdentifier* Anchor=nullptr;
 int32 Examined=0;
 // Prefer unclaimed designated rest stops; never use a cargo endpoint or
 // interrupt a different truck's home assignment as a recovery anchor.
 for(const auto& Depot:DepotStations) {
  if(++Examined>32) break;
  auto* ID=Depot.Get();
  if(!IsValid(ID) || ID==R.Home || HomeStations.Contains(TWeakObjectPtr<AFGDockingStationIdentifier>(ID)) || !ValidHome(ID) ||
     ID->GetPathNodeGUID()==Home || !IsValid(ID->GetStation()) || ID->GetStation()->GetDockedActor()) continue;
  if(Reachable(Home,ID->GetPathNodeGUID()) && Reachable(ID->GetPathNodeGUID(),Home)) { Anchor=ID; break; }
 }
 if(!Anchor) return false;
 if(O->GetDriver() || !V->IsAutopilotEnabled() || V->GetVehicleRoute()!=R.RouteIntent || V->IsCurrentlyDocking()) return false;
 const TArray<FGuid> Intent={Home,Anchor->GetPathNodeGUID()};
 V->SetVehicleRoute(Intent);
 if(V->GetVehicleRoute()!=Intent) return false;
 DisarmDockReceipts(R.JobId);
 R.RouteIntent=Intent; R.AbortReturn=true; R.State=static_cast<uint8>(filn::State::Returning);
 V->SetCurrentTargetWaypoint(0);
 if(V->GetCurrentTargetWaypointIndex()!=0 || V->GetVehicleRoute()!=Intent || !V->IsAutopilotEnabled() || O->GetDriver()) return false;
 R.TravelProgress.Reset(); R.Elapsed=0;
 R.Diagnostic=TEXT("Aborted shipment: native return route read back; cargo and leases retained until home");
 return true;
}

void ALogisticsSubsystem::AuditIdleFleet() {
 check(IsInGameThread());
 if(!HasAuthority() || bDispatchBlocked) return;
 const double Now=GetWorld()->GetTimeSeconds();
 for(int32 Count=0,Budget=FMath::Min(4,Registrations.Num());Count<Budget;++Count) {
  IdleCursor%=Registrations.Num(); auto& R=Registrations[IdleCursor++];
  if(R.State!=static_cast<uint8>(filn::State::Idle)) continue;
  if(!WatchTransfers(R)) { R.CargoOriginKnown=false; R.CargoOriginStorage=false; R.Held=false; continue; }
  auto* V=R.Vehicle.Get(); auto* O=IsValid(V)?V->GetOwnerVehicle():nullptr;
  auto* H=IsValid(R.Home)?R.Home->GetStation():nullptr;
  if(!IsValid(O) || !IsValid(H) || !H->GetDockingPathNode() || !O->GetStorageInventory()) { R.Held=false; continue; }
  const FGuid Home=R.Home->GetPathNodeGUID();
  const TArray<FGuid> Route={Home};
  const FVector Delta=O->GetVehicleLocation()-H->GetDockingPathNode()->GetActorLocation();
  const bool Near=FMath::Abs(Delta.Z)<=250.f && Delta.SizeSquared()<=FMath::Square(800.f);
  if(R.Relocating) {
   // Relocation is not a cargo job. Hold only on verified physical arrival;
   // never release the new exclusive bay merely because AP was disabled.
   if(!V->IsAutopilotEnabled() || O->GetDriver() || R.IdleRoute.Num()!=2 || R.IdleRoute[0]!=Home || R.IdleRoute[1]==Home || V->GetVehicleRoute()!=R.IdleRoute || !ValidHome(R.Home)) { R.Held=false; continue; }
   if(H->GetDockedActor()!=O || FMath::Abs(O->GetForwardSpeed())>1.f) {
    if(V->IsCurrentlyDocking() || !O->HasFuel() || (V->GetAutopilotErrorStatus()==EVehicleAutopilotErrorStatus::None && V->GetCurrentTargetWaypointIndex()==0) || !R.NavigationRetry.Due(Now)) continue;
    auto* VS=AFGVehicleSubsystem::Get(GetWorld()); auto* Preset=O->GetVehiclePathPreset();
    const auto From=V->GetCurrentToPathNodeGUID(); double Cost=-1;
    if(!From.IsValid() || !Preset || !VS) { R.NavigationRetry.Attempt(Now); continue; }
    if(!RoadCache.Distance(VS,From,Home,Preset,Cost)) continue;
    R.NavigationRetry.Attempt(Now);
    if(Cost<0 || O->GetDriver() || !V->IsAutopilotEnabled() || V->GetVehicleRoute()!=R.IdleRoute || V->IsCurrentlyDocking()) continue;
    V->SetVehicleRoute(R.IdleRoute);
    if(V->GetVehicleRoute()!=R.IdleRoute || !V->IsAutopilotEnabled() || O->GetDriver()) { bDispatchBlocked=true; continue; }
    V->SetCurrentTargetWaypoint(0);
    if(V->GetCurrentTargetWaypointIndex()!=0 || V->GetVehicleRoute()!=R.IdleRoute || !V->IsAutopilotEnabled() || O->GetDriver()) bDispatchBlocked=true;
    continue;
   }
   V->SetVehicleRoute(Route);
   if(V->GetVehicleRoute()!=Route || !V->IsAutopilotEnabled() || O->GetDriver()) {
    bDispatchBlocked=true; R.Held=false; R.Diagnostic=TEXT("Depot relocation completion readback failed"); continue;
   }
   R.IdleRoute=Route; R.Relocating=false; R.Held=true; R.NavigationRetry.Reset(); R.Diagnostic=TEXT("Relocated to free depot"); ResetScan(); continue;
  }
  const bool Owned=Home.IsValid() && R.IdleRoute==Route && V->GetVehicleRoute()==Route && !R.JobId.IsValid();
  const bool Docked=H->GetDockedActor()==O;
  const bool HomeValid=ValidHome(R.Home) && (!H->GetDockedActor() || Docked);
  const bool Fault=V->GetAutopilotErrorStatus()!=EVehicleAutopilotErrorStatus::None;
  const filn::idle::Snapshot Facts{Owned,true,V->IsAutopilotEnabled(),O->GetDriver()!=nullptr,HomeValid,
    Near,FMath::Abs(O->GetForwardSpeed())<=1.f,Docked,R.Held,O->GetStorageInventory()->IsEmpty(),Fault,O->HasFuel()};
  const auto Action=filn::idle::Evaluate(Facts);
  if(Action==filn::idle::Action::Hold) {
   if(!R.Held) R.Diagnostic=TEXT("Idle home-only vehicle returned to its own depot; hold restored");
   R.Held=true; R.NavigationRetry.Reset(); continue;
  }
  R.Held=false;
  if(Action==filn::idle::Action::Release) { R.NavigationRetry.Reset(); continue; }
  // Native AP remains in control, without re-enabling it or inventing a trip.
  // Carrying cargo is never a reason to rewrite an orphan's route.
  if(Action!=filn::idle::Action::RetryEmptyReturn) continue;
  auto& Clock=R.NavigationRetry;
  if(!Clock.Due(Now) || V->IsCurrentlyDocking()) continue;
  auto* VS=AFGVehicleSubsystem::Get(GetWorld()); auto* Preset=O->GetVehiclePathPreset();
  const FGuid From=V->GetCurrentToPathNodeGUID();
  if(!VS || !Preset || !From.IsValid()) { Clock.Attempt(Now); continue; }
  double Cost=-1;
  if(!RoadCache.Distance(VS,From,Home,Preset,Cost)) continue;
  Clock.Attempt(Now);
  if(Cost<0) { R.Diagnostic=TEXT("Idle home return: road unreachable; retry scheduled"); continue; }
  if(O->GetDriver() || !V->IsAutopilotEnabled() || V->GetVehicleRoute()!=Route ||
     V->IsCurrentlyDocking() || !O->GetStorageInventory()->IsEmpty()) continue;
  V->SetVehicleRoute(Route);
  if(V->GetVehicleRoute()!=Route || !V->IsAutopilotEnabled() || O->GetDriver()) {
   R.Diagnostic=TEXT("Idle home return: route readback failed; operator inspection required"); continue;
  }
  V->SetCurrentTargetWaypoint(0);
  R.Diagnostic=V->GetCurrentTargetWaypointIndex()==0?TEXT("Idle home-only route reapplied; actual arrival pending"):TEXT("Idle home return: target readback failed");
 }
}

bool ALogisticsSubsystem::RehomeParkedVehicle(int32 Index,AFGDockingStationIdentifier* Home) {
 check(IsInGameThread());
 using S=filn::State;
 if(!HasAuthority() || !bRegistryValidated || bDispatchBlocked || bPreparingUninstall || bChangingFilters || SchemaVersion!=1 || !Registrations.IsValidIndex(Index) ||
    !IsValid(Home) || Home->GetWorld()!=GetWorld() || !IsDepot(Home) || !ValidHome(Home)) return false;
 auto& R=Registrations[Index];
 auto* V=R.Vehicle.Get(); auto* O=IsValid(V)?V->GetOwnerVehicle():nullptr;
 auto* H=Home->GetStation(); auto* VS=AFGVehicleSubsystem::Get(GetWorld());
 if(!IsValid(O) || !O->HasAuthority() || !IsValid(H) || !VS || !O->GetStorageInventory()) return false;
 const auto Node=Home->GetPathNodeGUID(); if(!Node.IsValid()) return false;
 const auto Id=[](const FGuid& G){ return std::string(TCHAR_TO_UTF8(*G.ToString())); };
 const auto Truck=Id(R.TruckId), OldBay=Id(R.BayId);
 const FGuid Bay=StationId(Home);
 const auto NewBay=Id(Bay),Job=Id(R.JobId);
 bool Exclusive=Ownership.CanMove(Truck,OldBay,NewBay) && !Reservations.StationBusy(NewBay);
 for(const auto& Other:Registrations) if(&Other!=&R && Other.Home==Home) Exclusive=false;
 for(auto* Other:VS->GetAllVehicles()) if(IsValid(Other) && Other!=V && Other->GetVehicleRoute().Contains(Node)) Exclusive=false;
 for(auto* Other:VS->GetDockingStations()) if(IsValid(Other) && Other!=Home && Other->GetPathNodeGUID()==Node) Exclusive=false;
 const auto Phase=static_cast<S>(R.State);
 const bool LeaseOwned=R.JobId.IsValid()?Reservations.OwnsJob(Job,Truck):Phase==S::Idle;
 const filn::rehome::Snapshot Facts{true,true,Phase==S::Idle||Phase==S::Recovery,
   true,Exclusive,V->IsAutopilotEnabled(),O->GetDriver()!=nullptr,H->GetDockedActor()==O,
   FMath::Abs(O->GetForwardSpeed())<=1.f,LeaseOwned,true};
 if(!filn::rehome::Allowed(Facts)) return false;
 // Explicit administrator action after physical relocation. Never take a moving
 // truck or auto-enable AP. Cargo remains untouched and provenance is revoked.
 const TArray<FGuid> Before=V->GetVehicleRoute(), Intent={Node};
 V->SetVehicleRoute(Intent);
 if(V->GetVehicleRoute()!=Intent || V->IsAutopilotEnabled() || O->GetDriver() || H->GetDockedActor()!=O) {
  if(!V->IsAutopilotEnabled() && !O->GetDriver() && V->GetVehicleRoute()==Intent) V->SetVehicleRoute(Before);
  if(V->GetVehicleRoute()!=Before || V->IsAutopilotEnabled()) bDispatchBlocked=true;
  R.Diagnostic=TEXT("Explicit rehome route readback failed; original ownership and leases retained");
  return false;
 }
 if(!Ownership.Move(Truck,OldBay,NewBay)) {
  V->SetVehicleRoute(Before);
  if(V->GetVehicleRoute()!=Before || V->IsAutopilotEnabled()) bDispatchBlocked=true;
  return false;
 }
 if(IsValid(R.Home)) HomeStations.Remove(TWeakObjectPtr<AFGDockingStationIdentifier>(R.Home.Get()));
 R.Home=Home; R.BayId=Bay; HomeStations.Add(Home); TrackDepot(Home);
 if(R.JobId.IsValid()) { DisarmDockReceipts(R.JobId); Reservations.Release(Job); }
 R.JobId.Invalidate(); R.ProviderLease.Invalidate(); R.RequesterLease.Invalidate();
 R.Provider=nullptr; R.Requester=nullptr; R.RouteIntent.Empty(); R.IdleRoute=Intent;
 R.State=static_cast<uint8>(S::Idle); R.Held=false; R.Automatic=false;
 R.CargoOriginKnown=false; R.CargoOriginStorage=false; R.RetainedCargo=false;
 R.AbortReturn=false; R.Resource=nullptr;
 R.ExpectedCargo=R.LoadedCargo=R.DeliveredCargo=R.LastCargo=0;
 R.NavigationRetry.Reset(); R.TravelProgress.Reset(); R.FuelPending=false; R.ObservedFuel=nullptr;
 Active.Remove(Index); ResetScan();
 R.Diagnostic=TEXT("Explicit parked recovery: home ownership and route restored; AP remains off; cargo preserved");
 return true;
}
