// SPDX-License-Identifier: GPL-3.0-only
#include "LogisticsSubsystem.h"
#include "WheeledVehicles/FGWheeledVehicleIdentifier.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "FGInventoryComponent.h"
#include "Engine/World.h"

bool ALogisticsSubsystem::RetryNavigation(FFILNRegistration& R,double Now) {
 using S=filn::State;
 using A=filn::recovery::Action;
 check(IsInGameThread());
 if(!HasAuthority() || bDispatchBlocked || !R.Automatic || !R.JobId.IsValid() || !IsValid(R.Vehicle)) return false;
 auto* V=R.Vehicle.Get(); auto* Owner=V->GetOwnerVehicle();
 if(!IsValid(Owner) || !Owner->GetStorageInventory() || !R.Resource) return false;
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
   if(IsValid(Endpoint) && IsValid(Endpoint->GetStation()) && Endpoint->GetStation()->GetDockedActor()==Owner) return true;
  return false;
 };
 // Observe() reconciles mixed cargo and measured transfer events before calling
 // us. Preserve that phase and reservation; never invent a pickup or delivery.
 auto& Clock=R.NavigationRetry;
 const filn::recovery::Snapshot Snapshot{
  V->IsAutopilotEnabled(),Owner->GetDriver()!=nullptr,Matches(),true,Docking(),
  Owner->GetStorageInventory()->GetNumItems(R.Resource)==R.LastCargo,
  Owner->HasFuel(),V->GetAutopilotErrorStatus()!=EVehicleAutopilotErrorStatus::None};
 const auto Action=filn::recovery::Decide(Snapshot,Clock,Now);
 if(Action==A::Stop || Action==A::DriverWait) return false;
 Clock.waiting=true;
 if(Action==A::FuelWait) { R.Diagnostic=TEXT("Navigation recovery: awaiting native fuel; no fuel synthesized"); return true; }
 if(Action==A::RetryWait || Action==A::Resume) return true;
 auto* VS=AFGVehicleSubsystem::Get(GetWorld());
 auto* Preset=Owner->GetVehiclePathPreset();
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
 if(Owner->GetDriver() || !V->IsAutopilotEnabled() || !Owner->HasFuel() || !Matches() || Docking()) return false;
 V->SetVehicleRoute(R.RouteIntent);
 if(Owner->GetDriver() || !V->IsAutopilotEnabled() || V->GetVehicleRoute()!=R.RouteIntent) return false;
 V->SetCurrentTargetWaypoint(Waypoint);
 if(!Matches() || !V->IsAutopilotEnabled() || Owner->GetDriver()) return false;
 R.Diagnostic=TEXT("Navigation recovery: native route/target reapplied and read back; awaiting actual fault clearance");
 return true;
}
