// SPDX-License-Identifier: GPL-3.0-only
#include "LogisticsSubsystem.h"
#include "FILNDispatchPolicy.h"
#include "FILNEnrollment.h"
#include "FILNStationControls.h"
#include "FILNTransferObserver.h"
#include "Resources/FGItemDescriptor.h"
#include "UObject/UnrealType.h"
#include "WheeledVehicles/FGVehicleSubsystem.h"
#include "WheeledVehicles/FGWheeledVehicleIdentifier.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "FGInventoryComponent.h"
#include "WheeledVehicles/FGVehiclePathNode.h"
#include "FGFactoryConnectionComponent.h"
#include "FGPipeConnectionComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogFILN);
namespace {
using S = filn::State;
std::string Key(const FGuid& Id) { return TCHAR_TO_UTF8(*Id.ToString()); }
S StateOf(const FFILNRegistration& R) { return static_cast<S>(R.State); }
void SetState(FFILNRegistration& R,S State) { R.State=static_cast<uint8>(State); }
AFGBuildableDockingStation* Building(AFGDockingStationIdentifier* S) { return IsValid(S)? S->GetStation():nullptr; }
}
ALogisticsSubsystem::ALogisticsSubsystem() {
 ReplicationPolicy=ESubsystemReplicationPolicy::SpawnOnServer;
 PrimaryActorTick.bCanEverTick=false;
}
void ALogisticsSubsystem::BeginPlay() {
 Super::BeginPlay();
 if(!HasAuthority()) return;
 Rebuild();
 GetWorldTimerManager().SetTimer(DispatchTimer,this,&ALogisticsSubsystem::Schedule,0.1f,true);
 GetWorldTimerManager().SetTimer(Watchdog,this,&ALogisticsSubsystem::Observe,1.f,true);
 UE_LOG(LogFILN,Log,TEXT("M1 subsystem ready; schema=%d registrations=%d. Runtime acceptance unverified."),SchemaVersion,Registrations.Num());
}
void ALogisticsSubsystem::EndPlay(const EEndPlayReason::Type Reason) {
 GetWorldTimerManager().ClearTimer(Watchdog);
 GetWorldTimerManager().ClearTimer(DispatchTimer);
 for(const auto& Observer:TransferObservers) if(Observer) Observer->Unbind();
 TransferObservers.Empty();
 Super::EndPlay(Reason);
}
void ALogisticsSubsystem::PostLoadGame_Implementation(int32,int32) {
 // References may not be ready during deserialization; reconcile in BeginPlay.
 for(auto& R:Registrations) {
  if(!R.Automatic || R.State<static_cast<uint8>(S::ToProvider) || R.State>static_cast<uint8>(S::Returning)) SetState(R,filn::AfterReload(StateOf(R)));
  if(StateOf(R)==S::Recovery) R.Diagnostic=TEXT("Saved journey interrupted; inspect and return home before reset");
 }
}
FGuid ALogisticsSubsystem::StationId(AFGDockingStationIdentifier* Station) {
 if(const auto* Id=StationIds.Find(Station)) return *Id;
 FFILNStation Entry; Entry.InstanceId=FGuid::NewGuid(); Entry.Station=Station;
 Stations.Add(Entry); StationIds.Add(Station,Entry.InstanceId); return Entry.InstanceId;
}
void ALogisticsSubsystem::Rebuild() {
 for(const auto& Observer:TransferObservers) if(Observer) Observer->Unbind();
 TransferObservers.Empty();
 Ownership=filn::Ownership(); Reservations=filn::Reservations();
 HomeStations.Empty();
 ByVehicle.Empty(); StationIds.Empty(); Active.Empty(); Cursor=0;
 RoadCache.Clear(); ResetScan();
 bDispatchBlocked=SchemaVersion!=1;
 TSet<FGuid> TruckIdentities, BayIdentities;
 TSet<AFGDockingStationIdentifier*> HomeActors;
 for(const auto& E:Stations) {
  if(!E.InstanceId.IsValid() || (IsValid(E.Station) && StationIds.Contains(E.Station))) bDispatchBlocked=true;
  else if(IsValid(E.Station)) StationIds.Add(E.Station,E.InstanceId);
 }
 for(int32 I=0;I<Registrations.Num();++I) {
  auto& R=Registrations[I];
  if(TruckIdentities.Contains(R.TruckId) || BayIdentities.Contains(R.BayId) || (IsValid(R.Home) && HomeActors.Contains(R.Home))) bDispatchBlocked=true;
  TruckIdentities.Add(R.TruckId); BayIdentities.Add(R.BayId);
  if(IsValid(R.Home)) { HomeActors.Add(R.Home); HomeStations.Add(R.Home.Get()); }
  if(!R.TruckId.IsValid() || !R.BayId.IsValid() || !Ownership.Pair(Key(R.TruckId),Key(R.BayId))) bDispatchBlocked=true;
  if(IsValid(R.Vehicle)) {
   if(ByVehicle.Contains(R.Vehicle)) bDispatchBlocked=true;
   ByVehicle.Add(R.Vehicle,I); WatchTransfers(R);
  }
  if(StateOf(R)!=S::Idle) {
   const bool Resume=R.Automatic && R.Resource && R.ExpectedCargo>0 && R.State>=static_cast<uint8>(S::ToProvider) && R.State<=static_cast<uint8>(S::Returning) &&
    IsValid(R.Vehicle) && IsValid(R.Vehicle->GetOwnerVehicle()) && !R.Vehicle->GetOwnerVehicle()->GetDriver() && R.Vehicle->IsAutopilotEnabled() &&
    R.Vehicle->GetVehicleRoute()==R.RouteIntent && IsValid(R.Home) && IsValid(R.Requester) && (R.RetainedCargo || IsValid(R.Provider));
   if(Resume) { Active.Add(I); R.LastObserved=GetWorld()->GetTimeSeconds(); }
   else SetState(R,S::Recovery);
   if(!Resume && IsValid(R.Vehicle) && IsValid(R.Vehicle->GetOwnerVehicle()) && !R.Vehicle->GetOwnerVehicle()->GetDriver() &&
      !R.RouteIntent.IsEmpty() && R.Vehicle->GetVehicleRoute()==R.RouteIntent && R.Vehicle->IsAutopilotEnabled()) R.Vehicle->SetAutopilotEnabled(false);
   bool Reserved=false;
   if(R.JobId.IsValid() && IsValid(R.Requester)) {
    if(R.RetainedCargo) Reserved=Reservations.AcquireDelivery(Key(R.JobId),Key(R.TruckId),Key(StationId(R.Requester)));
    else if(IsValid(R.Provider)) Reserved=Reservations.Acquire(Key(R.JobId),Key(R.TruckId),Key(StationId(R.Provider)),Key(StationId(R.Requester)));
   }
   if(!Reserved) bDispatchBlocked=true;
  }
 }
 for(const auto& C:StationConfigs) if(IsValid(C.Station)) {
  if(!ApplyStationFilter(C)) bDispatchBlocked=true;
  else if(auto* B=Building(C.Station); B && !B->FindComponentByClass<UFILNStationMarker>()) NewObject<UFILNStationMarker>(B,NAME_None,RF_Transient)->RegisterComponent();
 }
 if(bDispatchBlocked) UE_LOG(LogFILN,Error,TEXT("Dispatch locked: unsupported schema, missing endpoints or conflicting saved identities/reservations. Restore infrastructure/save; do not discard reservations."));
}
bool ALogisticsSubsystem::ApplyStationFilter(const FFILNStationConfig& C,TSubclassOf<UFGItemDescriptor> PriorResource) {
 auto* B=Building(C.Station); auto* Inventory=IsValid(B)?B->GetInventory():nullptr;
 if(!Inventory || !C.Resource || Inventory->GetSizeLinear()<=0 || Inventory->GetSizeLinear()!=C.OriginalFilters.Num()) return false;
 TArray<TSubclassOf<UFGItemDescriptor>> Before;
 // Do not overwrite another mod/operator's changed restriction during recovery.
 for(int32 I=0;I<Inventory->GetSizeLinear();++I) {
  auto Existing=Inventory->GetAllowedItemOnIndex(I);
  if(Existing!=C.Resource && Existing!=C.OriginalFilters[I] && (!PriorResource || Existing!=PriorResource)) return false;
  Before.Add(Existing);
 }
 for(int32 I=0;I<Inventory->GetSizeLinear();++I) Inventory->SetAllowedItemOnIndex(I,C.Resource);
 for(int32 I=0;I<Inventory->GetSizeLinear();++I) if(Inventory->GetAllowedItemOnIndex(I)!=C.Resource) {
  // Native setters are void: read back rather than report a metadata-only success.
  for(int32 J=0;J<Before.Num();++J) {
   const auto Current=Inventory->GetAllowedItemOnIndex(J);
   if(Current==C.Resource) Inventory->SetAllowedItemOnIndex(J,Before[J]);
   else if(Current!=Before[J]) bDispatchBlocked=true;
  }
  for(int32 J=0;J<Before.Num();++J) if(Inventory->GetAllowedItemOnIndex(J)!=Before[J]) bDispatchBlocked=true;
  UE_LOG(LogFILN,Error,TEXT("Native resource filter readback failed; configuration not committed, rollback conflicts=%d"),bDispatchBlocked);
  return false;
 }
 return true;
}
bool ALogisticsSubsystem::ConfigureStation(AFGDockingStationIdentifier* Station,TSubclassOf<UFGItemDescriptor> Resource,bool ImportExisting) {
 auto* B=Building(Station); auto* Inventory=IsValid(B)?B->GetInventory():nullptr;
 if(bDispatchBlocked || !HasAuthority() || !Inventory || !Resource || (!ImportExisting && !Inventory->IsEmpty()) || B->GetDockedActor()) return false;
 if(ImportExisting) {
  TArray<FInventoryStack> Contents; Inventory->GetInventoryStacks(Contents);
  for(const auto& Stack:Contents) if(Stack.NumItems>0 && Stack.Item.GetItemClass()!=Resource) return false;
 }
 if(Reservations.StationBusy(Key(StationId(Station)))) return false;
 for(const auto& R:Registrations) if(R.Home==Station) return false;
 const auto* FluidProperty=FindFProperty<FBoolProperty>(B->GetClass(),TEXT("mIsFluidStorageInventory"));
 if(!FluidProperty) return false;
 const auto Form=UFGItemDescriptor::GetForm(Resource);
 const bool IsFluid=Form==EResourceForm::RF_LIQUID || Form==EResourceForm::RF_GAS;
 if(IsFluid && B->GetCurrentFluidDescriptor() && B->GetCurrentFluidDescriptor()!=Resource) return false;
 if((!IsFluid && Form!=EResourceForm::RF_SOLID) || IsFluid!=FluidProperty->GetPropertyValue_InContainer(B)) return false;
 FFILNStationConfig* Existing=StationConfigs.FindByPredicate([Station](const FFILNStationConfig& C){return C.Station==Station;});
 if(Existing) {
  if(!ApplyStationFilter(*Existing)) return false;
  // Only FILN-owned restrictions may be changed.
  for(const auto& Original:Existing->OriginalFilters) if(Original && Original!=Resource) return false;
  ResetScan();
  FFILNStationConfig Updated=*Existing; Updated.Resource=Resource;
  if(!ApplyStationFilter(Updated,Existing->Resource)) return false;
  *Existing=Updated; return true;
 }
 FFILNStationConfig C; C.Station=Station; C.Resource=Resource;
 for(int32 I=0;I<Inventory->GetSizeLinear();++I) {
  const auto Original=Inventory->GetAllowedItemOnIndex(I);
  if(Original && Original!=Resource) return false;
  C.OriginalFilters.Add(Original);
 }
 if(!ApplyStationFilter(C)) return false;
 StationConfigs.Add(C);
 if(!B->FindComponentByClass<UFILNStationMarker>()) NewObject<UFILNStationMarker>(B,NAME_None,RF_Transient)->RegisterComponent();
 ResetScan(); return true;
}
bool ALogisticsSubsystem::PrepareStationsForUninstall() {
 if(!HasAuthority() || bDispatchBlocked || Reservations.Size()!=0) return false;
 // Validate the complete operation before changing any station. Cargo need not be empty.
 for(const auto& R:Registrations) if(StateOf(R)!=S::Idle) return false;
 for(const auto& C:StationConfigs) {
  auto* B=Building(C.Station); if(!IsValid(B)) continue;
  auto* Inventory=B->GetInventory();
  if(!Inventory || B->GetDockedActor() || Inventory->GetSizeLinear()!=C.OriginalFilters.Num()) return false;
  for(int32 I=0;I<Inventory->GetSizeLinear();++I) if(Inventory->GetAllowedItemOnIndex(I)!=C.Resource) return false;
 }
 for(const auto& C:StationConfigs) {
  auto* B=Building(C.Station); if(!IsValid(B)) continue;
  for(int32 I=0;I<B->GetInventory()->GetSizeLinear();++I) B->GetInventory()->SetAllowedItemOnIndex(I,C.OriginalFilters[I]);
 }
 bAutoDispatchEnabled=false;
 for(const auto& C:StationConfigs) {
  auto* B=Building(C.Station); if(!IsValid(B)) continue;
  for(int32 I=0;I<B->GetInventory()->GetSizeLinear();++I) if(B->GetInventory()->GetAllowedItemOnIndex(I)!=C.OriginalFilters[I]) {
   bDispatchBlocked=true; UE_LOG(LogFILN,Error,TEXT("Uninstall filter restoration readback failed; saved recovery metadata retained")); return false;
  }
 }
 for(const auto& C:StationConfigs) if(auto* B=Building(C.Station); IsValid(B)) {
  if(auto* Marker=B->FindComponentByClass<UFILNStationMarker>()) Marker->DestroyComponent();
 }
 StationConfigs.Empty(); Registrations.Empty(); Stations.Empty(); Rebuild();
 return true;
}
bool ALogisticsSubsystem::ValidHome(AFGDockingStationIdentifier* Home) const {
 auto* B=Building(Home);
 if(!IsValid(B) || !B->GetDockingPathNode() || !B->GetIsInLoadMode() || !B->GetInventory() || !B->GetInventory()->IsEmpty()) return false;
 // Port identities are native reflected components, not connector names.
 TSet<const UFGFactoryConnectionComponent*> FuelPorts;
 const auto* FuelProperty=FindFProperty<FArrayProperty>(B->GetClass(),TEXT("mFuelConnections"));
 const auto* FuelInner=FuelProperty?CastField<FObjectPropertyBase>(FuelProperty->Inner):nullptr;
 if(FuelProperty && FuelInner) {
  FScriptArrayHelper Ports(FuelProperty,FuelProperty->ContainerPtrToValuePtr<void>(B));
  for(int32 I=0;I<Ports.Num();++I)
   if(const auto* Port=Cast<UFGFactoryConnectionComponent>(FuelInner->GetObjectPropertyValue(Ports.GetRawPtr(I)))) FuelPorts.Add(Port);
 }
 TArray<UFGFactoryConnectionComponent*> Belts; B->GetComponents(Belts);
 for(const auto* Connection:Belts) {
  if(!IsValid(Connection)) return false;
  if(!Connection->IsConnected()) continue;
  auto* Inventory=Connection->GetInventory();
  if(!filn::enrollment::ConnectedPortAllowed(Inventory==B->GetInventory(),
     Inventory && Inventory==B->GetFuelInventory(),FuelPorts.Contains(Connection))) return false;
 }
 TArray<UFGPipeConnectionComponent*> Pipes; B->GetComponents(Pipes);
 for(const auto* Connection:Pipes) if(Connection->IsConnected()) return false;
 return true;
}
bool ALogisticsSubsystem::Parked(const FFILNRegistration& R) const {
 auto* Home=Building(R.Home);
 return IsValid(R.Vehicle) && IsValid(R.Vehicle->GetOwnerVehicle()) && IsValid(Home) &&
  Home->GetDockedActor()==R.Vehicle->GetOwnerVehicle() && !R.Vehicle->IsAutopilotEnabled();
}
void ALogisticsSubsystem::Recover(FFILNRegistration& R,const FString& Why) {
 SetState(R,S::Recovery); R.Diagnostic=Why; R.NavigationRetry.Reset();
 // Do not disable a player-owned or externally changed route.
 if(IsValid(R.Vehicle) && IsValid(R.Vehicle->GetOwnerVehicle()) && !R.Vehicle->GetOwnerVehicle()->GetDriver() &&
    !R.RouteIntent.IsEmpty() && R.Vehicle->GetVehicleRoute()==R.RouteIntent && R.Vehicle->IsAutopilotEnabled()) R.Vehicle->SetAutopilotEnabled(false);
 UE_LOG(LogFILN,Warning,TEXT("job=%s RECOVERY: %s (reservations retained)"),*R.JobId.ToString(),*Why);
}
bool ALogisticsSubsystem::Pair(AFGWheeledVehicleIdentifier* V,AFGDockingStationIdentifier* H) {
 using namespace filn::enrollment;
 check(IsInGameThread());
 if(!HasAuthority() || bDispatchBlocked || !IsValid(V) || !IsValid(H) ||
    V->GetWorld()!=GetWorld() || H->GetWorld()!=GetWorld() || !ValidHome(H) || !H->GetPathNodeGUID().IsValid()) return false;
 if(const auto* Existing=ByVehicle.Find(V)) {
  if(!Registrations.IsValidIndex(*Existing)) return false;
  const auto& R=Registrations[*Existing];
  return R.Home==H && StateOf(R)==S::Idle && R.Held && V->IsAutopilotEnabled() && V->GetVehicleRoute()==R.IdleRoute;
 }
 for(const auto& R:Registrations) if(R.Home==H || (StateOf(R)!=S::Idle && (R.Provider==H || R.Requester==H))) return false;
 for(const auto& C:StationConfigs) if(C.Station==H) return false;
 auto* Owner=V->GetOwnerVehicle(); auto* Home=Building(H);
 auto* VS=AFGVehicleSubsystem::Get(GetWorld());
 if(!IsValid(Owner) || !Owner->HasAuthority() || !VS || !Owner->GetStorageInventory() || !Home->GetDockingPathNode()) return false;
 const FGuid Node=H->GetPathNodeGUID();
 const FVector HomeLocation=Home->GetDockingPathNode()->GetActorLocation();
 FVector Position; FRotator Rotation; Owner->GetVehicleLocationAndRotation(Position,Rotation);
 const auto Near=[](const FVector& A,const FVector& B) {
  return FMath::Abs(A.Z-B.Z)<=250.f && FVector::DistSquared(A,B)<=FMath::Square(800.f);
 };
 bool UniqueHome=true, UniqueTruck=true;
 // Explicit enrollment is a rare operator action, not a periodic world scan.
 // Reject duplicate GUIDs and any competing eligible nearby depot or truck.
 for(auto* Other:VS->GetDockingStations()) {
  if(!IsValid(Other) || Other==H) continue;
  if(Other->GetPathNodeGUID()==Node) { UniqueHome=false; break; }
  auto* B=Building(Other);
  if(IsValid(B) && ValidHome(Other) && B->GetDockingPathNode() && Near(Position,B->GetDockingPathNode()->GetActorLocation())) {
   UniqueHome=false; break;
  }
 }
 for(auto* Other:VS->GetAllVehicles()) {
  if(!IsValid(Other) || Other==V) continue;
  if(Other->GetVehicleRoute().Contains(Node)) { UniqueTruck=false; break; }
  auto* OtherOwner=Other->GetOwnerVehicle();
  if(IsValid(OtherOwner)) {
   FVector OtherPosition; FRotator OtherRotation; OtherOwner->GetVehicleLocationAndRotation(OtherPosition,OtherRotation);
   if(Near(OtherPosition,HomeLocation)) { UniqueTruck=false; break; }
  }
 }
 const TArray<FGuid> Before=V->GetVehicleRoute();
 const bool BeforeAP=V->IsAutopilotEnabled();
 const TArray<FGuid> Intended={Node};
 const Route ExistingRoute=Before.IsEmpty()?Route::Empty:Before==Intended?Route::HomeOnly:Route::Other;
 const Snapshot Facts{true,(!Home->GetDockedActor() || Home->GetDockedActor()==Owner),UniqueHome,UniqueTruck,
  Owner->GetDriver()!=nullptr,FMath::Abs(Owner->GetForwardSpeed())<=1.f,Owner->GetStorageInventory()->IsEmpty(),Near(Position,HomeLocation),BeforeAP,ExistingRoute};
 const auto Verdict=Evaluate(Facts);
 if(Verdict==Decision::Reject) return false;
 FFILNRegistration R; R.Vehicle=V; R.Home=H; R.TruckId=FGuid::NewGuid(); R.BayId=StationId(H);
 for(const auto& Other:Registrations) if(Other.BayId==R.BayId || Other.TruckId==R.TruckId) return false;
 if(Reservations.StationBusy(Key(R.BayId))) return false;
 // No native writes for adoption of an already opted-in home-only truck.
 const auto Rollback=[&]() {
  if(Verdict!=Decision::AssignHome) return;
  if(Owner->GetDriver() || V->GetVehicleRoute()!=Intended) {
   bDispatchBlocked=true;
   UE_LOG(LogFILN,Error,TEXT("Enrollment rollback conflict; dispatch locked; inspect vehicle route manually"));
   return;
  }
  V->SetAutopilotEnabled(BeforeAP);
  V->SetVehicleRoute(Before);
  if(V->GetVehicleRoute()!=Before || V->IsAutopilotEnabled()!=BeforeAP) {
   bDispatchBlocked=true;
   UE_LOG(LogFILN,Error,TEXT("Enrollment rollback readback failed; dispatch locked"));
  }
 };
 if(Owner->GetDriver() || V->GetVehicleRoute()!=Before || V->IsAutopilotEnabled()!=BeforeAP ||
    !Owner->GetStorageInventory()->IsEmpty() || !ValidHome(H)) return false;
 if(Verdict==Decision::AssignHome) {
  V->SetVehicleRoute(Intended);
  if(V->GetVehicleRoute()!=Intended) {
   if(V->GetVehicleRoute()!=Before || V->IsAutopilotEnabled()!=BeforeAP) Rollback();
   return false;
  }
  if(Owner->GetDriver()) { Rollback(); return false; }
  V->SetAutopilotEnabled(true);
 }
 if(Owner->GetDriver() || V->GetVehicleRoute()!=Intended || !V->IsAutopilotEnabled()) { Rollback(); return false; }
 if(!Ownership.Pair(Key(R.TruckId),Key(R.BayId))) { Rollback(); return false; }
 R.Held=true; R.IdleRoute=Intended;
 R.Diagnostic=TEXT("Explicit home-only enrollment; native route and autopilot read back");
 HomeStations.Add(H);
 ByVehicle.Add(V,Registrations.Add(R)); WatchTransfers(Registrations.Last()); ResetScan();
 UE_LOG(LogFILN,Log,TEXT("PAIRED truck=%s bay=%s home-only/AP enabled"),*R.TruckId.ToString(),*R.BayId.ToString());
 return true;
}
bool ALogisticsSubsystem::Dispatch(FFILNRegistration& R,AFGDockingStationIdentifier* P,AFGDockingStationIdentifier* Q) {
 auto* PB=Building(P); auto* QB=Building(Q);
 if(bDispatchBlocked || StateOf(R)!=S::Idle || !Parked(R) || !ValidHome(R.Home) ||
  !IsValid(PB) || !IsValid(QB) || P==Q || P==R.Home || Q==R.Home || !PB->GetIsInLoadMode() || QB->GetIsInLoadMode()) return false;
 if(!filn::dispatch::PairAllowed(TCHAR_TO_UTF8(*P->GetStationName().ToString()),TCHAR_TO_UTF8(*Q->GetStationName().ToString()))) return false;
 if(R.Vehicle->GetOwnerVehicle()->GetDriver() || !R.Vehicle->GetOwnerVehicle()->HasFuel()) return false;
 const auto* Config=StationConfigs.FindByPredicate([Q](const FFILNStationConfig& C){return C.Station==Q;});
 if(!Config || !Config->Resource || !QB->GetInventory() || !QB->GetInventory()->IsEmpty() || !PB->GetInventory()) return false;
 const auto Form=UFGItemDescriptor::GetForm(Config->Resource);
 const bool Fluid=Form==EResourceForm::RF_LIQUID || Form==EResourceForm::RF_GAS;
 if(!Fluid && Form!=EResourceForm::RF_SOLID) return false;
 for(const UObject* Object:{static_cast<const UObject*>(PB),static_cast<const UObject*>(QB),static_cast<const UObject*>(R.Vehicle->GetOwnerVehicle())}) {
  const auto* Property=FindFProperty<FBoolProperty>(Object->GetClass(),TEXT("mIsFluidStorageInventory"));
  if(!Property || Property->GetPropertyValue_InContainer(Object)!=Fluid) return false;
 }
 auto* CargoInventory=R.Vehicle->GetOwnerVehicle()->GetStorageInventory();
 if(!CargoInventory) return false;
 int64 FullLoad=0;
 for(int32 I=0;I<CargoInventory->GetSizeLinear();++I) FullLoad+=CargoInventory->GetSlotSizeForItem(I,Config->Resource);
 if(FullLoad<=0 || FullLoad>MAX_int32) return false;
 TArray<FInventoryStack> Stock; PB->GetInventory()->GetInventoryStacks(Stock);
 for(const auto& Stack:Stock) if(Stack.NumItems>0 && Stack.Item.GetItemClass()!=Config->Resource) return false;
 if(PB->GetInventory()->GetNumItems(Config->Resource)<FullLoad ||
    !QB->GetInventory()->HasEnoughSpaceForStack(FInventoryStack(static_cast<int32>(FullLoad),Config->Resource))) return false;
 if(!R.Vehicle->GetOwnerVehicle()->GetStorageInventory() || !R.Vehicle->GetOwnerVehicle()->GetStorageInventory()->IsEmpty()) return false;
 for(const auto& Other:Registrations) if(Other.Home==P || Other.Home==Q) return false;
 const auto* Preset=R.Vehicle->GetOwnerVehicle()->GetVehiclePathPreset();
 auto* Vehicles=AFGVehicleSubsystem::Get(GetWorld());
 if(!Preset || !Vehicles) return false;
 const FGuid Nodes[]={R.Home->GetPathNodeGUID(),P->GetPathNodeGUID(),Q->GetPathNodeGUID(),R.Home->GetPathNodeGUID()};
 // Only explicit dispatch invokes these bounded three game queries. No inventory callback does.
 for(int32 I=0;I<3;++I) {
  if(!Nodes[I].IsValid() || !Nodes[I+1].IsValid()) return false;
  auto* N=Vehicles->FindNetworkByPathNodeGuid(Nodes[I]);
  TArray<FGuid> Path;
  if(!N || N!=Vehicles->FindNetworkByPathNodeGuid(Nodes[I+1]) || !N->FindVehiclePath(Nodes[I],Nodes[I+1],Preset,Path)) {
   UE_LOG(LogFILN,Warning,TEXT("Dispatch rejected: directed leg %d is unreachable"),I); return false;
  }
 }
 const FGuid Job=FGuid::NewGuid();
 if(!Reservations.Acquire(Key(Job),Key(R.TruckId),Key(StationId(P)),Key(StationId(Q)))) return false;
 R.Automatic=false; R.RetainedCargo=false;
 R.JobId=Job; R.Provider=P; R.Requester=Q; R.Elapsed=0; R.LastObserved=GetWorld()->GetTimeSeconds();
 R.RouteIntent={Nodes[1],Nodes[2],Nodes[3]}; SetState(R,S::Reserved);
 R.Vehicle->SetAutopilotEnabled(false);
 R.Vehicle->SetVehicleRoute(R.RouteIntent);
 if(R.Vehicle->GetVehicleRoute()!=R.RouteIntent || !R.Vehicle->CanEnableAutopilot()) {
  Recover(R,TEXT("Game rejected route or autopilot; target-index reset semantics require runtime investigation")); return false;
 }
 SetState(R,S::ToProvider);
 R.Vehicle->SetAutopilotEnabled(true);
 if(!R.Vehicle->IsAutopilotEnabled()) { Recover(R,TEXT("Autopilot enable did not take effect")); return false; }
 Active.AddUnique(ByVehicle.FindChecked(R.Vehicle));
 UE_LOG(LogFILN,Log,TEXT("job=%s TO_PROVIDER route=%s -> %s -> home %s; transfer amounts unmeasured"),*Job.ToString(),*Nodes[1].ToString(),*Nodes[2].ToString(),*Nodes[3].ToString());
 return true;
}
void ALogisticsSubsystem::Observe() {
 check(IsInGameThread());
 ServiceFuel();
 // Bounded round-robin across active jobs only. Recovery and idle records are not scanned.
 const int32 Budget=FMath::Min(16,Active.Num());
 for(int32 Count=0;Count<Budget && Active.Num()>0;++Count) {
  Cursor%=Active.Num(); const int32 Index=Active[Cursor]; auto& R=Registrations[Index];
  if(StateOf(R)==S::Recovery || StateOf(R)==S::Idle) { Active.RemoveAtSwap(Cursor); continue; }
  const double Now=GetWorld()->GetTimeSeconds();
  R.Elapsed+=static_cast<float>(FMath::Max(0.0,Now-R.LastObserved)); R.LastObserved=Now;
  auto* PB=Building(R.Provider); auto* QB=Building(R.Requester); auto* HB=Building(R.Home);
  if(!IsValid(R.Vehicle) || !IsValid(R.Vehicle->GetOwnerVehicle()) || (!R.RetainedCargo && !IsValid(PB)) || !IsValid(QB) || !IsValid(HB)) {
   Recover(R,TEXT("Vehicle or station destroyed/unavailable")); ++Cursor; continue;
  }
  if(!ValidHome(R.Home) || (!R.RetainedCargo && !PB->GetIsInLoadMode()) || QB->GetIsInLoadMode() || R.Vehicle->GetVehicleRoute()!=R.RouteIntent) {
   Recover(R,TEXT("Home cargo/mode, endpoint mode or route changed")); ++Cursor; continue;
  }
  auto* Owner=R.Vehicle->GetOwnerVehicle();
  if(Owner->GetDriver() || !R.Vehicle->IsAutopilotEnabled()) { Recover(R,TEXT("Player control or autopilot disabled; leave vehicle state untouched")); ++Cursor; continue; }
  const bool AtP=PB && PB->GetDockedActor()==Owner, AtQ=QB->GetDockedActor()==Owner, AtH=HB->GetDockedActor()==Owner;
  const S Before=StateOf(R); S Next=Before;
  if(R.Automatic) {
   auto* Cargo=Owner->GetStorageInventory();
   if(!Cargo || !R.Resource) { Recover(R,TEXT("Cargo inventory/resource missing")); ++Cursor; continue; }
   TArray<FInventoryStack> Stacks; Cargo->GetInventoryStacks(Stacks); bool Mixed=false;
   for(const auto& Stack:Stacks) if(Stack.NumItems>0 && Stack.Item.GetItemClass()!=R.Resource) Mixed=true;
   const int32 Quantity=Cargo->GetNumItems(R.Resource);
   const int32 Delta=Quantity-R.LastCargo;
   if(Mixed || Delta!=0) {
    R.CargoOriginKnown=false; R.CargoOriginStorage=false;
    Recover(R,TEXT("Cargo reconciliation disagrees with native source/target transfer events; no inferred transfer credited")); ++Cursor; continue;
   }
   R.LastCargo=Quantity;
   if((Before==S::Loading && !AtP && (Quantity<=0 || R.LoadedCargo<=0)) ||
      (Before==S::Unloading && !AtQ && R.DeliveredCargo<=0)) {
    Recover(R,TEXT("Station departed without measured matching cargo transfer")); ++Cursor; continue;
   }
  }
  const bool Travelling=Before==S::ToProvider || Before==S::ToRequester || Before==S::Returning;
  const FVector Location=Owner->GetVehicleLocation();
  const int32 Waypoint=R.Vehicle->GetCurrentTargetWaypointIndex();
  const bool Moved=!R.TravelProgress.initialized || Waypoint!=R.ProgressWaypoint ||
    FVector::DistSquared(Location,R.ProgressLocation)>=FMath::Square(200.f);
  const bool Docking=!Travelling || AtP || AtQ || AtH || R.Vehicle->IsCurrentlyDocking();
  if(Moved || Docking) { R.ProgressLocation=Location; R.ProgressWaypoint=Waypoint; }
  const bool Stalled=R.TravelProgress.Sample(Now,Moved,Docking);
  if((Stalled || R.Vehicle->GetAutopilotErrorStatus()!=EVehicleAutopilotErrorStatus::None) && !Docking) {
   if(!R.Automatic || !RetryNavigation(R,Now,Stalled)) Recover(R,FString::Printf(TEXT("Native autopilot error=%d; automatic retry is unsafe"),static_cast<int32>(R.Vehicle->GetAutopilotErrorStatus())));
   ++Cursor; continue;
  }
  if(R.NavigationRetry.waiting && (Moved || Docking)) {
   R.NavigationRetry.Reset(); R.Elapsed=0;
   R.Diagnostic=TEXT("Native navigation fault cleared; continuing reserved journey, no transfer inferred");
  }
  if(R.Elapsed>1800.f) { Recover(R,TEXT("Journey timeout without a native navigation error")); ++Cursor; continue; }
  if(Before==S::ToProvider && AtP) Next=filn::Advance(Before,filn::Event::ProviderDock);
  else if(Before==S::Loading && !AtP) Next=filn::Advance(Before,filn::Event::ProviderLeave);
  else if(Before==S::ToRequester && AtQ) Next=filn::Advance(Before,filn::Event::RequesterDock);
  else if(Before==S::Unloading && !AtQ) Next=filn::Advance(Before,filn::Event::RequesterLeave);
  else if(Before==S::Returning && AtH) {
   if(!R.Automatic) R.Vehicle->SetAutopilotEnabled(false);
   auto* Cargo=Owner->GetStorageInventory();
   if(!Cargo || (!R.Automatic && !Cargo->IsEmpty()) || (R.Automatic && R.DeliveredCargo<=0 && !(R.LegacyDelivery && Cargo->IsEmpty()))) { Recover(R,TEXT("Returned with cargo: incomplete unload requires operator recovery")); ++Cursor; continue; }
   if(R.Automatic && FMath::Abs(Owner->GetForwardSpeed())<=1.f) {
    if(!RestoreHomeRoute(R)) { Recover(R,TEXT("Home route restoration failed; reservations retained")); ++Cursor; continue; }
    Next=filn::Advance(Before,filn::Event::HomeStopped);
   } else if(!R.Automatic && Parked(R)) Next=filn::Advance(Before,filn::Event::HomeStopped);
  }
  else if((Before==S::ToProvider && AtQ) || (Before==S::ToRequester && AtH) || (Before==S::Returning && AtP)) Next=S::Recovery;
  if(Next==S::Recovery) Recover(R,TEXT("Out-of-order docking observed; journey cannot be proven"));
  else if(Next!=Before) {
   SetState(R,Next);
   UE_LOG(LogFILN,Log,TEXT("job=%s state=%d; docking observation only, transfer unverified"),*R.JobId.ToString(),R.State);
   if(Next==S::Completed) {
    Reservations.Release(Key(R.JobId)); SetState(R,S::Idle); R.Diagnostic=R.Automatic?FString::Printf(TEXT("Home stopped; loaded=%d delivered=%d retained=%d; native autopilot tick held at home"),R.LoadedCargo,R.DeliveredCargo,R.LastCargo):TEXT("Home stopped; transfers not measured");
    R.JobId.Invalidate(); R.Provider=nullptr; R.Requester=nullptr; R.RouteIntent.Empty(); R.LegacyDelivery=false;
    R.NavigationRetry.Reset(); R.TravelProgress.Reset();
    Active.RemoveAtSwap(Cursor); continue;
   }
  }
  if(!R.Vehicle->IsAutopilotEnabled() && StateOf(R)!=S::Idle && StateOf(R)!=S::Recovery)
   Recover(R,TEXT("Autopilot stopped before confirmed return"));
  ++Cursor;
 }
}
void ALogisticsSubsystem::Execute(const FString& Command,const TArray<FString>& Args) {
 check(IsInGameThread()); if(!HasAuthority()) return;
 auto* VS=AFGVehicleSubsystem::Get(GetWorld()); if(!VS) return;
 if(Command==TEXT("list")) {
  for(auto* V:VS->GetAllVehicles()) if(IsValid(V)) UE_LOG(LogFILN,Log,TEXT("vehicle %s name=%s"),*V->GetPathName(),*V->GetVehicleName().ToString());
  for(auto* H:VS->GetDockingStations()) if(IsValid(H)) UE_LOG(LogFILN,Log,TEXT("station %s name=%s node=%s"),*H->GetPathName(),*H->GetStationName().ToString(),*H->GetPathNodeGUID().ToString());
  return;
 }
 if(Command==TEXT("legacyJournal") && Args.Num()==2 && (Args[0]==TEXT("preview") || Args[0]==TEXT("import"))) { ImportLegacyJournal(Args[1],Args[0]==TEXT("import")); return; }
 if(Command==TEXT("legacy") && Args.Num()==1 && (Args[0]==TEXT("preview") || Args[0]==TEXT("import"))) { ImportLegacy(Args[0]==TEXT("import")); return; }
 if(Command==TEXT("removeStation") && Args.Num()==1) {
  for(auto* Station:VS->GetDockingStations()) if(IsValid(Station) && Station->GetPathName()==Args[0]) { RemoveStation(Station); return; }
 }
 if(Command==TEXT("enable") && Args.Num()==1 && (Args[0]==TEXT("0") || Args[0]==TEXT("1"))) {
  bAutoDispatchEnabled=Args[0]==TEXT("1"); ResetScan();
  UE_LOG(LogFILN,Log,TEXT("Automatic dispatch=%d; existing journeys continue"),bAutoDispatchEnabled); return;
 }
 if(Command==TEXT("status")) {
  UE_LOG(LogFILN,Log,TEXT("automatic=%d blocked=%d stations=%d cache hits=%llu misses=%llu invalidations=%llu work=%llu"),bAutoDispatchEnabled,bDispatchBlocked,StationConfigs.Num(),RoadCache.Hits,RoadCache.Misses,RoadCache.Invalidations,RoadCache.Work);
  for(const auto& R:Registrations) UE_LOG(LogFILN,Log,TEXT("truck=%s bay=%s job=%s state=%d diagnostic=%s"),*R.TruckId.ToString(),*R.BayId.ToString(),*R.JobId.ToString(),R.State,*R.Diagnostic);
  return;
 }
 AFGWheeledVehicleIdentifier* V=nullptr;
 if(Args.Num()>0) for(auto* Candidate:VS->GetAllVehicles()) if(IsValid(Candidate) && Candidate->GetPathName()==Args[0]) { V=Candidate; break; }
 auto FindStation=[VS](const FString& Path)->AFGDockingStationIdentifier* {
  for(auto* H:VS->GetDockingStations()) if(IsValid(H) && H->GetPathName()==Path) return H;
  return nullptr;
 };
 if(Command==TEXT("station") && Args.Num()==2) {
  auto Resource=LoadClass<UFGItemDescriptor>(nullptr,*Args[1]);
  const bool Ok=ConfigureStation(FindStation(Args[0]),Resource);
  UE_LOG(LogFILN,Log,TEXT("Station resource configuration: %s"),Ok?TEXT("accepted"):TEXT("rejected")); return;
 }
 if(Command==TEXT("prepareUninstall") && Args.Num()==0) {
  const bool Ok=PrepareStationsForUninstall();
  UE_LOG(LogFILN,Log,TEXT("Prepare uninstall: %s. Independent save/load without FILN remains unverified."),Ok?TEXT("station restrictions restored; save required"):TEXT("rejected; resolve active jobs/filter conflicts")); return;
 }
 bool Ok=false;
 if(V && Command==TEXT("pair") && Args.Num()==2) Ok=Pair(V,FindStation(Args[1]));
 else if(V) if(const auto* I=ByVehicle.Find(V)) {
  auto& R=Registrations[*I];
  if(Command==TEXT("dispatch") && Args.Num()==3) Ok=Dispatch(R,FindStation(Args[1]),FindStation(Args[2]));
  if(Command==TEXT("reset") && Args.Num()==1 && SchemaVersion==1 && Parked(R) && ValidHome(R.Home)) {
   Reservations.Release(Key(R.JobId)); SetState(R,S::Idle); R.RouteIntent.Empty(); R.Provider=nullptr; R.Requester=nullptr;
   R.JobId.Invalidate(); R.Diagnostic=TEXT("Operator confirmed home reset"); Active.Remove(*I); Ok=true;
  }
 }
 UE_LOG(LogFILN,Log,TEXT("%s: %s"),*Command,Ok?TEXT("accepted"):TEXT("rejected; inspect filn.status and preconditions"));
}
