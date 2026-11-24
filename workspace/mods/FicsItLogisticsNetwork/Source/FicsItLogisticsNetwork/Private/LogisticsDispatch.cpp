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
 if(!IsValid(I) || !Resource || I->GetSizeLinear()<=0 || I->GetSizeLinear()>4096) return false;
 TArray<FInventoryStack> Stacks; I->GetInventoryStacks(Stacks);
 TSet<UClass*> Types; int64 Amount=0, Size=0;
 if(Cargo) Cargo->clear();
 for(const auto& S:Stacks) {
  if(S.NumItems<0 || (S.NumItems>0 && !S.Item.GetItemClass())) return false;
  if(S.NumItems>0) { Types.Add(S.Item.GetItemClass()); Amount+=S.NumItems; if(Cargo) *Cargo=Descriptor(S.Item.GetItemClass()); }
 }
 for(int32 Slot=0;Slot<I->GetSizeLinear();++Slot) {
  auto Filter=I->GetAllowedItemOnIndex(Slot);
  if(!Filter || Filter==Resource) { const auto Capacity=I->GetSlotSizeForItem(Slot,Resource); if(Capacity<0) return false; Size+=Capacity; }
 }
 if(Amount>MAX_int32 || Size>MAX_int32 || Size<0) return false;
 Kinds=Types.Num(); Quantity=static_cast<int>(Amount); Capacity=static_cast<int>(Size); return true;
}
bool ExactFilter(UFGInventoryComponent* I,TSubclassOf<UFGItemDescriptor> Resource) {
 if(!IsValid(I) || !Resource || I->GetSizeLinear()<=0 || I->GetSizeLinear()>4096) return false;
 for(int32 Slot=0;Slot<I->GetSizeLinear();++Slot) if(I->GetAllowedItemOnIndex(Slot)!=Resource) return false;
 return true;
}
bool TransportMedium(const UObject* Object,filn::dispatch::Medium& Out) {
 const auto* Property=Object?FindFProperty<FBoolProperty>(Object->GetClass(),TEXT("mIsFluidStorageInventory")):nullptr;
 if(!Property) return false;
 Out=Property->GetPropertyValue_InContainer(Object)?filn::dispatch::Medium::Fluid:filn::dispatch::Medium::Solid;
 return true;
}
}
void ALogisticsSubsystem::ResetScan() {
 bScanning=false; bCheckingRetained=true; Rankings.Empty(); ProviderCandidates.Empty(); ScanVehicle=0; ScanProvider=0; FirstEligibleProviderDistance=-1; BestOffer.reset(); BestVehicle=INDEX_NONE; BestProvider=INDEX_NONE;
}
bool ALogisticsSubsystem::BuildOffer(int32 V,int32 Q,int32 P,filn::dispatch::Offer& O,bool QueryRoads,bool& Pending) {
 Pending=false; O={};
 if(!HasAuthority() || !bRegistryValidated || bDispatchBlocked || bPreparingUninstall || bChangingFilters || !Registrations.IsValidIndex(V) || !StationConfigs.IsValidIndex(Q) || (P!=INDEX_NONE && !ProviderCandidates.IsValidIndex(P))) return false;
 auto& R=Registrations[V]; const auto& C=StationConfigs[Q];
 if(R.State!=static_cast<uint8>(filn::State::Idle) || !R.Held || R.JobId.IsValid() || !WatchTransfers(R)) return false;
 R.Diagnostic=TEXT("No eligible compatible offer for this requester");
 auto* Owner=IsValid(R.Vehicle)?R.Vehicle->GetOwnerVehicle():nullptr;
 auto* QB=IsValid(C.Station)?C.Station->GetStation():nullptr;
 auto* HB=IsValid(R.Home)?R.Home->GetStation():nullptr;
 if(!IsValid(Owner) || !IsValid(QB) || !IsValid(HB) || !C.Resource || !ValidHome(R.Home)) return false;
 if(Owner->GetDriver()) { R.Diagnostic=TEXT("Player-controlled vehicle excluded"); return false; }
 if(!R.Vehicle->IsAutopilotEnabled()) { R.Diagnostic=TEXT("Autopilot disabled; explicit player opt-in required"); return false; }
 if(!Owner->HasFuel() || !FuelReady(R)) { R.Diagnostic=TEXT("Awaiting full compatible native fuel inventory at assigned home"); return false; }
 if(QB->GetIsInLoadMode() || !QB->HasPower() || QB->IsProductionPaused()) { R.Diagnostic=TEXT("Requester mode/power/standby ineligible"); return false; }
 if(!QB->GetInventory() || !QB->GetInventory()->IsEmpty()) { R.Diagnostic=TEXT("Requester is not empty"); return false; }
 if(!ExactFilter(QB->GetInventory(),C.Resource)) { R.Diagnostic=TEXT("Requester native filter no longer matches FILN resource"); return false; }
 // Automatic scheduling never opts a truck back in after a player disables autopilot.
 // Existing vanilla valid route and a native home docking observation are prerequisites.
 const auto Error=R.Vehicle->GetAutopilotErrorStatus();
 if(!R.Vehicle->IsAutopilotEnabled() || Owner->GetDriver() ||
    !filn::recovery::IdleRouteFaultAllowed(Error==EVehicleAutopilotErrorStatus::None,Error==EVehicleAutopilotErrorStatus::TooFewStations) ||
    !HB->GetDockingPathNode()) return false;
 const FVector Delta=Owner->GetVehicleLocation()-HB->GetDockingPathNode()->GetActorLocation();
 const TArray<FGuid> HomeRoute={R.Home->GetPathNodeGUID()};
 if(!HomeRoute[0].IsValid() || R.IdleRoute!=HomeRoute || R.Vehicle->GetVehicleRoute()!=HomeRoute ||
    !filn::dispatch::HomePositionReady(Delta.SizeSquared(),Delta.Z,Owner->GetForwardSpeed(),HB->GetDockedActor() && HB->GetDockedActor()!=Owner)) return false;
 auto& Truck=O.vehicle; auto& Request=O.requester;
 Truck.cargoOriginKnown=R.CargoOriginKnown; Truck.cargoOriginStorage=R.CargoOriginStorage;
 Truck.id=Id(R.TruckId); Truck.home=Id(R.BayId); Truck.enrolled=true;
 Truck.homeReady=HB->HasPower() && !HB->IsProductionPaused(); Truck.idle=R.State==static_cast<uint8>(filn::State::Idle);
 Truck.autopilot=R.Vehicle->IsAutopilotEnabled(); Truck.driver=Owner->GetDriver()!=nullptr; Truck.fuelReady=true; // checked once above
 if(!TransportMedium(Owner,Truck.medium)) return false;
 const auto Form=UFGItemDescriptor::GetForm(C.Resource);
 const bool Fluid=Form==EResourceForm::RF_LIQUID || Form==EResourceForm::RF_GAS;
 if((!Fluid && Form!=EResourceForm::RF_SOLID) || Fluid!=(Truck.medium==filn::dispatch::Medium::Fluid)) return false;
 if(Fluid && QB->GetCurrentFluidDescriptor() && QB->GetCurrentFluidDescriptor()!=C.Resource) return false;
 if(!Inventory(Owner->GetStorageInventory(),C.Resource,Truck.cargoKinds,Truck.cargoQuantity,Truck.cargoCapacity,&Truck.cargoResource)) return false;
 Request.id=Id(StationId(C.Station)); Request.label=TCHAR_TO_UTF8(*C.Station->GetStationName().ToString()); Request.resource=Descriptor(C.Resource);
 Request.explicitFilter=true; Request.loadMode=QB->GetIsInLoadMode(); Request.powered=QB->HasPower(); Request.paused=QB->IsProductionPaused();
 Request.occupied=QB->GetDockedActor()!=nullptr || Reservations.StationBusy(Request.id); if(!TransportMedium(QB,Request.medium)) return false;
 Request.depot=IsDepot(C.Station);
 if(!Inventory(QB->GetInventory(),C.Resource,Request.kinds,Request.quantity,Request.capacity)) return false;
 if(P!=INDEX_NONE) {
  auto* ID=ProviderCandidates[P].Get(); auto* PB=IsValid(ID)?ID->GetStation():nullptr;
  if(!IsValid(PB) || ID==C.Station || IsDepot(ID) || !PB->GetIsInLoadMode()) return false;
  const auto* PC=StationConfigs.FindByPredicate([ID](const FFILNStationConfig& Entry){return Entry.Station==ID;});
  if(PC && (PC->Resource!=C.Resource || !ExactFilter(PB->GetInventory(),C.Resource))) return false;
  if(Fluid && PB->GetCurrentFluidDescriptor() && PB->GetCurrentFluidDescriptor()!=C.Resource) return false;
  filn::dispatch::Station Provider;
  Provider.id=Id(StationId(ID)); Provider.label=TCHAR_TO_UTF8(*ID->GetStationName().ToString()); Provider.explicitFilter=PC!=nullptr;
  Provider.loadMode=PB->GetIsInLoadMode(); Provider.powered=PB->HasPower(); Provider.paused=PB->IsProductionPaused();
  Provider.occupied=PB->GetDockedActor()!=nullptr || Reservations.StationBusy(Provider.id); if(!TransportMedium(PB,Provider.medium)) return false;
  Provider.depot=IsDepot(ID);
  std::string Actual;
  if(!Inventory(PB->GetInventory(),C.Resource,Provider.kinds,Provider.quantity,Provider.capacity,&Actual) ||
     !filn::dispatch::ProviderResourceMatches(Request.resource,Actual,Provider.kinds,PC?Descriptor(PC->Resource):std::string())) return false;
  Provider.resource=Actual;
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
  auto* Provider=ProviderCandidates[P].Get();
  if(!Distance(Provider->GetPathNodeGUID(),C.Station->GetPathNodeGUID(),O.roads.providerToRequester) ||
     !Distance(R.Home->GetPathNodeGUID(),Provider->GetPathNodeGUID(),O.roads.homeToProvider)) return false;
 }
 return filn::dispatch::Evaluate(O).has_value();
}
bool ALogisticsSubsystem::EndpointConfigurationValid(const FFILNRegistration& R) const {
 auto* Q=IsValid(R.Requester)?R.Requester->GetStation():nullptr;
 if(!IsValid(Q) || Q->GetIsInLoadMode() || IsDepot(R.Requester) || !R.Resource || !ExactFilter(Q->GetInventory(),R.Resource)) return false;
 const auto* QC=StationConfigs.FindByPredicate([&](const FFILNStationConfig& C){return C.Station==R.Requester;});
 if(!QC || QC->Resource!=R.Resource) return false;
 if(R.RetainedCargo) return !filn::dispatch::IsStorage(TCHAR_TO_UTF8(*R.Requester->GetStationName().ToString())) ||
  (R.CargoOriginKnown && !R.CargoOriginStorage) || (R.DeliveredCargo>0 && R.LastCargo==0);
 auto* P=IsValid(R.Provider)?R.Provider->GetStation():nullptr;
 if(!IsValid(P) || !P->GetIsInLoadMode() || IsDepot(R.Provider)) return false;
 const auto* PC=StationConfigs.FindByPredicate([&](const FFILNStationConfig& C){return C.Station==R.Provider;});
 return (!PC || (PC->Resource==R.Resource && ExactFilter(P->GetInventory(),R.Resource))) &&
  filn::dispatch::PairAllowed(TCHAR_TO_UTF8(*R.Provider->GetStationName().ToString()),TCHAR_TO_UTF8(*R.Requester->GetStationName().ToString()));
}
void ALogisticsSubsystem::Schedule() {
 check(IsInGameThread());
 if(!HasAuthority() || !bRegistryValidated || !bAutoDispatchEnabled || bDispatchBlocked || bChangingFilters || bPreparingUninstall || StationConfigs.IsEmpty() || Registrations.IsEmpty()) return;
 const double Now=GetWorld()->GetTimeSeconds();
 if(!bScanning) {
  if(Now<NextDispatch) return;
  RequestCursor%=StationConfigs.Num();
  const auto& Q=StationConfigs[RequestCursor]; auto* QB=IsValid(Q.Station)?Q.Station->GetStation():nullptr;
  if(!IsValid(QB) || QB->GetIsInLoadMode() || IsDepot(Q.Station) || !Q.Resource || !QB->GetInventory() || !QB->GetInventory()->IsEmpty()) {
   ++RequestCursor; if(RequestCursor>=StationConfigs.Num()) {RequestCursor=0; NextDispatch=Now+5.;} return;
  }
  auto* VS=AFGVehicleSubsystem::Get(GetWorld()); if(!VS) return;
  ProviderCandidates.Reset();
  // Native roster enumeration once per requester scan, never per offer.
  // Eligibility and inventory are read lazily under the existing work budget.
  for(auto* ID:VS->GetDockingStations()) if(IsValid(ID)) ProviderCandidates.Add(ID);
  ScanRoadRevision=RoadCache.Invalidations; bScanning=true; ScanVehicle=0; ScanProvider=0; FirstEligibleProviderDistance=-1;
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
   if(bCheckingRetained && !BestOffer) { bCheckingRetained=false; ScanVehicle=0; ScanProvider=0; FirstEligibleProviderDistance=-1; }
   else { Finish(); return; }
  }
  auto& R=Registrations[ScanVehicle];
  auto* Owner=IsValid(R.Vehicle)?R.Vehicle->GetOwnerVehicle():nullptr;
  auto* Home=IsValid(R.Home)?R.Home->GetStation():nullptr;
  if(!IsValid(Owner) || !IsValid(Home) || R.State!=static_cast<uint8>(filn::State::Idle) ||
     !R.Vehicle->IsAutopilotEnabled() || Owner->GetDriver() || !R.Held || R.JobId.IsValid() || (Home->GetDockedActor() && Home->GetDockedActor()!=Owner) || !Owner->HasFuel()) { ++ScanVehicle; ScanProvider=0; FirstEligibleProviderDistance=-1; continue; }
  int32 Provider=INDEX_NONE;
  if(!bCheckingRetained) {
   auto* Preset=Owner->GetVehiclePathPreset();
   if(!Preset || !Owner->GetStorageInventory() || !Owner->GetStorageInventory()->IsEmpty()) { ++ScanVehicle; ScanProvider=0; FirstEligibleProviderDistance=-1; continue; }
   auto& Rank=Rankings.FindOrAdd(Preset);
   if(!Rank.Sorted) {
    if(Rank.Cursor<ProviderCandidates.Num()) {
     auto* ID=ProviderCandidates[Rank.Cursor].Get(); const auto& Q=StationConfigs[RequestCursor];
     auto* B=IsValid(ID)?ID->GetStation():nullptr;
     if(IsValid(B) && B->GetIsInLoadMode() && ID!=Q.Station && !IsDepot(ID) && IsValid(Q.Station) && Q.Resource &&
        B->HasPower() && !B->IsProductionPaused() && !B->GetDockedActor() && IsValid(B->GetInventory())) {
      // Ignore irrelevant/mixed/empty cargo before scheduling expensive graph
      // work. Stock is not a road cache key; every new scan resamples it.
      int Kinds=0,Quantity=0,Capacity=0; std::string Actual;
      const auto* PC=StationConfigs.FindByPredicate([ID](const FFILNStationConfig& C){return C.Station==ID;});
      if((PC && PC->Resource!=Q.Resource) || !Inventory(B->GetInventory(),Q.Resource,Kinds,Quantity,Capacity,&Actual) ||
         !filn::dispatch::ProviderResourceMatches(Descriptor(Q.Resource),Actual,Kinds,PC?Descriptor(PC->Resource):std::string()) ||
         !filn::dispatch::PairAllowed(TCHAR_TO_UTF8(*ID->GetStationName().ToString()),TCHAR_TO_UTF8(*Q.Station->GetStationName().ToString()))) { ++Rank.Cursor; continue; }
      double Cost=-1;
      if(!RoadCache.Distance(AFGVehicleSubsystem::Get(GetWorld()),ID->GetPathNodeGUID(),Q.Station->GetPathNodeGUID(),Preset,Cost)) return;
      if(Cost>=0) Rank.Entries.Emplace(Cost,Rank.Cursor);
     }
     ++Rank.Cursor; continue;
    }
    Rank.Entries.Sort([](const auto& A,const auto& B){return A.Key==B.Key?A.Value<B.Value:A.Key<B.Key;}); Rank.Sorted=true;
   }
   if(!Rank.Entries.IsValidIndex(ScanProvider) || !filn::dispatch::WithinNearestGroup(FirstEligibleProviderDistance,Rank.Entries[ScanProvider].Key)) { ++ScanVehicle; ScanProvider=0; FirstEligibleProviderDistance=-1; continue; }
   Provider=Rank.Entries[ScanProvider].Value;
  }
  filn::dispatch::Offer Offer; bool Pending=false;
  if(BuildOffer(ScanVehicle,RequestCursor,Provider,Offer,true,Pending)) {
   auto Choice=filn::dispatch::Evaluate(Offer);
   if(Choice && (!BestOffer || filn::dispatch::Better(*Choice,*BestOffer))) { BestOffer=Choice; BestVehicle=ScanVehicle; BestProvider=Provider; }
   // Equal provider-to-requester distances must be compared by empty travel.
   // Never discard a cheaper empty leg merely because of native roster order.
   if(!bCheckingRetained && Choice) { FirstEligibleProviderDistance=Choice->providerDistance; ++ScanProvider; }
   else { ++ScanVehicle; ScanProvider=0; FirstEligibleProviderDistance=-1; }
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
 auto* Provider=P==INDEX_NONE?nullptr:ProviderCandidates[P].Get();
 const FGuid Job=FGuid::NewGuid();
 const bool Acquired=Provider?Reservations.Acquire(Id(Job),Id(R.TruckId),Id(StationId(Provider)),Id(StationId(Requester))):
  Reservations.AcquireDelivery(Id(Job),Id(R.TruckId),Id(StationId(Requester)));
 if(!Acquired) return false;
 R.Held=false; R.IdleRoute.Empty();
 R.JobId=Job; R.Provider=Provider; R.Requester=Requester; R.AbortReturn=false; R.DepartedHome=false;
 R.ProviderLease=Provider?StationId(Provider):FGuid(); R.RequesterLease=StationId(Requester); R.RetainedCargo=Provider==nullptr; R.Automatic=true;
 R.Resource=StationConfigs[Q].Resource; R.ExpectedCargo=Choice->amount; R.LastCargo=Offer.vehicle.cargoQuantity; R.LoadedCargo=0; R.DeliveredCargo=0;
 R.State=static_cast<uint8>(filn::State::Reserved); if(Provider) { R.CargoOriginKnown=false; R.CargoOriginStorage=false; } R.Elapsed=0; R.LastObserved=GetWorld()->GetTimeSeconds(); R.NavigationRetry.Reset(); R.TravelProgress.Reset();
 // Home is an explicit first waypoint so native target-index reset can be read back.
 R.RouteIntent={R.Home->GetPathNodeGUID()};
 if(Provider) R.RouteIntent.Add(Provider->GetPathNodeGUID());
 R.RouteIntent.Add(Requester->GetPathNodeGUID()); R.RouteIntent.Add(R.Home->GetPathNodeGUID());
 const auto CanContinue=[&]() {
  return R.State==static_cast<uint8>(filn::State::Reserved) && IsValid(R.Vehicle) && IsValid(R.Vehicle->GetOwnerVehicle()) &&
   !R.Vehicle->GetOwnerVehicle()->GetDriver() && R.Vehicle->IsAutopilotEnabled() &&
   R.Vehicle->GetVehicleRoute()==R.RouteIntent && Reservations.OwnsJob(Id(R.JobId),Id(R.TruckId));
 };
 R.Vehicle->SetVehicleRoute(R.RouteIntent);
 if(!CanContinue()) { Recover(R,TEXT("Native route write interrupted; reservations retained")); return false; }
 R.Vehicle->SetCurrentTargetWaypoint(1);
 if(!CanContinue() || R.Vehicle->GetCurrentTargetWaypointIndex()!=1) {
  Recover(R,TEXT("Native route/target/autopilot readback failed; reservations retained")); return false;
 }
 R.State=static_cast<uint8>(Provider?filn::State::ToProvider:filn::State::ToRequester);
 R.RouteActivationDeadline=GetWorld()->GetTimeSeconds()+5.;
 Active.AddUnique(V); R.Diagnostic=TEXT("Native automatic route committed; awaiting measured transfer");
 UE_LOG(LogFILN,Log,TEXT("AUTO job=%s retained=%d resource=%s amount=%d road=%f"),*Job.ToString(),R.RetainedCargo,*R.Resource->GetPathName(),R.ExpectedCargo,Choice->roadDistance);
 return true;
}
