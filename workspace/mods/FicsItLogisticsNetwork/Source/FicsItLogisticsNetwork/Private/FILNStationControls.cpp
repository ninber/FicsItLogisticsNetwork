// SPDX-License-Identifier: GPL-3.0-only
#include "FILNStationControls.h"
#include "LogisticsSubsystem.h"
#include "FGHUD.h"
#include "FGPlayerController.h"
#include "FGCharacterPlayer.h"
#include "FGRecipeManager.h"
#include "FGInventoryComponent.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "Resources/FGItemDescriptor.h"
#include "Registry/RemoteCallObjectRegistry.h"
#include "Subsystem/SubsystemActorManager.h"
#include "UI/FGInteractWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "UObject/UnrealType.h"
#include "Net/UnrealNetwork.h"
#include "Patching/NativeHookManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameMode.h"
#include "FGGameSession.h"
#include "WheeledVehicles/FGWheeledVehicleIdentifier.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "WheeledVehicles/FGVehicleSubsystem.h"

namespace {
FText StateLabel(uint8 State) {
 switch(static_cast<filn::State>(State)) {
  case filn::State::Idle: return NSLOCTEXT("FILN","StateIdle","Idle");
  case filn::State::Reserved: return NSLOCTEXT("FILN","StateReserved","Reserved");
  case filn::State::ToProvider: return NSLOCTEXT("FILN","StateToProvider","To provider");
  case filn::State::Loading: return NSLOCTEXT("FILN","StateLoading","Loading");
  case filn::State::ToRequester: return NSLOCTEXT("FILN","StateToRequester","To requester");
  case filn::State::Unloading: return NSLOCTEXT("FILN","StateUnloading","Unloading");
  case filn::State::Returning: return NSLOCTEXT("FILN","StateReturning","Returning");
  case filn::State::Completed: return NSLOCTEXT("FILN","StateCompleted","Completed");
  case filn::State::Recovery: return NSLOCTEXT("FILN","StateRecovery","Recovery required");
 }
 return NSLOCTEXT("FILN","StateUnknown","Unknown state");
}
FText YesNo(bool Value) {
 return Value?NSLOCTEXT("FILN","Yes","Yes"):NSLOCTEXT("FILN","No","No");
}
FText Enabled(bool Value) {
 return Value?NSLOCTEXT("FILN","Enabled","Enabled"):NSLOCTEXT("FILN","Disabled","Disabled");
}
FText JoinLines(const TArray<FText>& Lines) {
 return FText::Join(FText::FromString(TEXT("\n")),Lines);
}
ALogisticsSubsystem* Subsystem(UWorld* World) {
 auto* Manager=World?World->GetSubsystem<USubsystemActorManager>():nullptr;
 return Manager?Manager->GetSubsystemActor<ALogisticsSubsystem>():nullptr;
}
bool Compatible(TSubclassOf<UFGItemDescriptor> Resource,AFGBuildableDockingStation* Station) {
 if(!Resource || !Station) return false;
 const auto Form=UFGItemDescriptor::GetForm(Resource);
 const auto* Property=FindFProperty<FBoolProperty>(Station->GetClass(),TEXT("mIsFluidStorageInventory"));
 if(!Property) return false;
 const bool Fluid=Property->GetPropertyValue_InContainer(Station);
 return Fluid?(Form==EResourceForm::RF_LIQUID || Form==EResourceForm::RF_GAS):Form==EResourceForm::RF_SOLID;
}
}
void UFILNUIRegistration::Initialize(FSubsystemCollectionBase& Collection) {
 Super::Initialize(Collection);
 Collection.InitializeDependency<URemoteCallObjectRegistry>();
 GetGameInstance()->GetSubsystem<URemoteCallObjectRegistry>()->RegisterRemoteCallObject(UFILNRemoteControl::StaticClass());
}
void UFILNRemoteControl::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
 Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(UFILNRemoteControl,NetReady);
}
bool UFILNRemoteControl::ValidateContext(AFGBuildableDockingStation* Target) {
 auto* World=GetWorld(); auto* Player=GetOwnerPlayerCharacter();
 if(!World || !IsValid(Player) || !IsValid(Target) || Target->GetWorld()!=World || !Target->HasAuthority() ||
    FVector::DistSquared(Player->GetActorLocation(),Target->GetActorLocation())>FMath::Square(2000.f)) return false;
 const double Now=World->GetTimeSeconds(); if(Now-LastRequest<0.2) return false;
 LastRequest=Now; return true;
}
void UFILNRemoteControl::QueryStation_Implementation(AFGBuildableDockingStation* Target) {
 if(!ValidateContext(Target)) return;
 auto* S=Subsystem(GetWorld()); if(!S) return;
 const auto* C=S->StationConfigs.FindByPredicate([Target](const FFILNStationConfig& Entry){return Entry.Station && Entry.Station->GetStation()==Target;});
 TArray<FText> Lines;
 Lines.Add(S->IsDepot(Target->GetStationIdentifier())?NSLOCTEXT("FILN","DepotRole","FILN rest stop — fuel only"):C?NSLOCTEXT("FILN","Enrolled","FILN station enrolled"):NSLOCTEXT("FILN","SelectResource","Choose a resource and register this station with FILN"));
 if(C && S->Reservations.StationBusy(TCHAR_TO_UTF8(*S->StationId(C->Station).ToString()))) Lines.Add(NSLOCTEXT("FILN","ReservedLocked","Active logistics reservation; resource changes locked"));
 Lines.Add(FText::Format(NSLOCTEXT("FILN","StationSummary","Dispatcher: {0} | Registered vehicles: {1} | Active deliveries: {2}"),
  Enabled(S->bAutoDispatchEnabled),FText::AsNumber(S->Registrations.Num()),FText::AsNumber(S->Active.Num())));
 for(const auto& R:S->Registrations) if(R.Home==Target->GetStationIdentifier() || R.Provider==Target->GetStationIdentifier() || R.Requester==Target->GetStationIdentifier()) {
  Lines.Add(FText::Format(NSLOCTEXT("FILN","RelatedVehicle","Vehicle {0} — {1}: {2}"),FText::FromString(R.TruckId.ToString()),StateLabel(R.State),FText::FromString(R.Diagnostic)));
 }
 const FText Message=JoinLines(Lines);
 StationReply(Target,C!=nullptr,C?C->Resource:TSubclassOf<UFGItemDescriptor>(),Message);
}
void UFILNRemoteControl::Configure_Implementation(AFGBuildableDockingStation* Target,TSubclassOf<UFGItemDescriptor> Resource,bool Remove) {
 if(!ValidateContext(Target)) return;
 auto* S=Subsystem(GetWorld()); auto* Recipes=AFGRecipeManager::Get(GetWorld());
 if(!S) return;
 bool Accepted=false;
 if(Remove) Accepted=S->RemoveStation(Target->GetStationIdentifier());
 else if(Recipes && Recipes->IsItemDescriptorAvailable(Resource) && Compatible(Resource,Target)) Accepted=S->ConfigureStation(Target->GetStationIdentifier(),Resource);
 const auto* C=S->StationConfigs.FindByPredicate([Target](const FFILNStationConfig& Entry){return Entry.Station && Entry.Station->GetStation()==Target;});
 StationReply(Target,C!=nullptr,C?C->Resource:TSubclassOf<UFGItemDescriptor>(),Accepted?NSLOCTEXT("FILN","StationUpdated","FILN station updated"):NSLOCTEXT("FILN","StationRejected","Rejected: station must be empty, undocked, unreserved and compatible; existing filters must agree"));
}
void UFILNRemoteControl::NetworkOverview_Implementation(AFGBuildableDockingStation* Target,int32 Page) {
 if(!ValidateContext(Target) || Page<0) return;
 auto* S=Subsystem(GetWorld()); if(!S) return;
 constexpr int32 PageSize=32;
 TArray<AFGDockingStationIdentifier*> Unclaimed;
 for(const auto& D:S->DepotStations) if(IsValid(D) && !S->HomeStations.Contains(TWeakObjectPtr<AFGDockingStationIdentifier>(D.Get()))) Unclaimed.Add(D.Get());
 const int32 Total=S->StationConfigs.Num()+S->Registrations.Num()+Unclaimed.Num();
 const int32 Pages=FMath::Max(1,FMath::DivideAndRoundUp(Total,PageSize));
 Page=FMath::Clamp(Page,0,Pages-1);
 TArray<FText> Lines;
 Lines.Add(FText::Format(NSLOCTEXT("FILN","RoadSummary","Registry valid: {0} | Dispatch blocked: {1} | Road cache hits: {2} | Misses: {3} | Invalidations: {4}"),YesNo(S->bRegistryValidated),YesNo(S->bDispatchBlocked),FText::AsNumber(S->RoadCache.Hits),FText::AsNumber(S->RoadCache.Misses),FText::AsNumber(S->RoadCache.Invalidations)));
 Lines.Add(FText::Format(NSLOCTEXT("FILN","NetworkSummary","Stations: {0} | Vehicles/depots: {1} | Active deliveries: {2} | Reservations: {3} | Dispatcher: {4}"),
  FText::AsNumber(S->StationConfigs.Num()),FText::AsNumber(S->Registrations.Num()),FText::AsNumber(S->Active.Num()),FText::AsNumber(static_cast<int64>(S->Reservations.Size())),Enabled(S->bAutoDispatchEnabled)));
 for(int32 Row=Page*PageSize;Row<FMath::Min(Total,(Page+1)*PageSize);++Row) {
  if(Row<S->StationConfigs.Num()) {
   const auto& C=S->StationConfigs[Row]; auto* B=IsValid(C.Station)?C.Station->GetStation():nullptr;
   if(!IsValid(B)) { Lines.Add(NSLOCTEXT("FILN","StationMissing","Station removed or unavailable")); continue; }
   const auto* Fluid=FindFProperty<FBoolProperty>(B->GetClass(),TEXT("mIsFluidStorageInventory"));
   const bool Reserved=S->Reservations.StationBusy(TCHAR_TO_UTF8(*S->StationId(C.Station).ToString()));
   Lines.Add(FText::Format(NSLOCTEXT("FILN","StationRow","{0} | {1} | {2} | {3} | Cargo: {4} | {5}"),
    C.Station->GetStationName(),B->GetIsInLoadMode()?NSLOCTEXT("FILN","Provider","PROVIDER"):NSLOCTEXT("FILN","Requester","REQUESTER"),
    Fluid && Fluid->GetPropertyValue_InContainer(B)?NSLOCTEXT("FILN","Fluid","Fluid"):NSLOCTEXT("FILN","Solid","Solid"),
    C.Resource?UFGItemDescriptor::GetItemName(C.Resource):NSLOCTEXT("FILN","MissingResource","Missing resource"),
    FText::AsNumber(C.Resource && B->GetInventory()?B->GetInventory()->GetNumItems(C.Resource):0),
    Reserved?NSLOCTEXT("FILN","Reserved","Reserved"):NSLOCTEXT("FILN","Unreserved","Unreserved")));
  } else if(Row<S->StationConfigs.Num()+S->Registrations.Num()) {
   const auto& R=S->Registrations[Row-S->StationConfigs.Num()]; auto* Owner=IsValid(R.Vehicle)?R.Vehicle->GetOwnerVehicle():nullptr;
   if(!IsValid(Owner)) { Lines.Add(NSLOCTEXT("FILN","VehicleMissing","Registered vehicle removed or unavailable")); continue; }
   Lines.Add(FText::Format(NSLOCTEXT("FILN","VehicleRow","{0} | Home: {1} | {2} | Autopilot: {3} | Fuel available: {4} | Driver present: {5} | Held: {6} | Delivered: {7}\n  {8}"),
    R.Vehicle->GetVehicleName(),IsValid(R.Home)?R.Home->GetStationName():NSLOCTEXT("FILN","MissingHome","Missing depot"),StateLabel(R.State),
    Enabled(R.Vehicle->IsAutopilotEnabled()),YesNo(Owner->HasFuel()),YesNo(Owner->GetDriver()!=nullptr),YesNo(R.Held),FText::AsNumber(R.DeliveredCargo),FText::FromString(R.Diagnostic)));
   if(R.JobId.IsValid()) Lines.Add(FText::Format(NSLOCTEXT("FILN","JobDetails","  Job {0} | {1} → {2} | {3} | Planned: {4} | Loaded: {5} | Delivered: {6} | Cargo: {7} (native units)"),FText::FromString(R.JobId.ToString()),IsValid(R.Provider)?R.Provider->GetStationName():NSLOCTEXT("FILN","NoPickup","No pickup"),IsValid(R.Requester)?R.Requester->GetStationName():NSLOCTEXT("FILN","MissingRequester","Missing requester"),R.Resource?UFGItemDescriptor::GetItemName(R.Resource):NSLOCTEXT("FILN","MissingResource","Missing resource"),FText::AsNumber(R.ExpectedCargo),FText::AsNumber(R.LoadedCargo),FText::AsNumber(R.DeliveredCargo),FText::AsNumber(R.LastCargo)));
  } else {
   auto* D=Unclaimed[Row-S->StationConfigs.Num()-S->Registrations.Num()];
   Lines.Add(FText::Format(NSLOCTEXT("FILN","UnclaimedDepot","{0} | Unclaimed rest stop | Eligible: {1}"),D->GetStationName(),YesNo(S->ValidHome(D))));
  }
 }
 const FText Text=JoinLines(Lines);
 OverviewReply(Target,Page,Pages,Text);
}
void UFILNRemoteControl::OverviewReply_Implementation(AFGBuildableDockingStation* Target,int32 Page,int32 Pages,const FText& Text) {
 if(Receiver.IsValid()) Receiver->ShowOverview(Target,Page,Pages,Text);
}
void UFILNRemoteControl::RegisterVehicle_Implementation(AFGBuildableDockingStation* Target,AFGWheeledVehicleIdentifier* Vehicle) {
 if(!ValidateContext(Target) || !IsValid(Vehicle) || Vehicle->GetWorld()!=GetWorld()) return;
 auto* S=Subsystem(GetWorld()); if(!S) return;
 const bool Accepted=S->Pair(Vehicle,Target->GetStationIdentifier());
 const auto* C=S->StationConfigs.FindByPredicate([Target](const FFILNStationConfig& Entry){return Entry.Station && Entry.Station->GetStation()==Target;});
 StationReply(Target,C!=nullptr,C?C->Resource:TSubclassOf<UFGItemDescriptor>(),Accepted?NSLOCTEXT("FILN","VehicleRegistered","Vehicle registered: native home-only route and autopilot enabled"):NSLOCTEXT("FILN","VehicleRejected","Enrollment rejected: needs a unique unclaimed empty LOAD depot and empty stopped undriven truck within 8 m; existing non-home or disabled routes are not changed"));
}
void UFILNRemoteControl::UnregisterVehicle_Implementation(AFGBuildableDockingStation* Target,AFGWheeledVehicleIdentifier* Vehicle) {
 if(!ValidateContext(Target) || !IsValid(Vehicle) || Vehicle->GetWorld()!=GetWorld()) return;
 auto* S=Subsystem(GetWorld()); if(!S) return;
 auto* Mode=GetWorld()->GetAuthGameMode<AGameMode>();
 auto* Session=Mode?Cast<AFGGameSession>(Mode->GameSession):nullptr;
 const bool Admin=Session && Session->IsPlayerAdmin(GetOwnerPlayerController());
 const auto* Index=S->ByVehicle.Find(Vehicle);
 const bool Accepted=Admin && Index && S->UnpairParkedVehicle(*Index,Target->GetStationIdentifier());
 StationReply(Target,false,nullptr,!Admin?NSLOCTEXT("FILN","AdminRequired","Server administrator permission required"):
  Accepted?NSLOCTEXT("FILN","VehicleUnregistered","Vehicle unregistered; route, cargo and rest-stop role preserved"):
  NSLOCTEXT("FILN","UnregisterRejected","Unregister rejected. Finish the job and refueling, park at the assigned depot and turn off autopilot."));
}
void UFILNRemoteControl::RehomeVehicle_Implementation(AFGBuildableDockingStation* Target,AFGWheeledVehicleIdentifier* Vehicle) {
 if(!ValidateContext(Target) || !IsValid(Vehicle) || Vehicle->GetWorld()!=GetWorld()) return;
 auto* S=Subsystem(GetWorld()); if(!S) return;
 auto* Mode=GetWorld()->GetAuthGameMode<AGameMode>();
 auto* Session=Mode?Cast<AFGGameSession>(Mode->GameSession):nullptr;
 const bool Admin=Session && Session->IsPlayerAdmin(GetOwnerPlayerController());
 const auto* Index=S->ByVehicle.Find(Vehicle);
 const bool Accepted=Admin && Index && S->RehomeParkedVehicle(*Index,Target->GetStationIdentifier());
 const auto* C=S->StationConfigs.FindByPredicate([Target](const FFILNStationConfig& E){return IsValid(E.Station) && E.Station->GetStation()==Target;});
 StationReply(Target,C!=nullptr,C?C->Resource:TSubclassOf<UFGItemDescriptor>(),
  !Admin?NSLOCTEXT("FILN","AdminRequired","Server administrator permission required"):
  Accepted?NSLOCTEXT("FILN","RehomeAccepted","Recovery accepted. Cargo preserved; autopilot remains off."):
  NSLOCTEXT("FILN","RehomeRejected","Recovery rejected. Use a designated unclaimed rest stop, dock the registered truck, stop and disable autopilot first."));
}
void UFILNRemoteControl::AdminAction_Implementation(AFGBuildableDockingStation* Target,uint8 Action) {
 if(!ValidateContext(Target)) return;
 auto* Mode=GetWorld()->GetAuthGameMode<AGameMode>();
 auto* Session=Mode?Cast<AFGGameSession>(Mode->GameSession):nullptr;
 if(!Session || !Session->IsPlayerAdmin(GetOwnerPlayerController())) {
  if(auto* S=Subsystem(GetWorld())) {
   const auto* C=S->StationConfigs.FindByPredicate([Target](const FFILNStationConfig& Entry){return Entry.Station && Entry.Station->GetStation()==Target;});
   StationReply(Target,C!=nullptr,C?C->Resource:TSubclassOf<UFGItemDescriptor>(),NSLOCTEXT("FILN","AdminRequired","Server administrator permission required"));
  }
  return;
 }
 auto* S=Subsystem(GetWorld()); if(!S) return;
 FText Message;
 if(Action==0) {
  const bool Ok=S->SetDispatchEnabled(!S->bAutoDispatchEnabled);
  Message=Ok?FText::Format(NSLOCTEXT("FILN","DispatcherChanged","Automatic dispatcher: {0}; existing deliveries continue"),Enabled(S->bAutoDispatchEnabled)):
   NSLOCTEXT("FILN","DispatcherBlocked","Cannot enable dispatcher: unresolved native registry or filter conflict");
 }
 else if(Action==3) Message=S->PrepareStationsForUninstall()?NSLOCTEXT("FILN","UninstallPrepared","Restrictions restored; registered parked trucks have autopilot off. Save required; mod-disabled load still needs independent verification"):NSLOCTEXT("FILN","UninstallRejected","Preparation rejected: finish jobs and refueling, park registered trucks, resolve conflicts. Any trucks already stopped remain AP-off.");
 else if(Action==8 || Action==9) {
  const bool Ok=S->SetDepot(Target->GetStationIdentifier(),Action==9);
  Message=Ok?NSLOCTEXT("FILN","DepotUpdated","Rest-stop role updated"):NSLOCTEXT("FILN","DepotRejected","Rest-stop role rejected: cargo, connection, docking or ownership conflict");
 }
 else return;
 const auto* C=S->StationConfigs.FindByPredicate([Target](const FFILNStationConfig& Entry){return Entry.Station && Entry.Station->GetStation()==Target;});
 StationReply(Target,C!=nullptr,C?C->Resource:TSubclassOf<UFGItemDescriptor>(),Message);
}
void UFILNRemoteControl::StationReply_Implementation(AFGBuildableDockingStation* Target,bool Enrolled,TSubclassOf<UFGItemDescriptor> Resource,const FText& Message) {
 if(Receiver.IsValid()) Receiver->Reply(Target,Enrolled,Resource,Message);
}
void UFILNStationControls::NativeOnInitialized() {
 Super::NativeOnInitialized();
 auto* Root=WidgetTree->ConstructWidget<UVerticalBox>(); WidgetTree->RootWidget=Root;
 OpenButton=WidgetTree->ConstructWidget<UButton>();
 auto* OpenLabel=WidgetTree->ConstructWidget<UTextBlock>(); OpenLabel->SetText(NSLOCTEXT("FILN","OpenControls","FILN logistics"));
 OpenButton->AddChild(OpenLabel); Root->AddChildToVerticalBox(OpenButton);
 OpenButton->OnClicked.AddDynamic(this,&UFILNStationControls::OpenPanel);
 Panel=WidgetTree->ConstructWidget<UVerticalBox>(); Root->AddChildToVerticalBox(Panel);
 auto* Box=Panel.Get();
 Feedback=WidgetTree->ConstructWidget<UTextBlock>(); Box->AddChildToVerticalBox(Feedback);
 Selector=WidgetTree->ConstructWidget<UComboBoxString>(); Box->AddChildToVerticalBox(Selector);
 auto* Buttons=WidgetTree->ConstructWidget<UHorizontalBox>(); Box->AddChildToVerticalBox(Buttons);
 auto* ApplyButton=WidgetTree->ConstructWidget<UButton>(); auto* ApplyText=WidgetTree->ConstructWidget<UTextBlock>();
 ApplyText->SetText(NSLOCTEXT("FILN","ApplyResource","Register / set resource")); ApplyButton->AddChild(ApplyText); Buttons->AddChildToHorizontalBox(ApplyButton); ApplyButton->OnClicked.AddDynamic(this,&UFILNStationControls::Apply);
 auto* RemoveButton=WidgetTree->ConstructWidget<UButton>(); auto* RemoveText=WidgetTree->ConstructWidget<UTextBlock>();
 RemoveText->SetText(NSLOCTEXT("FILN","RemoveStation","Remove from FILN")); RemoveButton->AddChild(RemoveText); Buttons->AddChildToHorizontalBox(RemoveButton); RemoveButton->OnClicked.AddDynamic(this,&UFILNStationControls::Remove);
 Vehicles=WidgetTree->ConstructWidget<UComboBoxString>(); Box->AddChildToVerticalBox(Vehicles);
 auto AddAction=[&](const FText& Text) {
  auto* Button=WidgetTree->ConstructWidget<UButton>(); auto* Label=WidgetTree->ConstructWidget<UTextBlock>(); Label->SetText(Text); Button->AddChild(Label); Box->AddChildToVerticalBox(Button); return Button;
 };
 AddAction(NSLOCTEXT("FILN","RegisterDepot","Designate this station as a rest stop (admin)"))->OnClicked.AddDynamic(this,&UFILNStationControls::RegisterDepot);
 AddAction(NSLOCTEXT("FILN","RemoveDepot","Remove rest-stop role (admin)"))->OnClicked.AddDynamic(this,&UFILNStationControls::RemoveDepot);
 AddAction(NSLOCTEXT("FILN","EnrollVehicle","Register selected vehicle at this depot"))->OnClicked.AddDynamic(this,&UFILNStationControls::EnrollVehicle);
 AddAction(NSLOCTEXT("FILN","UnregisterVehicle","Unregister selected parked truck (admin)"))->OnClicked.AddDynamic(this,&UFILNStationControls::UnregisterVehicle);
 AddAction(NSLOCTEXT("FILN","RecoverVehicle","Recover selected parked truck at this rest stop (admin)"))->OnClicked.AddDynamic(this,&UFILNStationControls::RecoverVehicle);
 AddAction(NSLOCTEXT("FILN","ToggleDispatcher","Enable / disable automatic dispatch (admin)"))->OnClicked.AddDynamic(this,&UFILNStationControls::ToggleDispatcher);
 AddAction(NSLOCTEXT("FILN","PrepareUninstall","Prepare for uninstall (admin)"))->OnClicked.AddDynamic(this,&UFILNStationControls::PrepareUninstall);
 AddAction(NSLOCTEXT("FILN","Overview","Network overview / refresh"))->OnClicked.AddDynamic(this,&UFILNStationControls::QueryOverview);
 AddAction(NSLOCTEXT("FILN","PreviousPage","Previous page"))->OnClicked.AddDynamic(this,&UFILNStationControls::PreviousPage);
 AddAction(NSLOCTEXT("FILN","NextPage","Next page"))->OnClicked.AddDynamic(this,&UFILNStationControls::NextPage);
 auto* Size=WidgetTree->ConstructWidget<USizeBox>(); Size->SetHeightOverride(220.f); Box->AddChildToVerticalBox(Size);
 auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>(); Size->AddChild(Scroll);
 Overview=WidgetTree->ConstructWidget<UTextBlock>(); Overview->SetAutoWrapText(true); Scroll->AddChild(Overview);
 Feedback->SetAutoWrapText(true);
}
UFILNRemoteControl* UFILNStationControls::Remote() const {
 auto* PC=Cast<AFGPlayerController>(GetOwningPlayer());
 return PC?PC->GetRemoteCallObjectOfClass<UFILNRemoteControl>():nullptr;
}
FText UFILNLabelBinding::Read() {
 const FText Text=Original.IsBound()?Original.Execute():Fallback;
 if(!Enrolled) return Text;
 const FString* Source=FTextInspector::GetSourceString(Text); const FString Value=Source?*Source:Text.ToString();
 if(Value.Equals(TEXT("LOAD"),ESearchCase::IgnoreCase)) return NSLOCTEXT("FILN","Provider","PROVIDER");
 if(Value.Equals(TEXT("UNLOAD"),ESearchCase::IgnoreCase)) return NSLOCTEXT("FILN","Requester","REQUESTER");
 return Text;
}
void UFILNStationControls::RestoreLabels() {
 for(const auto& Binding:LabelBindings) if(IsValid(Binding->Label) && Binding->Label->TextDelegate.IsBoundToObject(Binding)) {
  Binding->Label->TextDelegate=Binding->Original; Binding->Label->SynchronizeProperties();
 }
 LabelBindings.Empty();
}
void UFILNStationWindow::NativeOnInitialized() {
 Super::NativeOnInitialized();
 Controls=CreateWidget<UFILNStationControls>(GetOwningPlayer());
 auto* Box=WidgetTree->ConstructWidget<UVerticalBox>(); WidgetTree->RootWidget=Box;
 Box->AddChildToVerticalBox(Controls);
}
void UFILNStationWindow::NativeConstruct() {
 Super::NativeConstruct();
 auto* Target=Cast<AFGBuildableDockingStation>(mInteractObject); if(Target && Controls) Controls->SetContext(Target,this);
}
void UFILNStationControls::SetContext(AFGBuildableDockingStation* Target,UUserWidget* Vanilla) {
 // Pooled interaction widgets may later serve an ordinary station: restore exact original text first.
 RestoreLabels(); OverviewPage=0; OverviewPages=1; Overview->SetText(FText::GetEmpty()); Station=Target; Resources.Empty(); Selector->ClearOptions();
 if(Vanilla && Vanilla->WidgetTree) Vanilla->WidgetTree->ForEachWidget([this](UWidget* Widget) {
  auto* Text=Cast<UTextBlock>(Widget); if(!Text) return;
  const FString* Source=FTextInspector::GetSourceString(Text->GetText());
  const FString Value=Source?*Source:Text->GetText().ToString();
  if(Value.Equals(TEXT("LOAD"),ESearchCase::IgnoreCase) || Value.Equals(TEXT("UNLOAD"),ESearchCase::IgnoreCase)) {
   auto* Binding=NewObject<UFILNLabelBinding>(this); Binding->Label=Text; Binding->Original=Text->TextDelegate; Binding->Fallback=Text->GetText();
   Text->TextDelegate.BindDynamic(Binding,&UFILNLabelBinding::Read); Text->SynchronizeProperties(); LabelBindings.Add(Binding);
  }
 });
 if(auto* Recipes=AFGRecipeManager::Get(GetWorld())) for(const auto& Resource:Recipes->GetAllAvailableItemDescriptors()) {
  if(!Compatible(Resource,Target)) continue;
  FString Name=UFGItemDescriptor::GetItemName(Resource).ToString();
  if(Selector->FindOptionIndex(Name)!=INDEX_NONE) Name+=FString::Printf(TEXT(" (%d)"),Resources.Num()+1);
  Selector->AddOption(Name); Resources.Add(Resource);
 }
 VehicleChoices.Empty(); Vehicles->ClearOptions();
 if(auto* VS=AFGVehicleSubsystem::Get(GetWorld())) for(auto* Vehicle:VS->GetAllVehicles()) if(IsValid(Vehicle)) {
  auto* Owner=Vehicle->GetOwnerVehicle();
  if(IsValid(Owner) && Target && FVector::DistSquared(Owner->GetVehicleLocation(),Target->GetActorLocation())<=FMath::Square(2000.f)) {
   VehicleChoices.Add(Vehicle); Vehicles->AddOption(FString::Printf(TEXT("%s (%d)"),*Vehicle->GetVehicleName().ToString(),VehicleChoices.Num()));
  }
 }
 // Ordinary stations keep their original switch and layout, with only a
 // collapsed FILN entry point. Do not issue an RPC until the player opens it.
 const bool Show=Target && (Target->FindComponentByClass<UFILNStationMarker>() || Cast<UFILNStationWindow>(Vanilla));
 Panel->SetVisibility(Show?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
 OpenButton->SetVisibility(Show?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
 if(Show) OpenPanel();
}
void UFILNStationControls::OpenPanel() {
 if(!IsValid(Station)) return;
 Panel->SetVisibility(ESlateVisibility::Visible); OpenButton->SetVisibility(ESlateVisibility::Collapsed);
 Feedback->SetText(NSLOCTEXT("FILN","Loading","Reading station state from server"));
 if(auto* R=Remote()) { R->Receiver=this; R->QueryStation(Station); }
 else Feedback->SetText(NSLOCTEXT("FILN","NoRemote","FILN server control unavailable"));
}
void UFILNStationControls::Reply(AFGBuildableDockingStation* Target,bool Enrolled,TSubclassOf<UFGItemDescriptor> Resource,const FText& Message) {
 if(Target!=Station) return;
 Feedback->SetText(Message);
 const int32 Selected=Resources.IndexOfByKey(Resource); if(Selected!=INDEX_NONE) Selector->SetSelectedIndex(Selected);
 for(const auto& Binding:LabelBindings) if(IsValid(Binding->Label)) { Binding->Enrolled=Enrolled; Binding->Label->SynchronizeProperties(); }
}
void UFILNStationControls::Apply() {
 const int32 Index=Selector->GetSelectedIndex();
 if(auto* R=Remote(); R && Resources.IsValidIndex(Index)) { R->Receiver=this; R->Configure(Station,Resources[Index],false); }
}
void UFILNStationControls::Remove() { if(auto* R=Remote()) { R->Receiver=this; R->Configure(Station,nullptr,true); } }
void UFILNStationControls::ShowOverview(AFGBuildableDockingStation* Target,int32 Page,int32 Pages,const FText& Text) {
 if(Target!=Station) return; OverviewPage=Page; OverviewPages=Pages;
 Overview->SetText(FText::Format(NSLOCTEXT("FILN","OverviewPage","Page {0}/{1}\n{2}"),FText::AsNumber(Page+1),FText::AsNumber(Pages),Text));
}
void UFILNStationControls::QueryOverview() { if(auto* R=Remote()) { R->Receiver=this; R->NetworkOverview(Station,OverviewPage); } }
void UFILNStationControls::NextPage() { if(OverviewPage+1<OverviewPages) { ++OverviewPage; QueryOverview(); } }
void UFILNStationControls::PreviousPage() { if(OverviewPage>0) { --OverviewPage; QueryOverview(); } }
void UFILNStationControls::EnrollVehicle() {
 const int32 Index=Vehicles->GetSelectedIndex();
 if(auto* R=Remote(); R && VehicleChoices.IsValidIndex(Index)) { R->Receiver=this; R->RegisterVehicle(Station,VehicleChoices[Index]); }
}
void UFILNStationControls::UnregisterVehicle() {
 const int32 Index=Vehicles->GetSelectedIndex();
 if(auto* R=Remote(); R && VehicleChoices.IsValidIndex(Index)) { R->Receiver=this; R->UnregisterVehicle(Station,VehicleChoices[Index]); }
}
void UFILNStationControls::RecoverVehicle() {
 const int32 Index=Vehicles->GetSelectedIndex();
 if(auto* R=Remote(); R && VehicleChoices.IsValidIndex(Index)) { R->Receiver=this; R->RehomeVehicle(Station,VehicleChoices[Index]); }
}
void UFILNStationControls::RegisterDepot() { if(auto* R=Remote()) { R->Receiver=this; R->AdminAction(Station,8); } }
void UFILNStationControls::RemoveDepot() { if(auto* R=Remote()) { R->Receiver=this; R->AdminAction(Station,9); } }
void UFILNStationControls::ToggleDispatcher() { if(auto* R=Remote()) { R->Receiver=this; R->AdminAction(Station,0); } }
void UFILNStationControls::PrepareUninstall() { if(auto* R=Remote()) { R->Receiver=this; R->AdminAction(Station,3); } }
void UFILNStationControls::InstallHook() {
 if(WITH_EDITOR) return;
 SUBSCRIBE_METHOD_AFTER(AFGHUD::RequestInteractWidget,[](UFGInteractWidget* Widget,AFGHUD* HUD,TSubclassOf<UFGInteractWidget>,UObject* Object) {
  if(Widget && Widget->IsA<UFILNStationWindow>()) return;
  auto* Target=Cast<AFGBuildableDockingStation>(Object);
  if(!Widget || !Widget->WidgetTree || !Target || !HUD->GetOwningPlayerController()) return;
  UFILNStationControls* Controls=nullptr;
  Widget->WidgetTree->ForEachWidget([&](UWidget* Child){if(auto* Existing=Cast<UFILNStationControls>(Child)) Controls=Existing;});
  if(!Controls) {
   Controls=CreateWidget<UFILNStationControls>(HUD->GetOwningPlayerController()); if(!Controls) return;
   auto* Root=Widget->WidgetTree->RootWidget.Get();
   auto* Wrapper=Widget->WidgetTree->ConstructWidget<UVerticalBox>(); Widget->WidgetTree->RootWidget=Wrapper;
   if(Root) Wrapper->AddChildToVerticalBox(Root)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
   Wrapper->AddChildToVerticalBox(Controls);
  }
  Controls->SetContext(Target,Widget);
 });
}
