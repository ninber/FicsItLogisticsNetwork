// SPDX-License-Identifier: GPL-3.0-only
#include "LogisticsSubsystem.h"
#include "FILNStationControls.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "WheeledVehicles/FGWheeledVehicleIdentifier.h"
#include "FGInventoryComponent.h"
#include "UObject/UnrealType.h"
#include "Resources/FGItemDescriptor.h"
#include "FGRecipe.h"
#include "FILNLegacy.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "WheeledVehicles/FGVehiclePathNode.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

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
 if(!HasAuthority() || bDispatchBlocked || LegacyImportVersion>2) return;
 auto* VS=AFGVehicleSubsystem::Get(GetWorld()); if(!VS) return;
 struct FCandidate { AFGDockingStationIdentifier* Station; TSubclassOf<UFGItemDescriptor> Resource; };
 TArray<FCandidate> Candidates; TArray<FString> Report;
 for(auto* Station:VS->GetDockingStations()) {
  auto* B=IsValid(Station)?Station->GetStation():nullptr; if(!IsValid(B)) continue;
  const auto Recipe=B->GetBuiltWithRecipe();
  if(Recipe && Recipe->GetPathName()==TEXT("/KDataForge/Gen/FILNRestStop.Gen_FILNRestStop_Recipe_RestStop")) {
   int32 Claims=0; AFGWheeledVehicleIdentifier* Match=nullptr;
   const auto Node=Station->GetPathNodeGUID();
   for(auto* V:VS->GetAllVehicles()) if(IsValid(V) && V->GetVehicleRoute().Contains(Node)) { ++Claims; Match=V; }
   if(Claims==1 && AdoptLegacyHome(Match,Station,false)) {
    const bool Ok=!Apply || AdoptLegacyHome(Match,Station,true);
    Report.Add(Station->GetPathName()+(Ok?TEXT(": legacy idle depot ownership compatible; original recipe dependency retained"):TEXT(": depot adoption rejected")));
   } else Report.Add(Station->GetPathName()+TEXT(": legacy depot not idle/unambiguous; journal reconciliation required"));
   continue;
  }
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
  } else { LegacyImportVersion=2; Report.Add(TEXT("Version 2 settings batch committed; compatible idle depots adopted separately; original actor classes and infrastructure preserved")); }
 }
 MigrationReport=Report;
 for(const auto& Line:Report) UE_LOG(LogFILN,Log,TEXT("LEGACY %s"),*Line);
}

bool ALogisticsSubsystem::AdoptLegacyHome(AFGWheeledVehicleIdentifier* V,AFGDockingStationIdentifier* H,bool Apply) {
 check(IsInGameThread());
 if(!HasAuthority() || bDispatchBlocked || !IsValid(V) || !IsValid(H) || !ValidHome(H)) return false;
 auto* B=H->GetStation(); auto* O=V->GetOwnerVehicle();
 if(!IsValid(O) || !O->GetStorageInventory() || !B->GetDockingPathNode()) return false;
 const auto Recipe=B->GetBuiltWithRecipe();
 const bool Exact=Recipe && Recipe->GetPathName()==TEXT("/KDataForge/Gen/FILNRestStop.Gen_FILNRestStop_Recipe_RestStop");
 const TArray<FGuid> HomeRoute={H->GetPathNodeGUID()};
 if(!filn::legacy::IdleAdoption(Exact,true,true,true,V->IsAutopilotEnabled(),O->GetDriver()!=nullptr,
  O->GetStorageInventory()->IsEmpty(),FMath::Abs(O->GetForwardSpeed())<=1.f,
  FVector::DistSquared(O->GetVehicleLocation(),B->GetDockingPathNode()->GetActorLocation())<=FMath::Square(800.f),
  V->GetVehicleRoute()==HomeRoute)) return false;
 if(const auto* I=ByVehicle.Find(V)) return Registrations.IsValidIndex(*I) && Registrations[*I].Home==H && Registrations[*I].State==static_cast<uint8>(filn::State::Idle);
 if(!Apply) return !HomeStations.Contains(TWeakObjectPtr<AFGDockingStationIdentifier>(H));
 return Pair(V,H); // adoption branch performs no native route/AP writes
}
void ALogisticsSubsystem::ImportLegacyJournal(const FString& Filename) {
 check(IsInGameThread());
 if(!HasAuthority() || bDispatchBlocked || bAutoDispatchEnabled) return;
 // Explicit server-console preview only. File is supplied by operator in Saved/FILN.
 // Never read the running Lua mod's journal or assume it belongs to this save.
 if(Filename!=TEXT("dispatch-journal.tsv")) return;
 const FString Path=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("FILN"),Filename);
 const int64 Size=IFileManager::Get().FileSize(*Path);
 FString Input;
 std::vector<filn::legacy::Entry> Entries;
 if(Size<0 || Size>65536 || !FFileHelper::LoadFileToString(Input,*Path) || !filn::legacy::Parse(TCHAR_TO_UTF8(*Input),Entries)) {
  UE_LOG(LogFILN,Error,TEXT("Legacy journal missing/invalid; no state imported")); return;
 }
 auto* VS=AFGVehicleSubsystem::Get(GetWorld()); if(!VS) return;
 MigrationReport.Empty();
 for(const auto& E:Entries) {
  int32 Matches=0; AFGWheeledVehicleIdentifier* Candidate=nullptr;
  auto Kind=filn::legacy::RouteMatch::None;
  for(auto* V:VS->GetAllVehicles()) if(IsValid(V)) {
   std::vector<std::string> Route;
   for(const auto& G:V->GetVehicleRoute()) Route.emplace_back(TCHAR_TO_UTF8(*G.ToString(EGuidFormats::Digits).ToLower()));
   auto M=filn::legacy::Match(E,Route);
   if(M!=filn::legacy::RouteMatch::None) { ++Matches; Candidate=V; Kind=M; }
  }
  const FString Home=UTF8_TO_TCHAR(E.home.c_str());
  if(Matches!=1) { MigrationReport.Add(Home+TEXT(": missing/ambiguous journal vehicle; untouched")); continue; }
  MigrationReport.Add(Home+FString::Printf(TEXT(": matched %s route kind=%d; active transfer counters cannot be reconstructed from journal alone; no route or cargo changed"),*Candidate->GetPathName(),static_cast<int32>(Kind)));
 }
 for(const auto& Line:MigrationReport) UE_LOG(LogFILN,Log,TEXT("JOURNAL PREVIEW %s"),*Line);
}
