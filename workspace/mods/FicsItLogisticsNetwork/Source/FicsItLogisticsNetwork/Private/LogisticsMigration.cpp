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
 if(!RestoreStationFilters(TArray<int32>{Index})) return false;
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
void ALogisticsSubsystem::ImportLegacyJournal(const FString& Filename,bool Apply) {
 check(IsInGameThread());
 if(!HasAuthority() || bDispatchBlocked || bAutoDispatchEnabled) return;
 // Explicit server-console preview/import. File is supplied by operator in Saved/FILN.
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
 if(Reservations.Size()!=0 || !Active.IsEmpty()) { UE_LOG(LogFILN,Error,TEXT("Finish existing native jobs before legacy journal import")); return; }
 // Resolve only unique actual node identifiers. Never guess a destination by name.
 auto Resolve=[VS](const std::string& Key)->AFGDockingStationIdentifier* {
  AFGDockingStationIdentifier* Found=nullptr;
  for(auto* ID:VS->GetDockingStations()) if(IsValid(ID) && Key==TCHAR_TO_UTF8(*ID->GetPathNodeGUID().ToString(EGuidFormats::Digits).ToLower())) {
   if(Found) return nullptr;
   Found=ID;
  }
  return Found;
 };
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
  auto* H=Resolve(E.home); auto* Q=Resolve(E.requester);
  auto* HB=IsValid(H)?H->GetStation():nullptr; auto* QB=IsValid(Q)?Q->GetStation():nullptr;
  auto* O=Candidate->GetOwnerVehicle(); auto* Cargo=IsValid(O)?O->GetStorageInventory():nullptr;
  const auto* Config=StationConfigs.FindByPredicate([Q](const FFILNStationConfig& C){return C.Station==Q;});
  const auto Recipe=IsValid(HB)?HB->GetBuiltWithRecipe():TSubclassOf<UFGRecipe>();
  if(!IsValid(HB) || !IsValid(QB) || !Recipe || Recipe->GetPathName()!=TEXT("/KDataForge/Gen/FILNRestStop.Gen_FILNRestStop_Recipe_RestStop") ||
     !ValidHome(H) || QB->GetIsInLoadMode() || !Config || !Config->Resource || !Cargo || !Candidate->IsAutopilotEnabled() || O->GetDriver()) {
   MigrationReport.Add(Home+TEXT(": invalid endpoint, recipe, filter, owner or player opt-in; untouched")); continue;
  }
  if(Kind==filn::legacy::RouteMatch::Idle) {
   const bool Ok=AdoptLegacyHome(Candidate,H,Apply);
   MigrationReport.Add(Home+(Ok?TEXT(": idle adoption compatible"):TEXT(": idle adoption rejected"))); continue;
  }
  if(ByVehicle.Contains(Candidate) || HomeStations.Contains(TWeakObjectPtr<AFGDockingStationIdentifier>(H))) {
   MigrationReport.Add(Home+TEXT(": already registered or home claimed; untouched")); continue;
  }
  bool Exclusive=true;
  for(auto* Other:VS->GetAllVehicles()) if(IsValid(Other) && Other!=Candidate && Other->GetVehicleRoute().Contains(H->GetPathNodeGUID())) Exclusive=false;
  TArray<FInventoryStack> Stacks; Cargo->GetInventoryStacks(Stacks);
  bool Mixed=false;
  for(const auto& Stack:Stacks) if(Stack.NumItems>0 && Stack.Item.GetItemClass()!=Config->Resource) Mixed=true;
  const int32 Quantity=Cargo->GetNumItems(Config->Resource);
  const int32 TargetIndex=Candidate->GetCurrentTargetWaypointIndex();
  const auto& Current=Candidate->GetVehicleRoute();
  const bool TargetValid=Current.IsValidIndex(TargetIndex);
  const bool AtRequester=TargetValid && Current[TargetIndex]==Q->GetPathNodeGUID();
  const bool AtHome=TargetValid && Current[TargetIndex]==H->GetPathNodeGUID();
  // Never resume a route whose next waypoint could pick up again. Legacy cargo
  // origin is unknown and an imported shipment must not be redirected.
  const auto Decision=filn::legacy::Restore(Exclusive,Mixed,Quantity,AtRequester,AtHome,Kind==filn::legacy::RouteMatch::Delivery,
    filn::dispatch::IsStorage(TCHAR_TO_UTF8(*Q->GetStationName().ToString())));
  if(Decision==filn::legacy::RestoreAction::Reject || Candidate->IsCurrentlyDocking() || QB->GetDockedActor()==O || HB->GetDockedActor()==O) {
   MigrationReport.Add(Home+TEXT(": active route/cargo cannot safely resume without possible repeat pickup; untouched")); continue;
  }
  if(!Apply) { MigrationReport.Add(Home+TEXT(": compatible active delivery/return adoption available")); continue; }
  FFILNRegistration R;
  R.TruckId=FGuid::NewGuid(); R.BayId=StationId(H); R.JobId=FGuid::NewGuid();
  R.Vehicle=Candidate; R.Home=H; R.Requester=Q; R.Resource=Config->Resource; R.RequesterLease=StationId(Q);
  R.Automatic=true; R.RetainedCargo=true; R.LegacyDelivery=true; R.RouteIntent=Current;
  R.ExpectedCargo=FMath::Max(1,Quantity); R.LastCargo=Quantity; R.LastObserved=GetWorld()->GetTimeSeconds();
  R.State=static_cast<uint8>(Decision==filn::legacy::RestoreAction::Deliver?filn::State::ToRequester:filn::State::Returning);
  // Existing route must contain no provider; returning empty is not delivery success.
  if(!Reservations.AcquireDelivery(TCHAR_TO_UTF8(*R.JobId.ToString()),TCHAR_TO_UTF8(*R.TruckId.ToString()),TCHAR_TO_UTF8(*StationId(Q).ToString()))) {
   MigrationReport.Add(Home+TEXT(": reservation conflict; untouched")); continue;
  }
  if(!Ownership.Pair(TCHAR_TO_UTF8(*R.TruckId.ToString()),TCHAR_TO_UTF8(*R.BayId.ToString()))) {
   Reservations.Release(TCHAR_TO_UTF8(*R.JobId.ToString())); MigrationReport.Add(Home+TEXT(": ownership conflict; untouched")); continue;
  }
  R.Diagnostic=TEXT("Legacy route adopted; only future native transfer events counted");
  HomeStations.Add(H); const int32 Index=Registrations.Add(R); ByVehicle.Add(Candidate,Index); WatchTransfers(Registrations[Index]); Active.AddUnique(Index);
  ResetScan(); MigrationReport.Add(Home+TEXT(": active legacy delivery/return adopted without native route or cargo write"));
 }
 for(const auto& Line:MigrationReport) UE_LOG(LogFILN,Log,TEXT("JOURNAL %s"),*Line);
}
