// SPDX-License-Identifier: GPL-3.0-only
#include "LogisticsSubsystem.h"
#include "WheeledVehicles/FGVehicleAutopilotComponent.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "WheeledVehicles/FGWheeledVehicleIdentifier.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "WheeledVehicles/FGVehiclePathNode.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "Subsystem/SubsystemActorManager.h"
#include "Patching/NativeHookManager.h"
#include "Engine/World.h"

bool ALogisticsSubsystem::HoldVehicle(AFGWheeledVehicleIdentifier* V) {
 if(!HasAuthority() || !IsInGameThread() || !bRegistryValidated || bPreparingUninstall) return false;
 const auto* Index=ByVehicle.Find(V); if(!Index || !Registrations.IsValidIndex(*Index)) return false;
 auto& R=Registrations[*Index]; auto* VehicleActor=IsValid(V)?V->GetOwnerVehicle():nullptr;
 auto* Home=IsValid(R.Home)?R.Home->GetStation():nullptr;
 const auto Phase=static_cast<filn::State>(R.State);
 const bool Arrival=R.JobId.IsValid() && R.Automatic && IsValid(Home) &&
  filn::recovery::CanFinalizeHome(Phase==filn::State::Returning,Phase==filn::State::Unloading,R.DeliveredCargo>0,R.AbortReturn,Home->GetDockedActor()==VehicleActor);
 const bool RelocationArrival=R.Relocating && Phase==filn::State::Idle && !R.JobId.IsValid() && IsValid(Home) && IsValid(VehicleActor) && Home->GetDockedActor()==VehicleActor && R.IdleRoute.Num()==2 && R.IdleRoute[0]==R.Home->GetPathNodeGUID() && R.IdleRoute[1]!=R.IdleRoute[0] && V->GetVehicleRoute()==R.IdleRoute;
 if(RelocationArrival) return VehicleStationCompatible(VehicleActor,Home) && V->IsAutopilotEnabled() && !VehicleActor->GetDriver() && FMath::Abs(VehicleActor->GetForwardSpeed())<=1.f;
 const bool Idle=R.Held && !R.Relocating && Phase==filn::State::Idle && !R.JobId.IsValid();
 if(!Idle && !Arrival) return false;
 if(!IsValid(VehicleActor) || VehicleActor->GetDriver() || !V->IsAutopilotEnabled() || !IsValid(Home) || !Home->GetDockingPathNode() || !VehicleStationCompatible(VehicleActor,Home)) { R.Held=false; return false; }
 if(Arrival) {
  // Hold a verified physical home arrival until the bounded watchdog reads
  // the final cargo ledger and restores the home-only route. Do not let
  // the native cyclic route depart for another pickup between observations.
  if(R.RouteIntent.IsEmpty() || V->GetVehicleRoute()!=R.RouteIntent) return false;
 } else if(V->GetVehicleRoute()!=R.IdleRoute || R.IdleRoute.Num()!=1 || R.IdleRoute[0]!=R.Home->GetPathNodeGUID()) { R.Held=false; return false; }
 // O(1) for a held truck, with no road or world queries in the tick hook.
 // A vehicle moved away from its bay must not remain frozen by a saved Held bit.
 const FVector Delta=VehicleActor->GetVehicleLocation()-Home->GetDockingPathNode()->GetActorLocation();
 if(!filn::dispatch::HomePositionReady(Delta.SizeSquared(),Delta.Z,VehicleActor->GetForwardSpeed(),Home->GetDockedActor() && Home->GetDockedActor()!=VehicleActor)) { R.Held=false; return false; }
 return true;
}
void ALogisticsSubsystem::InstallParkingHook() {
 if(WITH_EDITOR) return;
 SUBSCRIBE_METHOD(UFGVehicleAutopilotComponent::ShouldTickAutopilotComponent,[](auto& Scope,const UFGVehicleAutopilotComponent* Component) {
  const bool Native=Scope(Component);
  if(!Native || !IsInGameThread()) return;
  auto* V=Cast<AFGWheeledVehicle>(Component->GetOwner()); if(!V || !V->HasAuthority()) return;
  auto* World=V->GetWorld(); auto* Manager=World?World->GetSubsystem<USubsystemActorManager>():nullptr;
  auto* Logistics=Manager?Manager->GetSubsystemActor<ALogisticsSubsystem>():nullptr;
  // Only registered parked vehicles or measured final home arrivals are held.
  // Preserve the native route/AP until the authoritative watchdog reconciles it.
  // No per-frame world scan, path query, synthetic path or control-input injection.
  if(Logistics && Logistics->HoldVehicle(V->GetVehicleIdentifier())) Scope.Override(false);
 });
}
