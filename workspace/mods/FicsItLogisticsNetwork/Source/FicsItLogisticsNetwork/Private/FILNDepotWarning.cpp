#include "LogisticsSubsystem.h"
#include "FGActorRepresentationManager.h"
#include "Representation/FGWheeledVehicleRepresentation.h"
#include "WheeledVehicles/FGWheeledVehicleIdentifier.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "Subsystem/SubsystemActorManager.h"
#include "Patching/NativeHookManager.h"
#include "Engine/World.h"

// AccessTransformers.ini grants ALogisticsSubsystem access to the protected
// native virtual. Use its declaring class directly: MSVC rejects the inherited
// using-declaration as a HookInvoker non-type template argument.
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
 using FWarningMethod=bool (UFGWheeledVehicleRepresentation::*)(AActor*,FLinearColor&) const;
 SUBSCRIBE_METHOD_EXPLICIT_VIRTUAL(FWarningMethod,UFGWheeledVehicleRepresentation::IsWarningIndicatorVisible,GetDefault<UFGWheeledVehicleRepresentation>(),
  ([](auto& Scope,const UFGWheeledVehicleRepresentation* Rep,AActor* Actor,FLinearColor& Color) {
   Scope(Rep,Actor,Color);
   if(!IsInGameThread() || !IsValid(Actor) || !Actor->HasAuthority()) return;
   auto* V=Cast<AFGWheeledVehicleIdentifier>(Actor);
   if(!V) if(auto* Owner=Cast<AFGWheeledVehicle>(Actor)) V=Owner->GetVehicleIdentifier();
   auto* W=Actor->GetWorld();
   auto* Manager=W?W->GetSubsystem<USubsystemActorManager>():nullptr;
   auto* S=Manager?Manager->GetSubsystemActor<ALogisticsSubsystem>():nullptr;
   if(S && S->HasDepotCapacityWarning(V)) {
    Color=FLinearColor(1.f,0.05f,0.025f,1.f); Scope.Override(true);
   }
  }));
}
