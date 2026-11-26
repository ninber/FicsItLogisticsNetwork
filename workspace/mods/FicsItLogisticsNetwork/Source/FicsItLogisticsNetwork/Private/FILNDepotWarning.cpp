// SPDX-License-Identifier: GPL-3.0-only
#include "LogisticsSubsystem.h"
#include "FGActorRepresentationManager.h"
#include "Representation/FGWheeledVehicleRepresentation.h"
#include "WheeledVehicles/FGWheeledVehicleIdentifier.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "Subsystem/SubsystemActorManager.h"
#include "Patching/NativeHookManager.h"
#include "Engine/World.h"

// The native virtual is protected. Expose its inherited member pointer for
// SML's virtual hook without constructing a derived UObject or replacing a
// representation. No changes to vanilla map marker storage.
namespace {
struct FFILNVehicleWarningAccess : UFGWheeledVehicleRepresentation {
 using UFGWheeledVehicleRepresentation::IsWarningIndicatorVisible;
};
}
bool ALogisticsSubsystem::HasDepotCapacityWarning(AFGWheeledVehicleIdentifier* V) const {
 return IsValid(V) && PendingDepotMembership.Contains(V) && !ByVehicle.Contains(V) &&
  DepotCapacityWarnings.Contains(TWeakObjectPtr<AFGWheeledVehicleIdentifier>(V));
}
void ALogisticsSubsystem::SetDepotCapacityWarning(AFGWheeledVehicleIdentifier* V,bool Warning) {
 if(!IsValid(V) || !HasAuthority() || !IsInGameThread()) return;
 const TWeakObjectPtr<AFGWheeledVehicleIdentifier> Key(V);
 const bool Was=DepotCapacityWarnings.Contains(Key);
 if(Was==Warning) return;
 if(Warning) DepotCapacityWarnings.Add(Key); else DepotCapacityWarnings.Remove(Key);
 // Update the existing moving vehicle representation once on transition.
 // The native warning representation owns its visual and replication.
 if(auto* Manager=AFGActorRepresentationManager::Get(GetWorld()))
  Manager->UpdateRepresentationOfActor(V);
}
void ALogisticsSubsystem::InstallDepotWarningHook() {
 if(WITH_EDITOR) return;
 SUBSCRIBE_METHOD_VIRTUAL(FFILNVehicleWarningAccess::IsWarningIndicatorVisible,GetDefault<UFGWheeledVehicleRepresentation>(),
  ([](auto& Scope,const UFGWheeledVehicleRepresentation* Rep,AActor* Actor,FLinearColor& Color) {
   const bool Native=Scope(Rep,Actor,Color);
   if(!IsInGameThread() || !IsValid(Actor) || !Actor->HasAuthority()) return;
   auto* V=Cast<AFGWheeledVehicleIdentifier>(Actor);
   if(!V) if(auto* Owner=Cast<AFGWheeledVehicle>(Actor)) V=Owner->GetVehicleIdentifier();
   auto* W=Actor->GetWorld();
   auto* Manager=W?W->GetSubsystem<USubsystemActorManager>():nullptr;
   auto* S=Manager?Manager->GetSubsystemActor<ALogisticsSubsystem>():nullptr;
   if(S && S->HasDepotCapacityWarning(V)) {
    Color=FLinearColor(1.f,0.05f,0.025f,1.f); Scope.Override(true);
   } else Scope.Override(Native);
  }));
}
