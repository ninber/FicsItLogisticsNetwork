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
#include "WheeledVehicles/FGVehicleSubsystem.h"
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
 if(!HasAuthority() || bDispatchBlocked || bAutoDispatchEnabled || Reservations.Size()!=0 || LegacyImportVersion>2) return;
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
   if(Apply && ValidHome(Station) && !HomeStations.Contains(TWeakObjectPtr<AFGDockingStationIdentifier>(Station))) {
    if(SetDepot(Station,false)) Report.Add(Station->GetPathName()+TEXT(": persistent rest-stop role imported"));
   }
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
  const bool LegacyRecipe=Recipe && Recipe->GetPathName()==TEXT("/KDataForge/Gen/FILNRestStop.Gen_FILNRestStop_Recipe_RestStop");
  const bool ConvertedDepot=Recipe && Recipe->GetPathName()==TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_TruckStation.Recipe_TruckStation_C") &&
    DepotIndex.Contains(TWeakObjectPtr<AFGDockingStationIdentifier>(H));
  if(!IsValid(HB) || !IsValid(QB) || (!LegacyRecipe && !ConvertedDepot) ||
     !ValidHome(H) || QB->GetIsInLoadMode() || !Config || !Config->Resource || !Cargo || !Candidate->IsAutopilotEnabled() || O->GetDriver()) {
   MigrationReport.Add(Home+TEXT(": invalid endpoint, recipe, filter, owner or player opt-in; untouched")); continue;
  }
  if(Kind==filn::legacy::RouteMatch::Idle) {
   const bool Ok=LegacyRecipe?AdoptLegacyHome(Candidate,H,Apply):
     (Candidate->IsAutopilotEnabled() && !O->GetDriver() && Cargo->IsEmpty() &&
      FMath::Abs(O->GetForwardSpeed())<=1.f && HB->GetDockingPathNode() &&
      FVector::DistSquared(O->GetVehicleLocation(),HB->GetDockingPathNode()->GetActorLocation())<=FMath::Square(800.f) &&
      (!Apply || Pair(Candidate,H)));
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
  std::vector<std::string> RouteKeys;
  for(const auto& G:Current) RouteKeys.emplace_back(TCHAR_TO_UTF8(*G.ToString(EGuidFormats::Digits).ToLower()));
  // Pickup routes are safe only after the provider has been passed, with an
  // exact requester/home suffix. Arrival at home terminates the entire route.
  const bool SafeSuffix=filn::legacy::SafeRemainingRoute(E,RouteKeys,TargetIndex);
  auto Decision=filn::legacy::Restore(Exclusive,Mixed,Quantity,AtRequester,AtHome,SafeSuffix,
    filn::dispatch::IsStorage(TCHAR_TO_UTF8(*Q->GetStationName().ToString())));
  AFGDockingStationIdentifier* P=nullptr;
  int32 FullLoad=0;
  if(Decision==filn::legacy::RestoreAction::Reject && filn::legacy::PickupSuffix(E,RouteKeys,TargetIndex) && Exclusive && !Mixed && Quantity==0 && Cargo->IsEmpty()) {
   P=Resolve(E.provider); auto* PB=IsValid(P)?P->GetStation():nullptr;
   auto* Stock=IsValid(PB)?PB->GetInventory():nullptr;
   auto* Demand=QB->GetInventory();
   const auto* PC=StationConfigs.FindByPredicate([P](const FFILNStationConfig& C){return C.Station==P;});
   const auto Form=UFGItemDescriptor::GetForm(Config->Resource);
   const bool Fluid=Form==EResourceForm::RF_LIQUID || Form==EResourceForm::RF_GAS;
   bool Medium=Fluid || Form==EResourceForm::RF_SOLID;
   for(const UObject* Obj:{static_cast<const UObject*>(O),static_cast<const UObject*>(PB),static_cast<const UObject*>(QB)}) {
    const auto* Flag=Obj?FindFProperty<FBoolProperty>(Obj->GetClass(),TEXT("mIsFluidStorageInventory")):nullptr;
    if(!Flag || Flag->GetPropertyValue_InContainer(Obj)!=Fluid) Medium=false;
   }
   int64 Capacity=0;
   if(Cargo->GetSizeLinear()>0 && Cargo->GetSizeLinear()<=4096) {
    for(int32 Slot=0;Slot<Cargo->GetSizeLinear();++Slot) {
     const auto Allowed=Cargo->GetAllowedItemOnIndex(Slot);
     if(!Allowed || Allowed==Config->Resource) Capacity+=Cargo->GetSlotSizeForItem(Slot,Config->Resource);
    }
   }
   if(Capacity>0 && Capacity<=MAX_int32) FullLoad=static_cast<int32>(Capacity);
   bool StockMatches=Stock!=nullptr;
   if(Stock) {
    TArray<FInventoryStack> Contents; Stock->GetInventoryStacks(Contents);
    for(const auto& Stack:Contents) if(Stack.NumItems>0 && Stack.Item.GetItemClass()!=Config->Resource) StockMatches=false;
   }
   const bool ProviderOk=IsValid(PB) && !IsDepot(P) && P!=H && P!=Q && PB->GetIsInLoadMode() && PB->HasPower() &&
      !PB->IsProductionPaused() && !PB->GetDockedActor() && StockMatches && (!PC || PC->Resource==Config->Resource) &&
      !Reservations.StationBusy(TCHAR_TO_UTF8(*StationId(P).ToString()));
   const bool RequesterOk=!IsDepot(Q) && QB->HasPower() && !QB->IsProductionPaused() && !QB->GetDockedActor() && Demand &&
      Demand->IsEmpty() && FullLoad>0 && Demand->HasEnoughSpaceForStack(FInventoryStack(FullLoad,Config->Resource)) &&
      !Reservations.StationBusy(TCHAR_TO_UTF8(*StationId(Q).ToString()));
   const bool WarehouseAllowed=IsValid(P) && filn::dispatch::PairAllowed(TCHAR_TO_UTF8(*P->GetStationName().ToString()),TCHAR_TO_UTF8(*Q->GetStationName().ToString()));
   bool Roads=false;
   // This is an explicit migration operation, not a periodic scheduler scan.
   // Confirm all remaining directed legs with actual native path queries.
   if(ProviderOk && RequesterOk && Medium && WarehouseAllowed && O->HasFuel() && O->GetVehiclePathPreset()) {
    const FGuid Nodes[]={Candidate->GetCurrentToPathNodeGUID(),P->GetPathNodeGUID(),Q->GetPathNodeGUID(),H->GetPathNodeGUID()};
    Roads=true;
    for(int32 Leg=0;Leg<3;++Leg) {
     auto* Net=Nodes[Leg].IsValid()?VS->FindNetworkByPathNodeGuid(Nodes[Leg]):nullptr;
     TArray<FGuid> Route;
     if(!Net || !Nodes[Leg+1].IsValid() || Net!=VS->FindNetworkByPathNodeGuid(Nodes[Leg+1]) ||
        (Nodes[Leg]!=Nodes[Leg+1] && !Net->FindVehiclePath(Nodes[Leg],Nodes[Leg+1],O->GetVehiclePathPreset(),Route))) { Roads=false; break; }
    }
   }
   if(filn::legacy::CanResumePickup(Exclusive,Mixed,Quantity,true,ProviderOk,RequesterOk,Medium,WarehouseAllowed,Roads,FullLoad,
      Stock?Stock->GetNumItems(Config->Resource):0,RequesterOk?FullLoad:0)) Decision=filn::legacy::RestoreAction::Pickup;
  }
  if(Decision==filn::legacy::RestoreAction::Reject || Candidate->IsCurrentlyDocking() || QB->GetDockedActor()==O || HB->GetDockedActor()==O) {
   MigrationReport.Add(Home+TEXT(": active route/cargo cannot safely resume without possible repeat pickup; untouched")); continue;
  }
  const bool Pickup=Decision==filn::legacy::RestoreAction::Pickup;
  if(!Apply) { MigrationReport.Add(Home+(Pickup?TEXT(": compatible pre-pickup adoption available"):TEXT(": compatible active delivery/return adoption available"))); continue; }
  FFILNRegistration R;
  R.TruckId=FGuid::NewGuid(); R.BayId=StationId(H); R.JobId=FGuid::NewGuid();
  R.Vehicle=Candidate; R.Home=H; R.Requester=Q; R.Resource=Config->Resource; R.RequesterLease=StationId(Q);
  R.Automatic=true; R.RetainedCargo=!Pickup; R.LegacyDelivery=!Pickup; R.RouteIntent=Current;
  R.ExpectedCargo=Pickup?FullLoad:FMath::Max(1,Quantity); R.LastCargo=Quantity; R.LastObserved=GetWorld()->GetTimeSeconds();
  R.State=static_cast<uint8>(Pickup?filn::State::ToProvider:Decision==filn::legacy::RestoreAction::Deliver?filn::State::ToRequester:filn::State::Returning);
  // Reserve both endpoints for future pickup; historical transfers remain zero.
  if(Pickup) { R.Provider=P; R.ProviderLease=StationId(P); }
  const std::string Job=TCHAR_TO_UTF8(*R.JobId.ToString()),Truck=TCHAR_TO_UTF8(*R.TruckId.ToString()),Requester=TCHAR_TO_UTF8(*R.RequesterLease.ToString());
  const bool Acquired=Pickup?Reservations.Acquire(Job,Truck,TCHAR_TO_UTF8(*R.ProviderLease.ToString()),Requester):
    Reservations.AcquireDelivery(Job,Truck,Requester);
  if(!Acquired) {
   MigrationReport.Add(Home+TEXT(": reservation conflict; untouched")); continue;
  }
  if(!Ownership.Pair(TCHAR_TO_UTF8(*R.TruckId.ToString()),TCHAR_TO_UTF8(*R.BayId.ToString()))) {
   Reservations.Release(TCHAR_TO_UTF8(*R.JobId.ToString())); MigrationReport.Add(Home+TEXT(": ownership conflict; untouched")); continue;
  }
  R.Diagnostic=TEXT("Legacy route adopted; only future native transfer events counted");
  HomeStations.Add(H); TrackDepot(H); const int32 Index=Registrations.Add(R); ByVehicle.Add(Candidate,Index); WatchTransfers(Registrations[Index]); Active.AddUnique(Index);
  ResetScan(); MigrationReport.Add(Home+TEXT(": active legacy journey adopted without native route or cargo write"));
 }
 for(const auto& Line:MigrationReport) UE_LOG(LogFILN,Log,TEXT("JOURNAL %s"),*Line);
}

void ALogisticsSubsystem::ImportConvertedStations(bool Apply) {
 check(IsInGameThread());
 if(!HasAuthority() || bDispatchBlocked || bAutoDispatchEnabled || Reservations.Size()!=0 || !Active.IsEmpty()) return;
 const FString Path=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("FILN"),TEXT("converted-stations.tsv"));
 const int64 Size=IFileManager::Get().FileSize(*Path);
 FString Data;
 if(Size<0 || Size>65536 || !FFileHelper::LoadFileToString(Data,*Path)) return;
 TArray<FString> Lines; Data.ParseIntoArrayLines(Lines,false);
 if(Lines.IsEmpty() || Lines[0]!=TEXT("FILN_NATIVE_STATIONS_V1") || Lines.Num()>130) return;
 auto* VS=AFGVehicleSubsystem::Get(GetWorld()); if(!VS) return;
 struct FRow { AFGDockingStationIdentifier* Station; TSubclassOf<UFGItemDescriptor> Resource; bool Depot; };
 TArray<FRow> Rows; TSet<FGuid> Seen;
 for(int32 I=1;I<Lines.Num();++I) {
  if(I==Lines.Num()-1 && Lines[I].IsEmpty()) continue;
  TArray<FString> Fields; Lines[I].ParseIntoArray(Fields,TEXT("\t"),false);
  FGuid Node;
  if(Fields.Num()!=4 || !filn::legacy::Guid(TCHAR_TO_UTF8(*Fields[1])) ||
     !FGuid::ParseExact(Fields[1],EGuidFormats::Digits,Node) || Seen.Contains(Node) ||
     (Fields[3]!=TEXT("LOAD") && Fields[3]!=TEXT("UNLOAD"))) return;
  Seen.Add(Node);
  AFGDockingStationIdentifier* ID=nullptr;
  for(auto* Candidate:VS->GetDockingStations()) if(IsValid(Candidate) && Candidate->GetPathNodeGUID()==Node) {
   if(ID) return;
   ID=Candidate;
  }
  auto* B=IsValid(ID)?ID->GetStation():nullptr;
  if(!IsValid(B) || B->GetClass()->GetPathName()!=TEXT("/Game/FactoryGame/Buildable/Factory/TruckStation/Build_TruckStation.Build_TruckStation_C") ||
     B->GetIsInLoadMode()!=(Fields[3]==TEXT("LOAD"))) return;
  const auto Recipe=B->GetBuiltWithRecipe();
  if(!Recipe || Recipe->GetPathName()!=TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_TruckStation.Recipe_TruckStation_C")) return;
  if(Fields[0]==TEXT("depot") && Fields[2]==TEXT("-") && ValidHome(ID)) { Rows.Add({ID,nullptr,true}); continue; }
  if(Fields[0]!=TEXT("station") || !Fields[2].StartsWith(TEXT("/Game/FactoryGame/Resource/"))) return;
  const auto Resource=LoadClass<UFGItemDescriptor>(nullptr,*Fields[2]);
  if(!Resource) return;
  const auto* Existing=StationConfigs.FindByPredicate([ID](const FFILNStationConfig& C){return C.Station==ID;});
  if(Existing && Existing->Resource!=Resource) return;
  auto* Inventory=B->GetInventory();
  if(!Inventory || B->GetDockedActor() || IsDepot(ID)) return;
  TArray<FInventoryStack> Stacks; Inventory->GetInventoryStacks(Stacks);
  for(const auto& Stack:Stacks) if(Stack.NumItems>0 && Stack.Item.GetItemClass()!=Resource) return;
  for(int32 Slot=0;Slot<Inventory->GetSizeLinear();++Slot) {
   auto Filter=Inventory->GetAllowedItemOnIndex(Slot);
   if(Filter && Filter!=Resource) return;
  }
  Rows.Add({ID,Resource,false});
 }
 MigrationReport.Empty();
 for(const auto& Row:Rows) {
  if(Row.Depot) {
   // A converted depot no longer has legacy recipe identity. Only explicit
   // manifest rows authorize adoption, and Pair revalidates unique ownership.
   AFGWheeledVehicleIdentifier* V=nullptr; int32 Claims=0;
   for(auto* Candidate:VS->GetAllVehicles()) if(IsValid(Candidate) && Candidate->GetVehicleRoute().Contains(Row.Station->GetPathNodeGUID())) { V=Candidate; ++Claims; }
   const TArray<FGuid> Route={Row.Station->GetPathNodeGUID()};
   const bool Compatible=Claims==1 && IsValid(V) && V->IsAutopilotEnabled() && V->GetVehicleRoute()==Route;
   const bool Adopted=Compatible && (!Apply || Pair(V,Row.Station));
   const bool Designated=Adopted || !Apply || SetDepot(Row.Station,false);
   MigrationReport.Add(Row.Station->GetPathName()+(Adopted?TEXT(": compatible converted depot and idle vehicle"):Designated?TEXT(": depot role compatible; no safe idle vehicle adoption"):TEXT(": depot role rejected")));
  } else {
   const auto* Existing=StationConfigs.FindByPredicate([&Row](const FFILNStationConfig& C){return C.Station==Row.Station;});
   const bool Ok=Existing || !Apply || ConfigureStation(Row.Station,Row.Resource,true);
   MigrationReport.Add(Row.Station->GetPathName()+(Ok?TEXT(": converted station resource compatible"):TEXT(": resource import rejected")));
  }
  if(bDispatchBlocked) break;
 }
 for(const auto& Line:MigrationReport) UE_LOG(LogFILN,Log,TEXT("CONVERTED %s"),*Line);
}
