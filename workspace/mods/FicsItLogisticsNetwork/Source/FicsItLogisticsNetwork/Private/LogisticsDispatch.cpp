// SPDX-License-Identifier: GPL-3.0-only
#include "LogisticsSubsystem.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "WheeledVehicles/FGWheeledVehicleIdentifier.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "WheeledVehicles/FGVehiclePathNode.h"
#include "FGInventoryComponent.h"
#include "Resources/FGItemDescriptor.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "UObject/UnrealType.h"
namespace {
std::string Id(const FGuid& G) { return TCHAR_TO_UTF8(*G.ToString()); }
std::string Descriptor(TSubclassOf<UFGItemDescriptor> D) { return D?TCHAR_TO_UTF8(*D->GetPathName()):""; }
bool Inventory(UFGInventoryComponent* I,TSubclassOf<UFGItemDescriptor> Resource,int& Kinds,int& Quantity,int& Capacity,std::string* Cargo=nullptr) {
 if(!I || I->GetSizeLinear()>4096) return false;
 TArray<FInventoryStack> Stacks; I->GetInventoryStacks(Stacks);
 TSet<UClass*> Types; int64 Amount=0, Size=0;
 for(const auto& S:Stacks) if(S.NumItems>0) { Types.Add(S.Item.GetItemClass()); Amount+=S.NumItems; if(Cargo) *Cargo=Descriptor(S.Item.GetItemClass()); }
 for(int32 Slot=0;Slot<I->GetSizeLinear();++Slot) {
  auto Filter=I->GetAllowedItemOnIndex(Slot);
  if(!Filter || Filter==Resource) Size+=I->GetSlotSizeForItem(Slot,Resource);
 }
 if(Amount>MAX_int32 || Size>MAX_int32 || Size<0) return false;
 Kinds=Types.Num(); Quantity=static_cast<int>(Amount); Capacity=static_cast<int>(Size); return true;
}
bool TransportMedium(const UObject* Object,filn::dispatch::Medium& Out) {
 const auto* Property=Object?FindFProperty<FBoolProperty>(Object->GetClass(),TEXT("mIsFluidStorageInventory")):nullptr;
 if(!Property) return false;
 Out=Property->GetPropertyValue_InContainer(Object)?filn::dispatch::Medium::Fluid:filn::dispatch::Medium::Solid;
 return true;
}
}
void ALogisticsSubsystem::ResetScan() {
 bScanning=false; bCheckingRetained=true; Rankings.Empty(); ScanVehicle=0; ScanProvider=0; BestOffer.reset(); BestVehicle=INDEX_NONE; BestProvider=INDEX_NONE;
}
bool ALogisticsSubsystem::BuildOffer(int32 V,int32 Q,int32 P,filn::dispatch::Offer& O,bool QueryRoads,bool& Pending) {
 Pending=false;
 if(!HasAuthority() || bDispatchBlocked || !Registrations.IsValidIndex(V) || !StationConfigs.IsValidIndex(Q) || (P!=INDEX_NONE && !StationConfigs.IsValidIndex(P))) return false;
 auto& R=Registrations[V]; const auto& C=StationConfigs[Q];
 R.Diagnostic=TEXT("No eligible compatible offer for this requester");
 auto* Owner=IsValid(R.Vehicle)?R.Vehicle->GetOwnerVehicle():nullptr;
 auto* QB=IsValid(C.Station)?C.Station->GetStation():nullptr;
 auto* HB=IsValid(R.Home)?R.Home->GetStation():nullptr;
 if(!IsValid(Owner) || !IsValid(QB) || !IsValid(HB) || !C.Resource || !ValidHome(R.Home)) return false;
 if(!IsValid(Owner)) return false;
 if(Owner->GetDriver()) { R.Diagnostic=TEXT("Player-controlled vehicle excluded"); return false; }
 if(!R.Vehicle->IsAutopilotEnabled()) { R.Diagnostic=TEXT("Autopilot disabled; explicit player opt-in required"); return false; }
 if(!Owner->HasFuel()) { R.Diagnostic=TEXT("Native vehicle reports no fuel; awaiting supplied fuel at home"); return false; }
 if(QB->GetIsInLoadMode() || !QB->HasPower() || QB->IsProductionPaused()) { R.Diagnostic=TEXT("Requester mode/power/standby ineligible"); return false; }
 if(!QB->GetInventory() || !QB->GetInventory()->IsEmpty()) { R.Diagnostic=TEXT("Requester is not empty"); return false; }
 // Automatic scheduling never opts a truck back in after a player disables autopilot.
 // Existing vanilla valid route and a native home docking observation are prerequisites.
 if(!R.Vehicle->IsAutopilotEnabled() || Owner->GetDriver() || (HB->GetDockedActor()!=Owner && !R.Held) || FMath::Abs(Owner->GetForwardSpeed())>1.f ||
    !R.Vehicle->CanEnableAutopilot() || R.Vehicle->GetAutopilotErrorStatus()!=EVehicleAutopilotErrorStatus::None) return false;
 if(FVector::DistSquared(Owner->GetVehicleLocation(),HB->GetDockingPathNode()->GetActorLocation())>FMath::Square(500.f)) return false;
 if(R.Held && R.Vehicle->GetVehicleRoute()!=R.IdleRoute) return false;
 auto& Truck=O.vehicle; auto& Request=O.requester;
 Truck.cargoOriginKnown=R.CargoOriginKnown; Truck.cargoOriginStorage=R.CargoOriginStorage;
 Truck.id=Id(R.TruckId); Truck.home=Id(R.BayId); Truck.enrolled=true;
 Truck.homeReady=HB->HasPower() && !HB->IsProductionPaused(); Truck.idle=R.State==static_cast<uint8>(filn::State::Idle);
 Truck.autopilot=R.Vehicle->IsAutopilotEnabled(); Truck.driver=Owner->GetDriver()!=nullptr; Truck.fuelReady=Owner->HasFuel();
 if(!TransportMedium(Owner,Truck.medium)) return false;
 if(!Inventory(Owner->GetStorageInventory(),C.Resource,Truck.cargoKinds,Truck.cargoQuantity,Truck.cargoCapacity,&Truck.cargoResource)) return false;
 Request.id=Id(StationId(C.Station)); Request.label=TCHAR_TO_UTF8(*C.Station->GetStationName().ToString()); Request.resource=Descriptor(C.Resource);
 Request.explicitFilter=true; Request.loadMode=QB->GetIsInLoadMode(); Request.powered=QB->HasPower(); Request.paused=QB->IsProductionPaused();
 Request.occupied=QB->GetDockedActor()!=nullptr || Reservations.StationBusy(Request.id); if(!TransportMedium(QB,Request.medium)) return false;
 Request.depot=HomeStations.Contains(TWeakObjectPtr<AFGDockingStationIdentifier>(C.Station.Get()));
 if(!Inventory(QB->GetInventory(),C.Resource,Request.kinds,Request.quantity,Request.capacity)) return false;
 if(P!=INDEX_NONE) {
  const auto& PC=StationConfigs[P]; auto* PB=IsValid(PC.Station)?PC.Station->GetStation():nullptr;
  if(!IsValid(PB) || PC.Resource!=C.Resource) return false;
  filn::dispatch::Station Provider;
  Provider.id=Id(StationId(PC.Station)); Provider.label=TCHAR_TO_UTF8(*PC.Station->GetStationName().ToString()); Provider.resource=Descriptor(PC.Resource);
  Provider.loadMode=PB->GetIsInLoadMode(); Provider.powered=PB->HasPower(); Provider.paused=PB->IsProductionPaused();
  Provider.occupied=PB->GetDockedActor()!=nullptr || Reservations.StationBusy(Provider.id); if(!TransportMedium(PB,Provider.medium)) return false;
  Provider.depot=HomeStations.Contains(TWeakObjectPtr<AFGDockingStationIdentifier>(PC.Station.Get()));
  std::string Actual;
  if(!Inventory(PB->GetInventory(),C.Resource,Provider.kinds,Provider.quantity,Provider.capacity,&Actual) || Actual!=Request.resource) return false;
  O.provider=Provider;
 }
 // Cheap eligibility first; zero values here are local sentinels only, never route evidence.
 O.roads={0,0,0,0};
 if(!filn::dispatch::Evaluate(O)) return false;
 if(!QueryRoads) return true;
 auto* VS=AFGVehicleSubsystem::Get(GetWorld()); auto* Preset=Owner->GetVehiclePathPreset();
 auto Distance=[&](const FGuid& From,const FGuid& To,double& Cost) {
  if(!RoadCache.Distance(VS,From,To,Preset,Cost)) { Pending=true; R.Diagnostic=TEXT("Directed road calculation pending within work budget"); return false; }
  if(Cost<0) R.Diagnostic=TEXT("No usable directed road path, or graph exceeds configured safety bounds");
  return Cost>=0;
 };
 if(!Distance(C.Station->GetPathNodeGUID(),R.Home->GetPathNodeGUID(),O.roads.requesterToHome)) return false;
 if(P==INDEX_NONE) {
  if(!Distance(R.Home->GetPathNodeGUID(),C.Station->GetPathNodeGUID(),O.roads.homeToRequester)) return false;
 } else {
  auto* Provider=StationConfigs[P].Station.Get();
  if(!Distance(Provider->GetPathNodeGUID(),C.Station->GetPathNodeGUID(),O.roads.providerToRequester) ||
     !Distance(R.Home->GetPathNodeGUID(),Provider->GetPathNodeGUID(),O.roads.homeToProvider)) return false;
 }
 return filn::dispatch::Evaluate(O).has_value();
}
void ALogisticsSubsystem::Schedule() {
 check(IsInGameThread());
 if(!HasAuthority() || !bAutoDispatchEnabled || bDispatchBlocked || StationConfigs.IsEmpty() || Registrations.IsEmpty()) return;
 const double Now=GetWorld()->GetTimeSeconds();
 if(!bScanning) {
  if(Now<NextDispatch) return;
  RequestCursor%=StationConfigs.Num(); ScanRoadRevision=RoadCache.Invalidations; bScanning=true; ScanVehicle=0; ScanProvider=0;
 }
 auto Finish=[&]() {
  if(ScanRoadRevision!=RoadCache.Invalidations) { ResetScan(); return; }
  if(BestOffer) CommitOffer(BestVehicle,RequestCursor,BestProvider);
  ++RequestCursor;
  const bool Wrapped=RequestCursor>=StationConfigs.Num();
  ResetScan(); NextDispatch=Wrapped?Now+5.:Now;
 };
 const double Deadline=FPlatformTime::Seconds()+0.001;
 for(int32 Work=0;Work<8 && FPlatformTime::Seconds()<Deadline;++Work) {
  if(ScanVehicle>=Registrations.Num()) {
   if(bCheckingRetained && !BestOffer) { bCheckingRetained=false; ScanVehicle=0; ScanProvider=0; }
   else { Finish(); return; }
  }
  auto& R=Registrations[ScanVehicle];
  auto* Owner=IsValid(R.Vehicle)?R.Vehicle->GetOwnerVehicle():nullptr;
  auto* Home=IsValid(R.Home)?R.Home->GetStation():nullptr;
  if(!IsValid(Owner) || !IsValid(Home) || R.State!=static_cast<uint8>(filn::State::Idle) ||
     !R.Vehicle->IsAutopilotEnabled() || Owner->GetDriver() || (Home->GetDockedActor()!=Owner && !R.Held)) { ++ScanVehicle; ScanProvider=0; continue; }
  int32 Provider=INDEX_NONE;
  if(!bCheckingRetained) {
   auto* Preset=Owner->GetVehiclePathPreset();
   if(!Preset) { ++ScanVehicle; continue; }
   auto& Rank=Rankings.FindOrAdd(Preset);
   if(!Rank.Sorted) {
    if(Rank.Cursor<StationConfigs.Num()) {
     const auto& C=StationConfigs[Rank.Cursor]; const auto& Q=StationConfigs[RequestCursor];
     if(IsValid(C.Station) && C.Resource==Q.Resource && IsValid(Q.Station)) {
      double Cost=-1;
      if(!RoadCache.Distance(AFGVehicleSubsystem::Get(GetWorld()),C.Station->GetPathNodeGUID(),Q.Station->GetPathNodeGUID(),Preset,Cost)) return;
      if(Cost>=0) Rank.Entries.Emplace(Cost,Rank.Cursor);
     }
     ++Rank.Cursor; continue;
    }
    Rank.Entries.Sort([](const auto& A,const auto& B){return A.Key==B.Key?A.Value<B.Value:A.Key<B.Key;}); Rank.Sorted=true;
   }
   if(!Rank.Entries.IsValidIndex(ScanProvider)) { ++ScanVehicle; ScanProvider=0; continue; }
   Provider=Rank.Entries[ScanProvider].Value;
  }
  filn::dispatch::Offer Offer; bool Pending=false;
  if(BuildOffer(ScanVehicle,RequestCursor,Provider,Offer,true,Pending)) {
   auto Choice=filn::dispatch::Evaluate(Offer);
   if(Choice && (!BestOffer || filn::dispatch::Better(*Choice,*BestOffer))) { BestOffer=Choice; BestVehicle=ScanVehicle; BestProvider=Provider; }
   // First eligible provider in this preset's road ordering is sufficient for this
   // truck. Depot proximity never overrides nearest-provider-to-requester ordering.
   ++ScanVehicle; ScanProvider=0;
  } else if(Pending) return;
  else if(bCheckingRetained) ++ScanVehicle;
  else ++ScanProvider;
 }
}
bool ALogisticsSubsystem::CommitOffer(int32 V,int32 Q,int32 P) {
 filn::dispatch::Offer Offer; bool Pending=false;
 if(!BuildOffer(V,Q,P,Offer,true,Pending)) return false;
 if(ScanRoadRevision!=RoadCache.Invalidations) return false;
 const auto Choice=filn::dispatch::Evaluate(Offer); if(!Choice) return false;
 auto& R=Registrations[V]; auto* Requester=StationConfigs[Q].Station.Get();
 auto* Provider=P==INDEX_NONE?nullptr:StationConfigs[P].Station.Get();
 const FGuid Job=FGuid::NewGuid();
 const bool Acquired=Provider?Reservations.Acquire(Id(Job),Id(R.TruckId),Id(StationId(Provider)),Id(StationId(Requester))):
  Reservations.AcquireDelivery(Id(Job),Id(R.TruckId),Id(StationId(Requester)));
 if(!Acquired) return false;
 R.Held=false; R.IdleRoute.Empty();
 R.JobId=Job; R.Provider=Provider; R.Requester=Requester; R.RetainedCargo=Provider==nullptr; R.Automatic=true;
 R.Resource=StationConfigs[Q].Resource; R.ExpectedCargo=Choice->amount; R.LastCargo=Offer.vehicle.cargoQuantity; R.LoadedCargo=0; R.DeliveredCargo=0;
 R.State=static_cast<uint8>(filn::State::Reserved); R.Elapsed=0; R.LastObserved=GetWorld()->GetTimeSeconds();
 // Home is an explicit first waypoint so native target-index reset can be read back.
 R.RouteIntent={R.Home->GetPathNodeGUID()};
 if(Provider) R.RouteIntent.Add(Provider->GetPathNodeGUID());
 R.RouteIntent.Add(Requester->GetPathNodeGUID()); R.RouteIntent.Add(R.Home->GetPathNodeGUID());
 R.Vehicle->SetVehicleRoute(R.RouteIntent);
 R.Vehicle->SetCurrentTargetWaypoint(1);
 if(R.Vehicle->GetVehicleRoute()!=R.RouteIntent || R.Vehicle->GetCurrentTargetWaypointIndex()!=1 || !R.Vehicle->IsAutopilotEnabled()) {
  Recover(R,TEXT("Native route/target/autopilot readback failed; reservations retained")); return false;
 }
 R.State=static_cast<uint8>(Provider?filn::State::ToProvider:filn::State::ToRequester);
 Active.AddUnique(V); R.Diagnostic=TEXT("Native automatic route committed; awaiting measured transfer");
 UE_LOG(LogFILN,Log,TEXT("AUTO job=%s retained=%d resource=%s amount=%d road=%f"),*Job.ToString(),R.RetainedCargo,*R.Resource->GetPathName(),R.ExpectedCargo,Choice->roadDistance);
 return true;
}
