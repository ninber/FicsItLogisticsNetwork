// SPDX-License-Identifier: GPL-3.0-only
#include "LogisticsSubsystem.h"
#include "FILNStationControls.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "WheeledVehicles/FGWheeledVehicleIdentifier.h"
#include "FGInventoryComponent.h"
#include "UObject/UnrealType.h"
#include "Resources/FGItemDescriptor.h"

bool ALogisticsSubsystem::RemoveStation(AFGDockingStationIdentifier* Station) {
 if(!HasAuthority() || bDispatchBlocked || !IsValid(Station)) return false;
 const int32 Index=StationConfigs.IndexOfByPredicate([Station](const FFILNStationConfig& C){return C.Station==Station;});
 if(Index==INDEX_NONE) return true;
 if(Reservations.StationBusy(TCHAR_TO_UTF8(*StationId(Station).ToString()))) return false;
 auto* B=Station->GetStation(); auto* I=IsValid(B)?B->GetInventory():nullptr;
 const auto& C=StationConfigs[Index];
 if(!I || B->GetDockedActor() || I->GetSizeLinear()!=C.OriginalFilters.Num()) return false;
 for(int32 Slot=0;Slot<I->GetSizeLinear();++Slot) if(I->GetAllowedItemOnIndex(Slot)!=C.Resource) return false;
 for(int32 Slot=0;Slot<I->GetSizeLinear();++Slot) I->SetAllowedItemOnIndex(Slot,C.OriginalFilters[Slot]);
 for(int32 Slot=0;Slot<I->GetSizeLinear();++Slot) if(I->GetAllowedItemOnIndex(Slot)!=C.OriginalFilters[Slot]) {
  bDispatchBlocked=true; UE_LOG(LogFILN,Error,TEXT("Station filter restoration readback failed; metadata retained")); return false;
 }
 if(auto* Marker=B->FindComponentByClass<UFILNStationMarker>()) Marker->DestroyComponent();
 StationConfigs.RemoveAt(Index); ResetScan(); return true;
}
void ALogisticsSubsystem::ImportLegacy(bool Apply) {
 if(!HasAuthority() || bDispatchBlocked || LegacyImportVersion>1) return;
 auto* VS=AFGVehicleSubsystem::Get(GetWorld()); if(!VS) return;
 struct FCandidate { AFGDockingStationIdentifier* Station; TSubclassOf<UFGItemDescriptor> Resource; };
 TArray<FCandidate> Candidates; TArray<FString> Report;
 for(auto* Station:VS->GetDockingStations()) {
  auto* B=IsValid(Station)?Station->GetStation():nullptr; if(!IsValid(B)) continue;
  const auto* Property=FindFProperty<FArrayProperty>(B->GetClass(),TEXT("FilterItem"));
  if(!Property) {
   const FString ClassPath=B->GetClass()->GetPathName();
   if(ClassPath.Contains(TEXT("KDataForge")) || ClassPath.Contains(TEXT("FILNRestStop")) || ClassPath.Contains(TEXT("ProgrammableTruckStation")))
    Report.Add(Station->GetPathName()+TEXT(": unsupported legacy depot/actor; routes, enrollment and class conversion require a separate migration adapter; untouched"));
   continue;
  }
  const auto* ObjectProperty=CastField<FObjectPropertyBase>(Property->Inner);
  FScriptArrayHelper Array(Property,Property->ContainerPtrToValuePtr<void>(B));
  TSubclassOf<UFGItemDescriptor> Selected;
  if(ObjectProperty && Array.Num()==1) Selected=Cast<UClass>(ObjectProperty->GetObjectPropertyValue(Array.GetRawPtr(0)));
  if(!Selected || !Selected->IsChildOf(UFGItemDescriptor::StaticClass())) {
   Report.Add(Station->GetPathName()+TEXT(": unsupported/ambiguous legacy filter; untouched")); continue;
  }
  const auto* Existing=StationConfigs.FindByPredicate([Station](const FFILNStationConfig& C){return C.Station==Station;});
  if(Existing) {
   Report.Add(Station->GetPathName()+(Existing->Resource==Selected?TEXT(": already imported"):TEXT(": conflicting FILN assignment; untouched"))); continue;
  }
  auto* Inventory=B->GetInventory(); bool Valid=Inventory && !B->GetDockedActor() && !HomeStations.Contains(TWeakObjectPtr<AFGDockingStationIdentifier>(Station)) &&
   !Reservations.StationBusy(TCHAR_TO_UTF8(*StationId(Station).ToString()));
  if(Inventory) {
   TArray<FInventoryStack> Stacks; Inventory->GetInventoryStacks(Stacks);
   for(const auto& S:Stacks) if(S.NumItems>0 && S.Item.GetItemClass()!=Selected) Valid=false;
   for(int32 Slot=0;Slot<Inventory->GetSizeLinear();++Slot) if(Inventory->GetAllowedItemOnIndex(Slot) && Inventory->GetAllowedItemOnIndex(Slot)!=Selected) Valid=false;
  }
  if(!Valid) { Report.Add(Station->GetPathName()+TEXT(": inventory/reservation/docking conflict; untouched")); continue; }
  Candidates.Add({Station,Selected});
  Report.Add(Station->GetPathName()+TEXT(": settings import available; legacy actor class still requires its original mod until separately converted"));
 }
 if(Apply) {
  const int32 OriginalCount=StationConfigs.Num(); bool Success=true;
  for(const auto& Candidate:Candidates) if(!ConfigureStation(Candidate.Station,Candidate.Resource,true)) { Success=false; break; }
  if(!Success) {
   // Only new metadata/restrictions are changed. No actors, contents, routes or connections
   // are replaced. Rollback stops rather than overwrite an intervening third-party filter.
   while(StationConfigs.Num()>OriginalCount) {
    auto* Station=StationConfigs.Last().Station.Get();
    if(!RemoveStation(Station)) { bDispatchBlocked=true; Report.Add(TEXT("ROLLBACK BLOCKED: retained metadata for recovery; dispatch locked")); break; }
   }
   Report.Add(TEXT("Batch failed; no legacy objects removed"));
  } else { LegacyImportVersion=1; Report.Add(TEXT("Version 1 settings batch committed; original actor classes and all infrastructure preserved")); }
 }
 MigrationReport=Report;
 for(const auto& Line:Report) UE_LOG(LogFILN,Log,TEXT("LEGACY %s"),*Line);
}
