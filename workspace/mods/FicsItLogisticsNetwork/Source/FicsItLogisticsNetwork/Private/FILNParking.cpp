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
 if(!HasAuthority() || !IsInGameThread() || !bRegistryValidated) return false;
 const auto* Index=ByVehicle.Find(V); if(!Index || !Registrations.IsValidIndex(*Index)) return false;
 auto& R=Registrations[*Index]; auto* Owner=IsValid(V)?V->GetOwnerVehicle():nullptr;
 auto* Home=IsValid(R.Home)?R.Home->GetStation():nullptr;
 if(!R.Held || R.State!=static_cast<uint8>(filn::State::Idle) || R.JobId.IsValid()) return false;
 if(!IsValid(Owner) || Owner->GetDriver() || !V->IsAutopilotEnabled() || !IsValid(Home) || !Home->GetDockingPathNode() ||
    V->GetVehicleRoute()!=R.IdleRoute || R.IdleRoute.Num()!=1 || R.IdleRoute[0]!=R.Home->GetPathNodeGUID()) { R.Held=false; return false; }
 // O(1) for a held truck, with no road or world queries in the tick hook.
 // A vehicle moved away from its bay must not remain frozen by a saved Held bit.
 const FVector Delta=Owner->GetVehicleLocation()-Home->GetDockingPathNode()->GetActorLocation();
 if(!filn::dispatch::HomePositionReady(Delta.SizeSquared(),Delta.Z,Owner->GetForwardSpeed(),Home->GetDockedActor() && Home->GetDockedActor()!=Owner)) { R.Held=false; return false; }
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
  // Only explicitly enrolled idle vehicles held at their home are excluded. Their
  // valid native route and AP setting are preserved, and released by job reservation.
  // No per-frame world scan, path query, synthetic path or control-input injection.
  if(Logistics && Logistics->HoldVehicle(V->GetVehicleIdentifier())) Scope.Override(false);
 });
}
