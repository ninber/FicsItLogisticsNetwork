#include "LogisticsSubsystem.h"
#include "FILNInventoryCapacity.h"
#include "FILNOrganizerPolicy.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "WheeledVehicles/FGVehicleSubsystem.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "WheeledVehicles/FGWheeledVehicleIdentifier.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "FGInventoryComponent.h"
#include "FGDockableInterface.h"
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
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "ImageUtils.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/SecureHash.h"
#include "UObject/StrongObjectPtr.h"

namespace {
using Object=TSharedPtr<FJsonObject>;
using Value=TSharedPtr<FJsonValue>;
Object Obj() { return MakeShared<FJsonObject>(); }
Value Val(const Object& O) { return MakeShared<FJsonValueObject>(O); }
FString Guid(AFGDockingStationIdentifier* S) { return IsValid(S)?S->GetPathNodeGUID().ToString():FString(); }
FString FILNTelemetryDescriptorPath(TSubclassOf<UFGItemDescriptor> D) { return D?D->GetPathName():FString(); }
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
  if(S.Item && !filn::inventory::TryCapacity(I,S.Item,S.Capacity)) S.Valid=false;
 }
 S.Json->SetArrayField(TEXT("items"),S.Contents);
 S.Json->SetNumberField(TEXT("amount"),static_cast<double>(S.Total));
 S.Json->SetNumberField(TEXT("capacity"),static_cast<double>(S.Capacity));
 S.Json->SetNumberField(TEXT("slots"),Slots);
 S.Json->SetNumberField(TEXT("kinds"),S.Kinds);
 S.Json->SetStringField(TEXT("item"),FILNTelemetryDescriptorPath(S.Item));
 return S;
}
const TCHAR* FILNTelemetryMedium(EResourceForm Form) {
 if(Form==EResourceForm::RF_SOLID) return TEXT("solid");
 if(Form==EResourceForm::RF_LIQUID || Form==EResourceForm::RF_GAS) return TEXT("fluid");
 return TEXT("unknown");
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
FString TelemetryDirectory() { return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("FILN"))); }

// Only local base-game artwork. Cache identity uses the exact UTF-8 descriptor
// path and a build/schema namespace; Python derives the same file name.
FString FILNIconHash(const FString& Text) {
 FTCHARToUTF8 Bytes(*Text); uint8 Digest[20];
 FSHA1::HashBuffer(Bytes.Get(),Bytes.Length(),Digest);
 return BytesToHex(Digest,20).ToLower();
}
FString FILNIconRevision() {
 static const FString Revision=FILNIconHash(FString(TEXT("filn-icon-v1-64|"))+FApp::GetBuildVersion()+TEXT("|")+FEngineVersion::Current().ToString());
 return Revision;
}
// Cold-cache work is deliberately limited to one attempted icon per complete
// dashboard scan. No synchronous work on cache hits, no rendering on servers.
// Pixel readback is currently synchronous: shipping frame-time acceptance is
// mandatory. The PNG encoder/writer never touches a UObject.
template<typename Resources>
void FILNCacheDashboardIcon(UWorld* World,const Resources& Items) {
 if(!World || World->GetNetMode()==NM_DedicatedServer || !FApp::CanEverRender() || Items.IsEmpty()) return;
 const FString Directory=FPaths::Combine(TelemetryDirectory(),TEXT("cache/icons"),FILNIconRevision());
 static int32 Cursor=0;
 static TFuture<void> Pending;
 if(Pending.IsValid() && !Pending.IsReady()) return;
 for(int32 Attempt=0;Attempt<FMath::Min(Items.Num(),32);++Attempt) {
  Cursor%=Items.Num(); auto* Class=Items[Cursor++].Get();
  if(!IsValid(Class) || !Class->IsChildOf(UFGItemDescriptor::StaticClass())) continue;
  const FString Path=Class->GetPathName();
  if(!Path.StartsWith(TEXT("/Game/FactoryGame/"))) continue;
  const FString File=FPaths::Combine(Directory,FILNIconHash(Path)+TEXT(".png"));
  if(IFileManager::Get().FileSize(*File)>8) continue;
  auto* Icon=UFGItemDescriptor::GetSmallIcon(TSubclassOf<UFGItemDescriptor>(Class));
  if(!IsValid(Icon) || !Icon->GetResource()) return;
  constexpr int32 Side=64;
  TStrongObjectPtr<UTextureRenderTarget2D> Target(NewObject<UTextureRenderTarget2D>(GetTransientPackage()));
  Target->ClearColor=FLinearColor::Transparent;
  Target->InitCustomFormat(Side,Side,PF_B8G8R8A8,false);
  Target->UpdateResourceImmediate(true);
  auto* Resource=Target->GameThread_GetRenderTargetResource();
  if(!Resource) return;
  FCanvas Canvas(Resource,nullptr,World,World->GetFeatureLevel());
  FCanvasTileItem Tile(FVector2D::ZeroVector,Icon->GetResource(),FVector2D(Side,Side),FLinearColor::White);
  Tile.BlendMode=SE_BLEND_Opaque;
  Canvas.DrawItem(Tile); Canvas.Flush_GameThread();
  TArray<FColor> Pixels;
  if(!Resource->ReadPixels(Pixels) || Pixels.Num()!=Side*Side) return;
  Pending=Async(EAsyncExecution::ThreadPool,[Directory,File,Pixels=MoveTemp(Pixels)]() {
   TArray<uint8> Png; FImageUtils::CompressImageArray(64,64,Pixels,Png);
   if(Png.IsEmpty() || Png.Num()>65536) return;
   IFileManager::Get().MakeDirectory(*Directory,true);
   const FString Temp=File+TEXT(".")+FGuid::NewGuid().ToString()+TEXT(".tmp");
   if(!FFileHelper::SaveArrayToFile(Png,*Temp) || !IFileManager::Get().Move(*File,*Temp,true,true)) IFileManager::Get().Delete(*Temp,false,true);
  });
  return;
 }
}
}
TSharedPtr<const FJsonObject> ALogisticsSubsystem::OrganizerSnapshot() {
 check(IsInGameThread());
 OrganizerDemandUntil=FPlatformTime::Seconds()+2.;
 return LatestOrganizerSnapshot;
}
void ALogisticsSubsystem::ResetTelemetry() {
 LatestOrganizerSnapshot.Reset(); OrganizerDemandUntil=0;
 TelemetryDemand.Interrupted(bTelemetryCollecting);
 bTelemetryCollecting=false;
 NextTelemetryDemandCheck=0; NextTelemetryStart=0;
 TelemetryStationQueue.Reset(); TelemetryVehicleQueue.Reset();
 TelemetryStations.Reset(); TelemetryVehicles.Reset(); TelemetryErrors.Reset(); TelemetryCapacityRows.Reset();
 TelemetryResources.Reset(); TelemetryResourceSet.Reset(); TelemetryTypes.Reset(); TelemetryTypeKeys.Reset();
 TelemetryTypeCursor=TelemetryResourceCursor=0;
}
void ALogisticsSubsystem::RefreshNativeDashboardHeartbeat() {
 // One small atomic file every five real seconds, independent of browser demand.
 // Never launch processes or the browser from the game.
 const double Now=FPlatformTime::Seconds();
 if(Now<NextNativeDashboardHeartbeat) return;
 NextNativeDashboardHeartbeat=Now+5.;
 if(TelemetryWorld.IsEmpty()) {
  TelemetryWorld=FGuid::NewGuid().ToString();
  UE_LOG(LogFILN,Log,TEXT("FILN_TELEMETRY_DIRECTORY %s world=%s"),*TelemetryDirectory(),*TelemetryWorld);
 }
 const FString Directory=TelemetryDirectory();
 const FString Target=FPaths::Combine(Directory,TEXT("dashboard-native-mod.txt"));
 const FString Temp=Target+TEXT(".")+TelemetryWorld+TEXT(".tmp");
 const FString Body=FString::Printf(TEXT("%lld\n%s\n"),static_cast<long long>(Stamp()),*TelemetryWorld);
 IFileManager::Get().MakeDirectory(*Directory,true);
 const bool Written=FFileHelper::SaveStringToFile(Body,*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) &&
  IFileManager::Get().Move(*Target,*Temp,true,true);
 if(!Written) {
  IFileManager::Get().Delete(*Temp,false,true);
  if(!bNativeDashboardHeartbeatError) UE_LOG(LogFILN,Error,TEXT("Native dashboard heartbeat write failed: %s"),*Target);
 }
 bNativeDashboardHeartbeatError=!Written;
}
void ALogisticsSubsystem::StopNativeDashboardHeartbeat() {
 if(!HasAuthority() || TelemetryWorld.IsEmpty()) return;
 const FString Target=FPaths::Combine(TelemetryDirectory(),TEXT("dashboard-native-mod.txt"));
 FString Body;
 TArray<FString> Lines;
 if(FFileHelper::LoadFileToString(Body,*Target)) {
  Body.ParseIntoArrayLines(Lines);
  if(Lines.Num()==2 && Lines[1]==TelemetryWorld) IFileManager::Get().Delete(*Target,false,true);
 }
}
void ALogisticsSubsystem::SampleTelemetry() {
 // Demand-driven, no actor changes, no engine browser calls, no shell/process
 // launch. Only authoritative game-thread samples; writer sees plain JSON.
 if(!HasAuthority() || !GetWorld() || GetWorld()->bIsTearingDown) return;
 RefreshNativeDashboardHeartbeat();
 const double Now=FPlatformTime::Seconds();
 if(Now>=NextTelemetryDemandCheck) {
  NextTelemetryDemandCheck=Now+1.;
  const FString RequestPath=FPaths::Combine(TelemetryDirectory(),TEXT("dashboard-demand.txt"));
  const auto Modified=IFileManager::Get().GetTimeStamp(*RequestPath);
  const double Age=(FDateTime::UtcNow()-Modified).GetTotalSeconds();
  TelemetryDemand.Observe(Modified.GetTicks(),Age>=-5. && Age<=30.);
 }
 // Publish one real initial snapshot even without a browser; subsequent scans are demand-driven.
 if(TelemetryWrite.IsValid() && TelemetryWrite.IsReady()) {
  const bool Written=TelemetryWrite.Get();
  TelemetryWrite=TFuture<bool>();
  if(Written) {
   if(!bTelemetryPublished) {
    UE_LOG(LogFILN,Log,TEXT("FILN_TELEMETRY_FIRST_SNAPSHOT %s"),*FPaths::Combine(TelemetryDirectory(),TEXT("telemetry.json")));
   }
   UE_LOG(LogFILN,Log,TEXT("FILN_TELEMETRY_PUBLISHED sequence=%lld"),static_cast<long long>(TelemetrySequence));
  } else {
   UE_LOG(LogFILN,Error,TEXT("FILN_TELEMETRY_WRITE_FAILED %s"),*TelemetryDirectory());
   TelemetryDemand.Failed();
  }
  bTelemetryPublished=bTelemetryPublished || Written;
 }
 // Only gate the start of a scan. Never discard accepted work when the
 // demand file ages out, including during a long scan or a paused world.
 if(!bTelemetryCollecting) {
  if(!filn::organizer::RequestScan(TelemetryDemand.ShouldStart(bTelemetryPublished),Now,OrganizerDemandUntil)) return;
  if(Now<NextTelemetryStart || (TelemetryWrite.IsValid() && !TelemetryWrite.IsReady())) return;
  auto* VS=AFGVehicleSubsystem::Get(GetWorld());
  if(!VS) {
   if(!bTelemetryRosterMissing) UE_LOG(LogFILN,Error,TEXT("FILN_TELEMETRY_ROSTER_UNAVAILABLE"));
   bTelemetryRosterMissing=true; NextTelemetryStart=Now+5.; return;
  }
  bTelemetryRosterMissing=false;
  TelemetryStationQueue.Reset(); TelemetryVehicleQueue.Reset();
  TelemetryStations.Reset(); TelemetryVehicles.Reset(); TelemetryErrors.Reset(); TelemetryCapacityRows.Reset();
  TelemetryResources.Reset(); TelemetryResourceSet.Reset(); TelemetryTypes.Reset(); TelemetryTypeKeys.Reset();
  TelemetryTypeCursor=TelemetryResourceCursor=0;
  for(auto* S:VS->GetDockingStations()) if(IsValid(S)) TelemetryStationQueue.Add(S);
  for(auto* V:VS->GetAllVehicles()) if(IsValid(V)) TelemetryVehicleQueue.Add(V);
  TelemetryStationCursor=TelemetryVehicleCursor=0; TelemetryStarted=Stamp();
  check(!TelemetryWorld.IsEmpty());
  TelemetryDemand.Started();
  bTelemetryCollecting=true;
  UE_LOG(LogFILN,Log,TEXT("FILN_TELEMETRY_SCAN_STARTED next=%lld stations=%d vehicles=%d"),static_cast<long long>(TelemetrySequence+1),TelemetryStationQueue.Num(),TelemetryVehicleQueue.Num());
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
   else if(Config) Error(Guid(S),TEXT("Saved FILN resource configuration failed native filter readback"));
   const auto Saved=Config?Config->Resource:TSubclassOf<UFGItemDescriptor>();
   const auto Stock=InspectStock(B->GetInventory(),Saved?Saved:Selected);
   const FString Medium=FILNTelemetryMedium(B->GetDockingStationResourceForm());
   const bool Explicit=Selected && StationAcceptsResource(B,Selected);
   const bool Supply=Load && Stock.Valid && Stock.Kinds==1 && Stock.Total>0 && StationAcceptsResource(B,Stock.Item) && (!Config || Explicit);
   const bool Configured=!Depot && Stock.Valid && (Explicit || Supply) && Stock.Kinds<=1 && (!Selected || Stock.Total==Stock.Amount);
   if(Configured && Stock.Item) {
    const TWeakObjectPtr<UClass> Resource(Stock.Item.Get());
    if(!TelemetryResourceSet.Contains(Resource)) { TelemetryResourceSet.Add(Resource); TelemetryResources.Add(Resource); }
   }
   if(!Stock.Valid) Error(Guid(S),TEXT("Station inventory not readable"));
   if(Explicit && Stock.Total!=Stock.Amount) Error(Guid(S),TEXT("Station cargo conflicts with selected resource"));
   auto O=Obj(); O->SetStringField(TEXT("id"),Guid(S)); O->SetStringField(TEXT("guid"),Guid(S)); O->SetStringField(TEXT("name"),S->GetStationName().ToString());
   O->SetStringField(TEXT("buildable_class"),B->GetClass()->GetPathName());
   const auto BuiltWith=B->GetBuiltWithRecipe();
   O->SetStringField(TEXT("built_with_recipe"),BuiltWith?BuiltWith->GetPathName():FString());
   O->SetStringField(TEXT("medium"),Medium); O->SetStringField(TEXT("quantity_unit"),Medium==TEXT("fluid")?TEXT("native fluid units"):TEXT("items"));
   O->SetStringField(TEXT("role"),Load?TEXT("provider"):TEXT("requester")); O->SetBoolField(TEXT("explicit_filter"),Explicit); O->SetBoolField(TEXT("configured"),Configured); O->SetBoolField(TEXT("is_depot"),Depot);
   O->SetStringField(TEXT("item"),Configured?FILNTelemetryDescriptorPath(Stock.Item):FString());
   O->SetStringField(TEXT("saved_item"),FILNTelemetryDescriptorPath(Saved));
   O->SetStringField(TEXT("observed_item"),!Saved && Stock.Kinds==1?FILNTelemetryDescriptorPath(Stock.Item):FString());
   O->SetBoolField(TEXT("filter_valid"),!Config || Explicit);
   O->SetBoolField(TEXT("stock_valid"),Stock.Valid);
   O->SetNumberField(TEXT("display_amount"),static_cast<double>(Stock.Amount));
   O->SetNumberField(TEXT("display_capacity"),Stock.Valid?static_cast<double>(Stock.Capacity):0.); O->SetNumberField(TEXT("amount"),static_cast<double>(Stock.Total));
   O->SetNumberField(TEXT("capacity"),Configured?static_cast<double>(Stock.Capacity):0.); O->SetArrayField(TEXT("contents"),Stock.Contents);
   O->SetBoolField(TEXT("powered"),B->HasPower()); O->SetBoolField(TEXT("standby"),B->IsProductionPaused()); O->SetBoolField(TEXT("occupied"),B->GetDockedActor()!=nullptr);
   O->SetObjectField(TEXT("position"),Position(B->GetActorLocation())); O->SetObjectField(TEXT("fuel"),InspectStock(B->GetFuelInventory()).Json);
   O->SetNumberField(TEXT("sampled_at"),static_cast<double>(Stamp())); TelemetryStations.Add(Val(O)); continue;
  }
  if(TelemetryVehicleCursor<TelemetryVehicleQueue.Num()) {
   auto* V=TelemetryVehicleQueue[TelemetryVehicleCursor++].Get();
   auto* VehicleOwner=IsValid(V)?V->GetOwnerVehicle():nullptr;
   if(!IsValid(VehicleOwner)) { Error(TEXT("vehicle"),TEXT("Vehicle owner unavailable")); continue; }
   const auto* Index=ByVehicle.Find(V); const auto* R=Index && Registrations.IsValidIndex(*Index)?&Registrations[*Index]:nullptr;
   auto* H=R && IsValid(R->Home)?R->Home->GetStation():nullptr;
   const auto Cargo=InspectStock(VehicleOwner->GetStorageInventory(),R && R->JobId.IsValid()?R->Resource:TSubclassOf<UFGItemDescriptor>());
   auto Fuel=InspectStock(VehicleOwner->GetFuelInventory());
   if(!Fuel.Item && IsValid(H)) Fuel=InspectStock(VehicleOwner->GetFuelInventory(),InspectStock(H->GetFuelInventory()).Item);
   if(!Cargo.Valid || !Fuel.Valid) Error(V->GetPathName(),TEXT("Vehicle cargo or fuel inventory not readable"));
   auto O=Obj(); const FString Id=R?R->TruckId.ToString():V->GetPathName();
   O->SetStringField(TEXT("id"),Id); O->SetStringField(TEXT("key"),V->GetPathName()); O->SetStringField(TEXT("name"),V->GetVehicleName().ToString());
   const FString VehicleClass=V->GetVehicleClass()?V->GetVehicleClass()->GetPathName():FString();
   O->SetStringField(TEXT("class"),VehicleClass);
   O->SetBoolField(TEXT("managed"),R!=nullptr);
   O->SetBoolField(TEXT("filn_member"),R!=nullptr || PendingDepotMembership.Contains(V));
   O->SetBoolField(TEXT("registration_pending"),!R && PendingDepotMembership.Contains(V));
   O->SetBoolField(TEXT("depot_capacity_warning"),HasDepotCapacityWarning(V)); O->SetBoolField(TEXT("home_powered"),IsValid(H) && H->HasPower() && !H->IsProductionPaused());
   O->SetBoolField(TEXT("autopilot"),V->IsAutopilotEnabled()); O->SetBoolField(TEXT("driver"),VehicleOwner->GetDriver()!=nullptr);
   O->SetBoolField(TEXT("has_fuel"),VehicleOwner->HasFuel()); O->SetBoolField(TEXT("docking"),V->IsCurrentlyDocking());
   const FString Medium=VehicleOwner->GetClass()->ImplementsInterface(UFGDockableInterface::StaticClass())?FILNTelemetryMedium(IFGDockableInterface::Execute_GetDockInventoryResourceForm(VehicleOwner)):TEXT("unknown");
   O->SetBoolField(TEXT("held"),R && R->Held); O->SetStringField(TEXT("medium"),Medium);
   O->SetStringField(TEXT("quantity_unit"),Medium==TEXT("fluid")?TEXT("native fluid units"):TEXT("items"));
   O->SetNumberField(TEXT("native_error"),static_cast<int32>(V->GetAutopilotErrorStatus())); O->SetNumberField(TEXT("target"),V->GetCurrentTargetWaypointIndex());
   Cargo.Json->SetBoolField(TEXT("valid"),Cargo.Valid);
   O->SetObjectField(TEXT("cargo"),Cargo.Json); O->SetObjectField(TEXT("fuel"),Fuel.Json);
   FVector Location; FRotator Rotation; VehicleOwner->GetVehicleLocationAndRotation(Location,Rotation);
   O->SetObjectField(TEXT("position"),Position(Location));
   TArray<Value> Route; for(const auto& Node:V->GetVehicleRoute()) Route.Add(MakeShared<FJsonValueString>(Node.ToString()));
   O->SetArrayField(TEXT("route"),Route); O->SetNumberField(TEXT("sampled_at"),static_cast<double>(Stamp()));
   if(R) {
    O->SetStringField(TEXT("home"),Guid(R->Home));
    auto J=Obj(); J->SetStringField(TEXT("kind"),R->RetainedCargo?TEXT("cargo"):TEXT("pickup")); J->SetStringField(TEXT("state"),Phase(*R));
    J->SetStringField(TEXT("provider"),Guid(R->Provider)); J->SetStringField(TEXT("requester"),Guid(R->Requester)); J->SetStringField(TEXT("item"),FILNTelemetryDescriptorPath(R->Resource));
    J->SetNumberField(TEXT("loaded"),R->LoadedCargo); J->SetNumberField(TEXT("unloaded"),R->DeliveredCargo); J->SetNumberField(TEXT("truckload"),R->ExpectedCargo); J->SetStringField(TEXT("error"),R->Diagnostic);
    O->SetObjectField(TEXT("job"),J);
   } else if(const auto* Why=PendingHomeReason.Find(TWeakObjectPtr<AFGWheeledVehicleIdentifier>(V))) O->SetStringField(TEXT("native_pending_reason"),*Why);
   if(R && Cargo.Valid && IsValid(H) && H->HasPower() && !H->IsProductionPaused() && V->IsAutopilotEnabled() && !VehicleOwner->GetDriver() && VehicleStationCompatible(VehicleOwner,H) && Medium!=TEXT("unknown") && !VehicleClass.IsEmpty()) {
    auto* Inventory=VehicleOwner->GetStorageInventory(); const int32 Slots=Inventory->GetSizeLinear();
    const FString TypeKey=VehicleClass+TEXT("|")+FString::FromInt(Slots)+TEXT("|")+Medium;
    if(!TelemetryTypeKeys.Contains(TypeKey)) {
     TelemetryTypeKeys.Add(TypeKey); auto Row=Obj(), ByItem=Obj();
     Row->SetStringField(TEXT("class"),VehicleClass); Row->SetStringField(TEXT("medium"),Medium); Row->SetNumberField(TEXT("slots"),Slots); Row->SetStringField(TEXT("sampled_vehicle"),Id); Row->SetObjectField(TEXT("by_item"),ByItem);
     TelemetryCapacityRows.Add(Val(Row)); TelemetryTypes.Add({Inventory,Slots,ByItem,Medium==TEXT("fluid")});
    }
   }
   TelemetryVehicles.Add(Val(O)); continue;
  }
  if(TelemetryTypeCursor<TelemetryTypes.Num() && !TelemetryResources.IsEmpty()) {
   auto& Type=TelemetryTypes[TelemetryTypeCursor]; auto* I=Type.Inventory.Get(); auto* Class=TelemetryResources[TelemetryResourceCursor++].Get();
   if(IsValid(I) && IsValid(Class) && I->GetSizeLinear()==Type.Slots && Type.Slots>0 && Type.Slots<=4096 && Class->IsChildOf(UFGItemDescriptor::StaticClass())) {
    TSubclassOf<UFGItemDescriptor> Resource(Class); int64 Capacity=0; bool Valid=true;
    const auto Form=UFGItemDescriptor::GetForm(Resource);
    const bool Compatible=Type.Fluid?(Form==EResourceForm::RF_LIQUID || Form==EResourceForm::RF_GAS):Form==EResourceForm::RF_SOLID;
    if(Compatible) Valid=filn::inventory::TryCapacity(I,Resource,Capacity);
    if(Compatible) {
     if(Valid && Capacity<=MAX_int32) Type.ByItem->SetNumberField(Class->GetPathName(),static_cast<double>(Capacity));
     else Error(Class->GetPathName(),TEXT("Vehicle capacity readback invalid"));
    }
   } else Error(TEXT("capacity"),TEXT("Representative inventory or resource changed during scan"));
   if(TelemetryResourceCursor>=TelemetryResources.Num()) { ++TelemetryTypeCursor; TelemetryResourceCursor=0; }
   // One exact capacity calculation per scheduler callback; no matrix spike.
   break;
  }
  // Icon work only while a local dashboard has requested fresh telemetry.
  const double IconDemandAge=(FDateTime::UtcNow()-IFileManager::Get().GetTimeStamp(*FPaths::Combine(TelemetryDirectory(),TEXT("dashboard-demand.txt")))).GetTotalSeconds();
  if(IconDemandAge>=-5. && IconDemandAge<=30.) FILNCacheDashboardIcon(GetWorld(),TelemetryResources);
  // Keep previous complete snapshot until the next one has been written.
  auto Snapshot=Obj(); auto Dispatcher=Obj(); auto Leases=Obj(); auto Stats=Obj();
  for(const auto& R:Registrations) if(R.JobId.IsValid()) {
   if(IsValid(R.Provider)) Leases->SetStringField(Guid(R.Provider),Guid(R.Home));
   if(IsValid(R.Requester)) Leases->SetStringField(Guid(R.Requester),Guid(R.Home));
  }
  Dispatcher->SetBoolField(TEXT("registration_supported"),false);
  Dispatcher->SetBoolField(TEXT("enabled"),bAutoDispatchEnabled && bRegistryValidated && !bDispatchBlocked && !bPreparingUninstall && !bUninstallPrepared);
  Dispatcher->SetBoolField(TEXT("cargo_reuse_enabled"),true); Dispatcher->SetBoolField(TEXT("storage_tag_rule_enabled"),true);
  Dispatcher->SetBoolField(TEXT("registry_validated"),bRegistryValidated); Dispatcher->SetBoolField(TEXT("dispatch_blocked"),bDispatchBlocked);
   Dispatcher->SetNumberField(TEXT("configured_station_count"),StationConfigs.Num()); Dispatcher->SetNumberField(TEXT("registered_vehicle_count"),Registrations.Num()); Dispatcher->SetNumberField(TEXT("pending_membership_count"),PendingDepotMembership.Num()); Dispatcher->SetNumberField(TEXT("tracked_depot_count"),DepotStations.Num()); Dispatcher->SetBoolField(TEXT("depot_recipe_ready"),bDepotRecipeReady);
  Dispatcher->SetObjectField(TEXT("reservations"),Leases);
  Stats->SetNumberField(TEXT("graph_hits"),static_cast<double>(RoadCache.Hits)); Stats->SetNumberField(TEXT("graph_builds"),static_cast<double>(RoadCache.Misses)); Stats->SetNumberField(TEXT("rank_invalidations"),static_cast<double>(RoadCache.Invalidations)); Stats->SetNumberField(TEXT("graph_work"),static_cast<double>(RoadCache.Work)); Dispatcher->SetObjectField(TEXT("stats"),Stats);
  Snapshot->SetStringField(TEXT("icon_cache_revision"),FILNIconRevision());
  Snapshot->SetNumberField(TEXT("schema_version"),1); Snapshot->SetStringField(TEXT("runtime"),TEXT("native")); Snapshot->SetStringField(TEXT("world"),TelemetryWorld);
  Snapshot->SetNumberField(TEXT("sequence"),static_cast<double>(++TelemetrySequence)); Snapshot->SetNumberField(TEXT("exported_at"),static_cast<double>(Stamp()));
  Snapshot->SetNumberField(TEXT("collection_seconds"),static_cast<double>(Stamp()-TelemetryStarted)); Snapshot->SetNumberField(TEXT("collection_started_at"),static_cast<double>(TelemetryStarted));
  Snapshot->SetArrayField(TEXT("stations"),TelemetryStations); Snapshot->SetArrayField(TEXT("vehicles"),TelemetryVehicles); Snapshot->SetArrayField(TEXT("errors"),TelemetryErrors);
  // Representative real inventory capacity by vehicle class and slot count,
  // as in Lua telemetry. Unknown values stay absent rather than estimated.
  Snapshot->SetArrayField(TEXT("capacity_samples"),TelemetryCapacityRows);
  Snapshot->SetObjectField(TEXT("dispatcher"),Dispatcher);
  TArray<Value> RegistryIssues; for(const auto& Issue:RegistryDiagnostics) RegistryIssues.Add(MakeShared<FJsonValueString>(Issue));
  Snapshot->SetArrayField(TEXT("registry_errors"),RegistryIssues);
  LatestOrganizerSnapshot=Snapshot; // immutable; same input to async web export
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
  TelemetryStations.Reset(); TelemetryVehicles.Reset(); TelemetryErrors.Reset(); TelemetryCapacityRows.Reset();
  TelemetryResources.Reset(); TelemetryResourceSet.Reset(); TelemetryTypes.Reset(); TelemetryTypeKeys.Reset();
  bTelemetryCollecting=false; NextTelemetryStart=Now+5.; break;
 }
}
