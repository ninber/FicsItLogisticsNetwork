// SPDX-License-Identifier: GPL-3.0-only
#include "LogisticsSubsystem.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "WheeledVehicles/FGVehicleSubsystem.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "WheeledVehicles/FGWheeledVehicleIdentifier.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "FGInventoryComponent.h"
#include "Resources/FGItemDescriptor.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "Engine/World.h"

namespace {
using Object=TSharedPtr<FJsonObject>;
using Value=TSharedPtr<FJsonValue>;
Object Obj() { return MakeShared<FJsonObject>(); }
Value Val(const Object& O) { return MakeShared<FJsonValueObject>(O); }
FString Guid(AFGDockingStationIdentifier* S) { return IsValid(S)?S->GetPathNodeGUID().ToString():FString(); }
FString Descriptor(TSubclassOf<UFGItemDescriptor> D) { return D?D->GetPathName():FString(); }
int64 Stamp() { return FDateTime::UtcNow().ToUnixTimestamp(); }
Object Position(const FVector& P) {
 auto O=Obj(); O->SetNumberField(TEXT("x"),P.X/100.); O->SetNumberField(TEXT("y"),P.Y/100.); O->SetNumberField(TEXT("z"),P.Z/100.); return O;
}
struct Stock {
 Object Json=Obj(); TSubclassOf<UFGItemDescriptor> Item; int32 Kinds=0; int64 Amount=0, Capacity=0, Total=0; bool Valid=false;
 TArray<Value> Contents;
};
Stock InspectStock(UFGInventoryComponent* I,TSubclassOf<UFGItemDescriptor> Requested=nullptr) {
 Stock S; S.Item=Requested;
 const int32 Slots=IsValid(I)?I->GetSizeLinear():0;
 TMap<UClass*,int64> Counts;
 if(IsValid(I) && Slots>0 && Slots<=4096) {
  S.Valid=true;
  TArray<FInventoryStack> Stacks; I->GetInventoryStacks(Stacks);
  for(const auto& Entry:Stacks) {
   if(Entry.NumItems<0 || (Entry.NumItems>0 && !Entry.Item.GetItemClass())) { S.Valid=false; continue; }
   if(Entry.NumItems>0) { Counts.FindOrAdd(Entry.Item.GetItemClass().Get())+=Entry.NumItems; S.Total+=Entry.NumItems; }
  }
  S.Kinds=Counts.Num();
  if(!S.Item && S.Kinds==1) for(const auto& Entry:Counts) S.Item=Entry.Key;
  for(const auto& Entry:Counts) {
   auto O=Obj(); O->SetStringField(TEXT("item"),Entry.Key->GetPathName()); O->SetNumberField(TEXT("amount"),static_cast<double>(Entry.Value)); S.Contents.Add(Val(O));
  }
  S.Amount=S.Item?Counts.FindRef(S.Item.Get()):S.Total;
  if(S.Item) for(int32 Slot=0;Slot<Slots;++Slot) {
   const auto Filter=I->GetAllowedItemOnIndex(Slot);
   if(!Filter || Filter==S.Item) {
    const auto Capacity=I->GetSlotSizeForItem(Slot,S.Item);
    if(Capacity<0) S.Valid=false; else S.Capacity+=Capacity;
   }
  }
 }
 S.Json->SetArrayField(TEXT("items"),S.Contents);
 S.Json->SetNumberField(TEXT("amount"),static_cast<double>(S.Total));
 S.Json->SetNumberField(TEXT("capacity"),static_cast<double>(S.Capacity));
 S.Json->SetNumberField(TEXT("slots"),Slots);
 S.Json->SetNumberField(TEXT("kinds"),S.Kinds);
 S.Json->SetStringField(TEXT("item"),Descriptor(S.Item));
 return S;
}
const TCHAR* Phase(const FFILNRegistration& R) {
 if(R.Relocating) return TEXT("Relocating");
 switch(static_cast<filn::State>(R.State)) {
 case filn::State::Idle:return TEXT("Idle");
 case filn::State::Reserved:return TEXT("Reserved");
 case filn::State::ToProvider:return TEXT("Going to provider");
 case filn::State::Loading:return TEXT("Loading");
 case filn::State::ToRequester:return TEXT("Going to requester");
 case filn::State::Unloading:return TEXT("Unloading");
 case filn::State::Returning:return TEXT("Returning to depot");
 case filn::State::Completed:return TEXT("Completed");
 default:return TEXT("Recovery");
 }
}
FString TelemetryDirectory() { return FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("FILN")); }
}
void ALogisticsSubsystem::ResetTelemetry() {
 bTelemetryCollecting=false; bTelemetryDemand=false;
 NextTelemetryDemandCheck=0; NextTelemetryStart=0;
 TelemetryStationQueue.Reset(); TelemetryVehicleQueue.Reset();
 TelemetryStations.Reset(); TelemetryVehicles.Reset(); TelemetryErrors.Reset();
 TelemetryWorld.Reset();
}
void ALogisticsSubsystem::SampleTelemetry() {
 // Demand-driven, no actor changes, no engine browser calls, no shell/process
 // launch. Only authoritative game-thread samples; writer sees plain JSON.
 if(!HasAuthority() || !GetWorld() || GetWorld()->bIsTearingDown) return;
 const double Now=GetWorld()->GetTimeSeconds();
 if(Now>=NextTelemetryDemandCheck) {
  NextTelemetryDemandCheck=Now+5.;
  const auto Modified=IFileManager::Get().GetTimeStamp(*FPaths::Combine(TelemetryDirectory(),TEXT("dashboard-demand.txt")));
  const double Age=(FDateTime::UtcNow()-Modified).GetTotalSeconds();
  bTelemetryDemand=Age>=-5. && Age<=30.;
 }
 if(!bTelemetryDemand) {
  if(bTelemetryCollecting) { ResetTelemetry(); NextTelemetryDemandCheck=Now+5.; }
  return;
 }
 if(!bTelemetryCollecting) {
  if(Now<NextTelemetryStart || (TelemetryWrite.IsValid() && !TelemetryWrite.IsReady())) return;
  if(TelemetryWrite.IsValid() && !TelemetryWrite.Get()) UE_LOG(LogFILN,Warning,TEXT("FILN telemetry write failed"));
  auto* VS=AFGVehicleSubsystem::Get(GetWorld()); if(!VS) return;
  TelemetryStationQueue.Reset(); TelemetryVehicleQueue.Reset();
  TelemetryStations.Reset(); TelemetryVehicles.Reset(); TelemetryErrors.Reset();
  for(auto* S:VS->GetDockingStations()) if(IsValid(S)) TelemetryStationQueue.Add(S);
  for(auto* V:VS->GetAllVehicles()) if(IsValid(V)) TelemetryVehicleQueue.Add(V);
  TelemetryStationCursor=TelemetryVehicleCursor=0; TelemetryStarted=Stamp();
  if(TelemetryWorld.IsEmpty()) TelemetryWorld=GetWorld()->GetPathName()+TEXT(":")+FGuid::NewGuid().ToString();
  bTelemetryCollecting=true;
 }
 const auto Error=[this](const FString& Id,const FString& Why) { auto E=Obj(); E->SetStringField(TEXT("id"),Id); E->SetStringField(TEXT("error"),Why); TelemetryErrors.Add(Val(E)); };
 // At most two objects, subject to a 1 ms work budget; no per-tick fleet scan.
 const double Deadline=FPlatformTime::Seconds()+0.001;
 for(int32 Work=0;Work<2 && FPlatformTime::Seconds()<Deadline;++Work) {
  if(TelemetryStationCursor<TelemetryStationQueue.Num()) {
   auto* S=TelemetryStationQueue[TelemetryStationCursor++].Get();
   auto* B=IsValid(S)?S->GetStation():nullptr;
   if(!IsValid(B) || !S->GetPathNodeGUID().IsValid()) { Error(TEXT("station"),TEXT("Native station identity unavailable")); continue; }
   const bool Depot=IsDepot(S), Load=B->GetIsInLoadMode();
   const auto* Config=StationConfigs.FindByPredicate([S](const FFILNStationConfig& C){return C.Station==S;});
   TSubclassOf<UFGItemDescriptor> Selected;
   if(Config && StationResourceMatches(*Config)) Selected=Config->Resource;
   const auto Stock=InspectStock(B->GetInventory(),Selected);
   const bool Solid=SupportsSolidCargo(B), Explicit=Selected && Solid;
   const bool Supply=Load && Stock.Valid && Stock.Kinds==1 && Stock.Total>0 && Stock.Item && UFGItemDescriptor::GetForm(Stock.Item)==EResourceForm::RF_SOLID;
   const bool Configured=!Depot && Solid && Stock.Valid && (Explicit || Supply) && Stock.Kinds<=1 && (!Selected || Stock.Total==Stock.Amount);
   if(!Stock.Valid) Error(Guid(S),TEXT("Station inventory not readable"));
   if(Explicit && Stock.Total!=Stock.Amount) Error(Guid(S),TEXT("Station cargo conflicts with selected resource"));
   auto O=Obj(); O->SetStringField(TEXT("id"),Guid(S)); O->SetStringField(TEXT("guid"),Guid(S)); O->SetStringField(TEXT("name"),S->GetStationName().ToString());
   O->SetStringField(TEXT("role"),Load?TEXT("provider"):TEXT("requester")); O->SetBoolField(TEXT("configured"),Configured); O->SetBoolField(TEXT("is_depot"),Depot);
   O->SetStringField(TEXT("item"),Configured?Descriptor(Stock.Item):FString()); O->SetNumberField(TEXT("amount"),static_cast<double>(Stock.Total));
   O->SetNumberField(TEXT("capacity"),Configured?static_cast<double>(Stock.Capacity):0.); O->SetArrayField(TEXT("contents"),Stock.Contents);
   O->SetBoolField(TEXT("powered"),B->HasPower()); O->SetBoolField(TEXT("standby"),B->IsProductionPaused()); O->SetBoolField(TEXT("occupied"),B->GetDockedActor()!=nullptr);
   O->SetObjectField(TEXT("position"),Position(B->GetActorLocation())); O->SetObjectField(TEXT("fuel"),InspectStock(B->GetFuelInventory()).Json);
   O->SetNumberField(TEXT("sampled_at"),static_cast<double>(Stamp())); TelemetryStations.Add(Val(O)); continue;
  }
  if(TelemetryVehicleCursor<TelemetryVehicleQueue.Num()) {
   auto* V=TelemetryVehicleQueue[TelemetryVehicleCursor++].Get();
   auto* Owner=IsValid(V)?V->GetOwnerVehicle():nullptr;
   if(!IsValid(Owner)) { Error(TEXT("vehicle"),TEXT("Vehicle owner unavailable")); continue; }
   const auto* Index=ByVehicle.Find(V); const auto* R=Index && Registrations.IsValidIndex(*Index)?&Registrations[*Index]:nullptr;
   auto* H=R && IsValid(R->Home)?R->Home->GetStation():nullptr;
   const auto Cargo=InspectStock(Owner->GetStorageInventory(),R && R->JobId.IsValid()?R->Resource:TSubclassOf<UFGItemDescriptor>());
   auto Fuel=InspectStock(Owner->GetFuelInventory());
   if(!Fuel.Item && IsValid(H)) Fuel=InspectStock(Owner->GetFuelInventory(),InspectStock(H->GetFuelInventory()).Item);
   if(!Cargo.Valid || !Fuel.Valid) Error(V->GetPathName(),TEXT("Vehicle cargo or fuel inventory not readable"));
   auto O=Obj(); const FString Id=R?R->TruckId.ToString():V->GetPathName();
   O->SetStringField(TEXT("id"),Id); O->SetStringField(TEXT("key"),V->GetPathName()); O->SetStringField(TEXT("name"),V->GetVehicleName().ToString());
   O->SetStringField(TEXT("class"),V->GetVehicleClass()?V->GetVehicleClass()->GetPathName():FString());
   O->SetBoolField(TEXT("managed"),R!=nullptr); O->SetBoolField(TEXT("home_powered"),IsValid(H) && H->HasPower() && !H->IsProductionPaused());
   O->SetBoolField(TEXT("autopilot"),V->IsAutopilotEnabled()); O->SetBoolField(TEXT("driver"),Owner->GetDriver()!=nullptr);
   O->SetBoolField(TEXT("has_fuel"),Owner->HasFuel()); O->SetBoolField(TEXT("docking"),V->IsCurrentlyDocking());
   O->SetBoolField(TEXT("held"),R && R->Held); O->SetBoolField(TEXT("solid_supported"),SupportsSolidCargo(Owner));
   O->SetNumberField(TEXT("native_error"),static_cast<int32>(V->GetAutopilotErrorStatus())); O->SetNumberField(TEXT("target"),V->GetCurrentTargetWaypointIndex());
   O->SetObjectField(TEXT("cargo"),Cargo.Json); O->SetObjectField(TEXT("fuel"),Fuel.Json);
   FVector Location; FRotator Rotation; Owner->GetVehicleLocationAndRotation(Location,Rotation);
   O->SetObjectField(TEXT("position"),Position(Location));
   TArray<Value> Route; for(const auto& Node:V->GetVehicleRoute()) Route.Add(MakeShared<FJsonValueString>(Node.ToString()));
   O->SetArrayField(TEXT("route"),Route); O->SetNumberField(TEXT("sampled_at"),static_cast<double>(Stamp()));
   if(R) {
    O->SetStringField(TEXT("home"),Guid(R->Home));
    auto J=Obj(); J->SetStringField(TEXT("kind"),R->RetainedCargo?TEXT("cargo"):TEXT("pickup")); J->SetStringField(TEXT("state"),Phase(*R));
    J->SetStringField(TEXT("provider"),Guid(R->Provider)); J->SetStringField(TEXT("requester"),Guid(R->Requester)); J->SetStringField(TEXT("item"),Descriptor(R->Resource));
    J->SetNumberField(TEXT("loaded"),R->LoadedCargo); J->SetNumberField(TEXT("unloaded"),R->DeliveredCargo); J->SetNumberField(TEXT("truckload"),R->ExpectedCargo); J->SetStringField(TEXT("error"),R->Diagnostic);
    O->SetObjectField(TEXT("job"),J);
   } else if(const auto* Why=PendingHomeReason.Find(TWeakObjectPtr<AFGWheeledVehicleIdentifier>(V))) O->SetStringField(TEXT("native_pending_reason"),*Why);
   TelemetryVehicles.Add(Val(O)); continue;
  }
  // Keep previous complete snapshot until the next one has been written.
  auto Snapshot=Obj(); auto Dispatcher=Obj(); auto Leases=Obj(); auto Stats=Obj();
  for(const auto& R:Registrations) if(R.JobId.IsValid()) {
   if(IsValid(R.Provider)) Leases->SetStringField(Guid(R.Provider),Guid(R.Home));
   if(IsValid(R.Requester)) Leases->SetStringField(Guid(R.Requester),Guid(R.Home));
  }
  Dispatcher->SetBoolField(TEXT("registration_supported"),false);
  Dispatcher->SetBoolField(TEXT("enabled"),bAutoDispatchEnabled && bRegistryValidated && !bDispatchBlocked && !bPreparingUninstall && !bUninstallPrepared);
  Dispatcher->SetBoolField(TEXT("cargo_reuse_enabled"),true); Dispatcher->SetBoolField(TEXT("storage_tag_rule_enabled"),true);
  Dispatcher->SetBoolField(TEXT("registry_validated"),bRegistryValidated); Dispatcher->SetBoolField(TEXT("depot_recipe_ready"),bDepotRecipeReady);
  Dispatcher->SetObjectField(TEXT("reservations"),Leases);
  Stats->SetNumberField(TEXT("graph_hits"),static_cast<double>(RoadCache.Hits)); Stats->SetNumberField(TEXT("graph_builds"),static_cast<double>(RoadCache.Misses)); Stats->SetNumberField(TEXT("rank_invalidations"),static_cast<double>(RoadCache.Invalidations)); Stats->SetNumberField(TEXT("graph_work"),static_cast<double>(RoadCache.Work)); Dispatcher->SetObjectField(TEXT("stats"),Stats);
  Snapshot->SetNumberField(TEXT("schema_version"),1); Snapshot->SetStringField(TEXT("runtime"),TEXT("native")); Snapshot->SetStringField(TEXT("world"),TelemetryWorld);
  Snapshot->SetNumberField(TEXT("sequence"),static_cast<double>(++TelemetrySequence)); Snapshot->SetNumberField(TEXT("exported_at"),static_cast<double>(Stamp()));
  Snapshot->SetNumberField(TEXT("collection_seconds"),static_cast<double>(Stamp()-TelemetryStarted)); Snapshot->SetNumberField(TEXT("collection_started_at"),static_cast<double>(TelemetryStarted));
  Snapshot->SetArrayField(TEXT("stations"),TelemetryStations); Snapshot->SetArrayField(TEXT("vehicles"),TelemetryVehicles); Snapshot->SetArrayField(TEXT("errors"),TelemetryErrors);
  // No fabricated truckload estimates. Per-resource vehicle capacity sampling
  // remains a separate acceptance task; dashboard shows unknown where absent.
  Snapshot->SetArrayField(TEXT("capacity_samples"),TArray<Value>());
  Snapshot->SetObjectField(TEXT("dispatcher"),Dispatcher);
  const FString Directory=TelemetryDirectory();
  const FString Temp=FPaths::Combine(Directory,FGuid::NewGuid().ToString()+TEXT(".tmp"));
  const FString Destination=FPaths::Combine(Directory,TEXT("telemetry.json"));
  TelemetryWrite=Async(EAsyncExecution::ThreadPool,[Snapshot,Directory,Temp,Destination]() {
   FString Json; auto Writer=TJsonWriterFactory<>::Create(&Json);
   if(!FJsonSerializer::Serialize(Snapshot.ToSharedRef(),Writer)) return false;
   IFileManager::Get().MakeDirectory(*Directory,true);
   if(!FFileHelper::SaveStringToFile(Json,*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) return false;
   const bool Moved=IFileManager::Get().Move(*Destination,*Temp,true,true);
   if(!Moved) IFileManager::Get().Delete(*Temp,false,true);
   return Moved;
  });
  TelemetryStations.Reset(); TelemetryVehicles.Reset(); TelemetryErrors.Reset();
  bTelemetryCollecting=false; NextTelemetryStart=Now+5.; break;
 }
}
