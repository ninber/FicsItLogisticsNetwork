// SPDX-License-Identifier: GPL-3.0-only
#include "LogisticsSubsystem.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "WheeledVehicles/FGWheeledVehicleIdentifier.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "WheeledVehicles/FGVehiclePathNode.h"
#include "FGInventoryComponent.h"

void ALogisticsSubsystem::ServiceFuel() {
 if(!HasAuthority() || bDispatchBlocked) return;
 for(int32 Count=0;Count<4 && !Registrations.IsEmpty();++Count) {
  FuelCursor%=Registrations.Num(); auto& R=Registrations[FuelCursor++];
  auto* Owner=IsValid(R.Vehicle)?R.Vehicle->GetOwnerVehicle():nullptr;
  auto* Home=IsValid(R.Home)?R.Home->GetStation():nullptr;
  if(R.State!=static_cast<uint8>(filn::State::Idle) || !IsValid(Owner) || !IsValid(Home) || !ValidHome(R.Home) ||
     Owner->GetDriver() || !R.Vehicle->IsAutopilotEnabled() || !Home->HasPower() || Home->IsProductionPaused() ||
     !Home->GetFuelInventory() || Home->GetFuelInventory()->IsEmpty() || !Owner->GetFuelInventory() ||
     !Home->GetDockingPathNode() || FMath::Abs(Owner->GetForwardSpeed())>1.f || Home->GetDockedActor()) continue;
  FVector Location; FRotator Rotation; Owner->GetVehicleLocationAndRotation(Location,Rotation);
  if(FVector::DistSquared(Location,Home->GetDockingPathNode()->GetActorLocation())>FMath::Square(500.f)) continue;
  // Let the game's ordinary docking/fuel inventory transfer and burn logic do all work.
  // No AddStack, fabricated fuel, inventory copy, consumption override or route rewrite.
  if(Home->DockActor(Owner) && Home->GetDockedActor()==Owner) R.Diagnostic=TEXT("Native home docking accepted for supplied-fuel service; transfer not yet measured");
 }
}
