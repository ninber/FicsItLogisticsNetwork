#include "LogisticsSubsystem.h"
#include "FILNInventoryCapacity.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "WheeledVehicles/FGWheeledVehicleIdentifier.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "WheeledVehicles/FGVehiclePathNode.h"
#include "FGInventoryComponent.h"
#include "FGDockableInterface.h"
#include "Resources/FGItemDescriptor.h"
#include "Engine/World.h"

namespace {
struct FFuelSnapshot {
 filn::fuel::Snapshot Facts;
 TSubclassOf<UFGItemDescriptor> Item;
};
bool SingleFuel(UFGInventoryComponent* I,TSubclassOf<UFGItemDescriptor>& Item) {
 if(!IsValid(I)) return false;
 TArray<FInventoryStack> Stacks; I->GetInventoryStacks(Stacks);
 for(const auto& S:Stacks) if(S.NumItems>0) {
  if(!S.Item.GetItemClass() || (Item && Item!=S.Item.GetItemClass())) return false;
  Item=S.Item.GetItemClass();
 }
 return true;
}
FFuelSnapshot Inspect(const FFILNRegistration& R,double Now) {
 FFuelSnapshot Result; auto& F=Result.Facts;
 auto* V=R.Vehicle.Get(); auto* O=IsValid(V)?V->GetOwnerVehicle():nullptr;
 auto* H=IsValid(R.Home)?R.Home->GetStation():nullptr;
 if(!IsValid(O) || !IsValid(H) || !H->GetDockingPathNode() || !R.Held ||
    R.State!=static_cast<uint8>(filn::State::Idle) || O->GetDriver() || !V->IsAutopilotEnabled() ||
    !H->HasPower() || H->IsProductionPaused() || FMath::Abs(O->GetForwardSpeed())>1.f ||
    (H->GetDockedActor() && H->GetDockedActor()!=O) ||
    R.JobId.IsValid()) return Result;
 const FVector Delta=O->GetVehicleLocation()-H->GetDockingPathNode()->GetActorLocation();
 if(!filn::dispatch::HomePositionReady(Delta.SizeSquared(),Delta.Z,O->GetForwardSpeed(),H->GetDockedActor() && H->GetDockedActor()!=O)) return Result;
 const TArray<FGuid> HomeRoute={R.Home->GetPathNodeGUID()};
 if(V->GetVehicleRoute()!=HomeRoute || R.IdleRoute!=HomeRoute) return Result;
 auto* Source=H->GetFuelInventory(); auto* Target=O->GetFuelInventory();
 TSubclassOf<UFGItemDescriptor> Supplied,Carried;
 if(!SingleFuel(Source,Supplied)||!SingleFuel(Target,Carried)) return Result;
 Result.Item=Carried?Carried:Supplied;
 if(!Result.Item) return Result;
 F.sameFuel=!Carried || !Supplied || Carried==Supplied;
 F.supplied=static_cast<bool>(Supplied); F.pending=R.FuelPending; F.cooldown=Now<R.FuelRetryAt;
 F.quantity=Target->GetNumItems(Result.Item);
 int64 Capacity=0;
 if(!filn::inventory::TryCapacity(Target,Result.Item,Capacity)) return Result;
 if(Capacity<=0 || Capacity>MAX_int32) return Result;
 F.capacity=static_cast<int>(Capacity); F.valid=true;
 return Result;
}
}
bool ALogisticsSubsystem::FuelReady(const FFILNRegistration& R) const {
 auto* O=IsValid(R.Vehicle)?R.Vehicle->GetOwnerVehicle():nullptr;
 auto* H=IsValid(R.Home)?R.Home->GetStation():nullptr;
 return VehicleStationCompatible(O,H) && filn::fuel::Decide(Inspect(R,GetWorld()->GetTimeSeconds()).Facts)==filn::fuel::Action::Ready;
}
void ALogisticsSubsystem::ServiceFuel() {
 if(!HasAuthority() || bDispatchBlocked) return;
 const double Now=GetWorld()->GetTimeSeconds();
 for(int32 Count=0, Budget=FMath::Min(4,Registrations.Num());Count<Budget;++Count) {
  FuelCursor%=Registrations.Num(); auto& R=Registrations[FuelCursor++];
  auto* V=R.Vehicle.Get(); auto* O=IsValid(V)?V->GetOwnerVehicle():nullptr;
  auto* H=IsValid(R.Home)?R.Home->GetStation():nullptr;
  if(!VehicleStationCompatible(O,H) && !R.FuelPending) continue;
  const auto Snapshot=Inspect(R,Now);
  if(R.FuelPending) {
   const bool Valid=Snapshot.Facts.valid && ValidHome(R.Home) && R.ObservedFuel && IsValid(O) && O->GetFuelInventory();
   const int After=Valid?O->GetFuelInventory()->GetNumItems(R.ObservedFuel):0;
   // Only credit a measured increase while this truck still owns the docking.
   const bool Owned=Valid && IsValid(H) && H->GetDockedActor()==O;
   const auto Receipt=filn::fuel::Observe(Valid,Owned,R.FuelBefore,After,Now-R.FuelStarted);
   if(Receipt==filn::fuel::Receipt::Pending) continue;
   if(Receipt==filn::fuel::Receipt::Received) {
    R.Diagnostic=FString::Printf(TEXT("Native stationary fuel received: %d -> %d"),R.FuelBefore,After);
    UE_LOG(LogFILN,Log,TEXT("DEPOT_NATIVE_FUEL_RECEIVED %s"),*R.Diagnostic);
   } else R.Diagnostic=TEXT("Native fuel service ended without verified receipt; no synthetic fuel");
   R.FuelPending=false; R.ObservedFuel=nullptr; R.FuelRetryAt=Now+15.; continue;
  }
  if(!ValidHome(R.Home) || !VehicleStationCompatible(O,H) || filn::fuel::Decide(Snapshot.Facts)!=filn::fuel::Action::RequestDock) continue;
  // No inventory writes or fuel-type overrides: native docking transfers fuel.
  if(!H->GetDockedActor() && (!IFGDockableInterface::Execute_CanDock(O,H) || !H->DockActor(O))) { R.FuelRetryAt=Now+15.; continue; }
  if(H->GetDockedActor()!=O || !V->IsAutopilotEnabled() || O->GetDriver() || V->GetVehicleRoute()!=R.IdleRoute) {
   R.FuelRetryAt=Now+15.; continue;
  }
  R.FuelPending=true; R.ObservedFuel=Snapshot.Item; R.FuelBefore=Snapshot.Facts.quantity;
  R.FuelStarted=Now; R.Diagnostic=TEXT("Native stationary refueling pending measured receipt");
 }
}
