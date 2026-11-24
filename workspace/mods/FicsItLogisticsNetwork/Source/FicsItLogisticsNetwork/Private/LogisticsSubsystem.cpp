// SPDX-License-Identifier: GPL-3.0-only
#include "LogisticsSubsystem.h"
#include "WheeledVehicles/FGVehicleSubsystem.h"
#include "WheeledVehicles/FGWheeledVehicleIdentifier.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "FGInventoryComponent.h"
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
 GetWorldTimerManager().SetTimer(Watchdog,this,&ALogisticsSubsystem::Observe,1.f,true);
 UE_LOG(LogFILN,Log,TEXT("M1 subsystem ready; schema=%d registrations=%d. Runtime acceptance unverified."),SchemaVersion,Registrations.Num());
}
void ALogisticsSubsystem::EndPlay(const EEndPlayReason::Type Reason) {
 GetWorldTimerManager().ClearTimer(Watchdog);
 Super::EndPlay(Reason);
}
void ALogisticsSubsystem::PostLoadGame_Implementation(int32,int32) {
 // References may not be ready during deserialization; reconcile in BeginPlay.
 for(auto& R:Registrations) {
  SetState(R,filn::AfterReload(StateOf(R)));
  if(StateOf(R)==S::Recovery) R.Diagnostic=TEXT("Saved journey interrupted; inspect and return home before reset");
 }
}
FGuid ALogisticsSubsystem::StationId(AFGDockingStationIdentifier* Station) {
 if(const auto* Id=StationIds.Find(Station)) return *Id;
 FFILNStation Entry; Entry.InstanceId=FGuid::NewGuid(); Entry.Station=Station;
 Stations.Add(Entry); StationIds.Add(Station,Entry.InstanceId); return Entry.InstanceId;
}
void ALogisticsSubsystem::Rebuild() {
 Ownership=filn::Ownership(); Reservations=filn::Reservations();
 ByVehicle.Empty(); StationIds.Empty(); Active.Empty(); Cursor=0;
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
  if(IsValid(R.Home)) HomeActors.Add(R.Home);
  if(!R.TruckId.IsValid() || !R.BayId.IsValid() || !Ownership.Pair(Key(R.TruckId),Key(R.BayId))) bDispatchBlocked=true;
  if(IsValid(R.Vehicle)) {
   if(ByVehicle.Contains(R.Vehicle)) bDispatchBlocked=true;
   ByVehicle.Add(R.Vehicle,I);
  }
  if(StateOf(R)!=S::Idle) {
   SetState(R,S::Recovery);
   if(IsValid(R.Vehicle)) R.Vehicle->SetAutopilotEnabled(false);
   if(!R.JobId.IsValid() || !IsValid(R.Provider) || !IsValid(R.Requester) ||
      !Reservations.Acquire(Key(R.JobId),Key(R.TruckId),Key(StationId(R.Provider)),Key(StationId(R.Requester)))) bDispatchBlocked=true;
  }
 }
 if(bDispatchBlocked) UE_LOG(LogFILN,Error,TEXT("Dispatch locked: unsupported schema, missing endpoints or conflicting saved identities/reservations. Restore infrastructure/save; do not discard reservations."));
}
bool ALogisticsSubsystem::ValidHome(AFGDockingStationIdentifier* Home) const {
 auto* B=Building(Home);
 return IsValid(B) && B->GetIsInLoadMode() && B->GetInventory() && B->GetInventory()->IsEmpty();
}
bool ALogisticsSubsystem::Parked(const FFILNRegistration& R) const {
 auto* Home=Building(R.Home);
 return IsValid(R.Vehicle) && IsValid(R.Vehicle->GetOwnerVehicle()) && IsValid(Home) &&
  Home->GetDockedActor()==R.Vehicle->GetOwnerVehicle() && !R.Vehicle->IsAutopilotEnabled();
}
void ALogisticsSubsystem::Recover(FFILNRegistration& R,const FString& Why) {
 SetState(R,S::Recovery); R.Diagnostic=Why;
 if(IsValid(R.Vehicle)) R.Vehicle->SetAutopilotEnabled(false);
 UE_LOG(LogFILN,Warning,TEXT("job=%s RECOVERY: %s (reservations retained)"),*R.JobId.ToString(),*Why);
}
bool ALogisticsSubsystem::Pair(AFGWheeledVehicleIdentifier* V,AFGDockingStationIdentifier* H) {
 if(bDispatchBlocked || !IsValid(V) || !ValidHome(H) || !H->GetPathNodeGUID().IsValid()) return false;
 if(auto* Existing=ByVehicle.Find(V)) return Registrations[*Existing].Home==H;
 for(const auto& R:Registrations) if(R.Home==H || (StateOf(R)!=S::Idle && (R.Provider==H || R.Requester==H))) return false;
 FFILNRegistration R; R.Vehicle=V; R.Home=H;
 if(!Parked(R)) return false;
 R.TruckId=FGuid::NewGuid(); R.BayId=StationId(H);
 if(!Ownership.Pair(Key(R.TruckId),Key(R.BayId))) return false;
 ByVehicle.Add(V,Registrations.Add(R));
 UE_LOG(LogFILN,Log,TEXT("PAIRED truck=%s bay=%s"),*R.TruckId.ToString(),*R.BayId.ToString());
 return true;
}
bool ALogisticsSubsystem::Dispatch(FFILNRegistration& R,AFGDockingStationIdentifier* P,AFGDockingStationIdentifier* Q) {
 auto* PB=Building(P); auto* QB=Building(Q);
 if(bDispatchBlocked || StateOf(R)!=S::Idle || !Parked(R) || !ValidHome(R.Home) ||
  !IsValid(PB) || !IsValid(QB) || P==Q || P==R.Home || Q==R.Home || !PB->GetIsInLoadMode() || QB->GetIsInLoadMode()) return false;
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
 // Bounded round-robin across active jobs only. Recovery and idle records are not scanned.
 const int32 Budget=FMath::Min(16,Active.Num());
 for(int32 Count=0;Count<Budget && Active.Num()>0;++Count) {
  Cursor%=Active.Num(); const int32 Index=Active[Cursor]; auto& R=Registrations[Index];
  if(StateOf(R)==S::Recovery || StateOf(R)==S::Idle) { Active.RemoveAtSwap(Cursor); continue; }
  const double Now=GetWorld()->GetTimeSeconds();
  R.Elapsed+=static_cast<float>(FMath::Max(0.0,Now-R.LastObserved)); R.LastObserved=Now;
  auto* PB=Building(R.Provider); auto* QB=Building(R.Requester); auto* HB=Building(R.Home);
  if(!IsValid(R.Vehicle) || !IsValid(R.Vehicle->GetOwnerVehicle()) || !IsValid(PB) || !IsValid(QB) || !IsValid(HB)) {
   Recover(R,TEXT("Vehicle or station destroyed/unavailable")); ++Cursor; continue;
  }
  if(!ValidHome(R.Home) || !PB->GetIsInLoadMode() || QB->GetIsInLoadMode() || R.Vehicle->GetVehicleRoute()!=R.RouteIntent) {
   Recover(R,TEXT("Home cargo/mode, endpoint mode or route changed")); ++Cursor; continue;
  }
  if(R.Vehicle->GetAutopilotErrorStatus()!=EVehicleAutopilotErrorStatus::None || R.Elapsed>1800.f) {
   Recover(R,FString::Printf(TEXT("Autopilot error=%d or journey timeout"),static_cast<int32>(R.Vehicle->GetAutopilotErrorStatus()))); ++Cursor; continue;
  }
  auto* Owner=R.Vehicle->GetOwnerVehicle();
  const bool AtP=PB->GetDockedActor()==Owner, AtQ=QB->GetDockedActor()==Owner, AtH=HB->GetDockedActor()==Owner;
  const S Before=StateOf(R); S Next=Before;
  if(Before==S::ToProvider && AtP) Next=filn::Advance(Before,filn::Event::ProviderDock);
  else if(Before==S::Loading && !AtP) Next=filn::Advance(Before,filn::Event::ProviderLeave);
  else if(Before==S::ToRequester && AtQ) Next=filn::Advance(Before,filn::Event::RequesterDock);
  else if(Before==S::Unloading && !AtQ) Next=filn::Advance(Before,filn::Event::RequesterLeave);
  else if(Before==S::Returning && AtH) {
   R.Vehicle->SetAutopilotEnabled(false);
   auto* Cargo=Owner->GetStorageInventory();
   if(!Cargo || !Cargo->IsEmpty()) { Recover(R,TEXT("Returned with cargo: incomplete unload requires operator recovery")); ++Cursor; continue; }
   if(Parked(R)) Next=filn::Advance(Before,filn::Event::HomeStopped);
  }
  else if((Before==S::ToProvider && AtQ) || (Before==S::ToRequester && AtH) || (Before==S::Returning && AtP)) Next=S::Recovery;
  if(Next==S::Recovery) Recover(R,TEXT("Out-of-order docking observed; journey cannot be proven"));
  else if(Next!=Before) {
   SetState(R,Next);
   UE_LOG(LogFILN,Log,TEXT("job=%s state=%d; docking observation only, transfer unverified"),*R.JobId.ToString(),R.State);
   if(Next==S::Completed) {
    Reservations.Release(Key(R.JobId)); SetState(R,S::Idle); R.Diagnostic=TEXT("Home stopped; transfers not measured");
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
 if(Command==TEXT("status")) {
  for(const auto& R:Registrations) UE_LOG(LogFILN,Log,TEXT("truck=%s bay=%s job=%s state=%d diagnostic=%s"),*R.TruckId.ToString(),*R.BayId.ToString(),*R.JobId.ToString(),R.State,*R.Diagnostic);
  return;
 }
 AFGWheeledVehicleIdentifier* V=nullptr;
 if(Args.Num()>0) for(auto* Candidate:VS->GetAllVehicles()) if(IsValid(Candidate) && Candidate->GetPathName()==Args[0]) { V=Candidate; break; }
 auto FindStation=[VS](const FString& Path)->AFGDockingStationIdentifier* {
  for(auto* H:VS->GetDockingStations()) if(IsValid(H) && H->GetPathName()==Path) return H;
  return nullptr;
 };
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
