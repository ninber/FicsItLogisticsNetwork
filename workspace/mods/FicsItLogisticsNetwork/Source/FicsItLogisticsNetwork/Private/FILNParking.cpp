// SPDX-License-Identifier: GPL-3.0-only
#include "LogisticsSubsystem.h"
#include "WheeledVehicles/FGVehicleAutopilotComponent.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "WheeledVehicles/FGWheeledVehicleIdentifier.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "Subsystem/SubsystemActorManager.h"
#include "Patching/NativeHookManager.h"
#include "Engine/World.h"

bool ALogisticsSubsystem::HoldVehicle(AFGWheeledVehicleIdentifier* V) {
 if(!HasAuthority() || !IsInGameThread()) return false;
 const auto* Index=ByVehicle.Find(V); if(!Index) return false;
 auto& R=Registrations[*Index]; auto* Owner=IsValid(V)?V->GetOwnerVehicle():nullptr;
 if(!IsValid(Owner) || Owner->GetDriver() || V->GetVehicleRoute()!=R.IdleRoute || !IsValid(R.Home)) { R.Held=false; return false; }
 return R.Held && R.State==static_cast<uint8>(filn::State::Idle) && V->IsAutopilotEnabled();
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
  // No per-frame world scan, position poll, synthetic path or control-input injection.
  if(Logistics && Logistics->HoldVehicle(V->GetVehicleIdentifier())) Scope.Override(false);
 });
}
