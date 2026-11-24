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

namespace {
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
 FString Message=C?TEXT("FILN station enrolled"):TEXT("Choose a resource and register this station with FILN");
 if(C && S->Reservations.StationBusy(TCHAR_TO_UTF8(*S->StationId(C->Station).ToString()))) Message=TEXT("Active logistics reservation; resource changes locked");
 Message+=FString::Printf(TEXT("\nDispatcher=%d registered vehicles=%d active deliveries=%d"),S->bAutoDispatchEnabled,S->Registrations.Num(),S->Active.Num());
 for(const auto& R:S->Registrations) if(R.Home==Target->GetStationIdentifier() || R.Provider==Target->GetStationIdentifier() || R.Requester==Target->GetStationIdentifier()) {
  Message+=FString::Printf(TEXT("\nVehicle %s state=%d: %s"),*R.TruckId.ToString(),R.State,*R.Diagnostic);
 }
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
 StationReply(Target,C!=nullptr,C?C->Resource:TSubclassOf<UFGItemDescriptor>(),Accepted?TEXT("FILN station updated"):TEXT("Rejected: station must be empty, undocked, unreserved and compatible; existing filters must agree"));
}
void UFILNRemoteControl::NetworkOverview_Implementation(AFGBuildableDockingStation* Target,int32 Page) {
 if(!ValidateContext(Target) || Page<0) return;
 auto* S=Subsystem(GetWorld()); if(!S) return;
 constexpr int32 PageSize=32;
 const int32 Total=S->StationConfigs.Num()+S->Registrations.Num();
 const int32 Pages=FMath::Max(1,FMath::DivideAndRoundUp(Total,PageSize));
 if(Page>=Pages) return;
 FString Text=FString::Printf(TEXT("Stations=%d vehicles/depots=%d active deliveries=%d reservations=%llu dispatcher=%s\n"),S->StationConfigs.Num(),S->Registrations.Num(),S->Active.Num(),static_cast<uint64>(S->Reservations.Size()),S->bAutoDispatchEnabled?TEXT("enabled"):TEXT("disabled"));
 for(int32 Row=Page*PageSize;Row<FMath::Min(Total,(Page+1)*PageSize);++Row) {
  if(Row<S->StationConfigs.Num()) {
   const auto& C=S->StationConfigs[Row]; auto* B=IsValid(C.Station)?C.Station->GetStation():nullptr;
   if(!IsValid(B)) { Text+=TEXT("Station removed / unavailable\n"); continue; }
   const auto* Fluid=FindFProperty<FBoolProperty>(B->GetClass(),TEXT("mIsFluidStorageInventory"));
   const bool Reserved=S->Reservations.StationBusy(TCHAR_TO_UTF8(*S->StationId(C.Station).ToString()));
   Text+=FString::Printf(TEXT("%s | %s | %s | %s | cargo=%d | %s\n"),*C.Station->GetStationName().ToString(),B->GetIsInLoadMode()?TEXT("PROVIDER"):TEXT("REQUESTER"),Fluid && Fluid->GetPropertyValue_InContainer(B)?TEXT("fluid"):TEXT("solid"),C.Resource?*UFGItemDescriptor::GetItemName(C.Resource).ToString():TEXT("missing resource"),C.Resource && B->GetInventory()?B->GetInventory()->GetNumItems(C.Resource):0,Reserved?TEXT("reserved"):TEXT("unreserved"));
  } else {
   const auto& R=S->Registrations[Row-S->StationConfigs.Num()]; auto* Owner=IsValid(R.Vehicle)?R.Vehicle->GetOwnerVehicle():nullptr;
   if(!IsValid(Owner)) { Text+=TEXT("Registered vehicle removed / unavailable\n"); continue; }
   Text+=FString::Printf(TEXT("%s | home=%s | state=%d | AP=%s | fuel=%s | driver=%s | held=%d | delivered=%d\n  %s\n"),*R.Vehicle->GetVehicleName().ToString(),IsValid(R.Home)?*R.Home->GetStationName().ToString():TEXT("missing"),R.State,R.Vehicle->IsAutopilotEnabled()?TEXT("on"):TEXT("off"),Owner->HasFuel()?TEXT("available"):TEXT("empty"),Owner->GetDriver()?TEXT("present"):TEXT("none"),R.Held,R.DeliveredCargo,*R.Diagnostic);
  }
 }
 OverviewReply(Target,Page,Pages,Text);
}
void UFILNRemoteControl::OverviewReply_Implementation(AFGBuildableDockingStation* Target,int32 Page,int32 Pages,const FString& Text) {
 if(Receiver.IsValid()) Receiver->ShowOverview(Target,Page,Pages,Text);
}
void UFILNRemoteControl::RegisterVehicle_Implementation(AFGBuildableDockingStation* Target,AFGWheeledVehicleIdentifier* Vehicle) {
 if(!ValidateContext(Target) || !IsValid(Vehicle) || Vehicle->GetWorld()!=GetWorld()) return;
 auto* S=Subsystem(GetWorld()); if(!S) return;
 const bool Accepted=S->Pair(Vehicle,Target->GetStationIdentifier());
 const auto* C=S->StationConfigs.FindByPredicate([Target](const FFILNStationConfig& Entry){return Entry.Station && Entry.Station->GetStation()==Target;});
 StationReply(Target,C!=nullptr,C?C->Resource:TSubclassOf<UFGItemDescriptor>(),Accepted?TEXT("Vehicle registered: native home-only route and autopilot enabled"):TEXT("Enrollment rejected: needs a unique unclaimed empty LOAD depot and empty stopped undriven truck within 8m; existing non-home or disabled routes are not changed"));
}
void UFILNRemoteControl::AdminAction_Implementation(AFGBuildableDockingStation* Target,uint8 Action) {
 if(!ValidateContext(Target)) return;
 auto* Mode=GetWorld()->GetAuthGameMode<AGameMode>();
 auto* Session=Mode?Cast<AFGGameSession>(Mode->GameSession):nullptr;
 if(!Session || !Session->IsPlayerAdmin(GetOwnerPlayerController())) {
  if(auto* S=Subsystem(GetWorld())) {
   const auto* C=S->StationConfigs.FindByPredicate([Target](const FFILNStationConfig& Entry){return Entry.Station && Entry.Station->GetStation()==Target;});
   StationReply(Target,C!=nullptr,C?C->Resource:TSubclassOf<UFGItemDescriptor>(),TEXT("Server administrator permission required"));
  }
  return;
 }
 auto* S=Subsystem(GetWorld()); if(!S) return;
 FString Message;
 if(Action==0) { S->bAutoDispatchEnabled=!S->bAutoDispatchEnabled; S->ResetScan(); Message=FString::Printf(TEXT("Automatic dispatcher=%d; existing deliveries continue"),S->bAutoDispatchEnabled); }
 else if(Action==1 || Action==2) { S->ImportLegacy(Action==2); Message=TEXT("Legacy settings report; no physical actor conversion performed\n")+FString::Join(S->MigrationReport,TEXT("\n")); }
 else if(Action==3) Message=S->PrepareStationsForUninstall()?TEXT("Restrictions restored. Save required; mod-disabled load still needs independent verification"):TEXT("Preparation rejected: active jobs or station/filter conflict");
 else return;
 const auto* C=S->StationConfigs.FindByPredicate([Target](const FFILNStationConfig& Entry){return Entry.Station && Entry.Station->GetStation()==Target;});
 StationReply(Target,C!=nullptr,C?C->Resource:TSubclassOf<UFGItemDescriptor>(),Message);
}
void UFILNRemoteControl::StationReply_Implementation(AFGBuildableDockingStation* Target,bool Enrolled,TSubclassOf<UFGItemDescriptor> Resource,const FString& Message) {
 if(Receiver.IsValid()) Receiver->Reply(Target,Enrolled,Resource,Message);
}
void UFILNStationControls::NativeOnInitialized() {
 Super::NativeOnInitialized();
 auto* Box=WidgetTree->ConstructWidget<UVerticalBox>(); WidgetTree->RootWidget=Box;
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
 AddAction(NSLOCTEXT("FILN","EnrollVehicle","Register selected vehicle at this depot"))->OnClicked.AddDynamic(this,&UFILNStationControls::EnrollVehicle);
 AddAction(NSLOCTEXT("FILN","ToggleDispatcher","Enable / disable automatic dispatch (admin)"))->OnClicked.AddDynamic(this,&UFILNStationControls::ToggleDispatcher);
 AddAction(NSLOCTEXT("FILN","PreviewLegacy","Preview legacy settings import (admin)"))->OnClicked.AddDynamic(this,&UFILNStationControls::PreviewLegacy);
 AddAction(NSLOCTEXT("FILN","ImportLegacy","Import compatible legacy settings (admin)"))->OnClicked.AddDynamic(this,&UFILNStationControls::ImportLegacy);
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
 Feedback->SetText(NSLOCTEXT("FILN","Loading","Reading station state from server"));
 if(auto* R=Remote()) { R->Receiver=this; R->QueryStation(Target); }
 else Feedback->SetText(NSLOCTEXT("FILN","NoRemote","FILN server control unavailable"));
}
void UFILNStationControls::Reply(AFGBuildableDockingStation* Target,bool Enrolled,TSubclassOf<UFGItemDescriptor> Resource,const FString& Message) {
 if(Target!=Station) return;
 Feedback->SetText(FText::FromString(Message));
 const int32 Selected=Resources.IndexOfByKey(Resource); if(Selected!=INDEX_NONE) Selector->SetSelectedIndex(Selected);
 for(const auto& Binding:LabelBindings) if(IsValid(Binding->Label)) { Binding->Enrolled=Enrolled; Binding->Label->SynchronizeProperties(); }
}
void UFILNStationControls::Apply() {
 const int32 Index=Selector->GetSelectedIndex();
 if(auto* R=Remote(); R && Resources.IsValidIndex(Index)) { R->Receiver=this; R->Configure(Station,Resources[Index],false); }
}
void UFILNStationControls::Remove() { if(auto* R=Remote()) { R->Receiver=this; R->Configure(Station,nullptr,true); } }
void UFILNStationControls::ShowOverview(AFGBuildableDockingStation* Target,int32 Page,int32 Pages,const FString& Text) {
 if(Target!=Station) return; OverviewPage=Page; OverviewPages=Pages;
 Overview->SetText(FText::FromString(FString::Printf(TEXT("Page %d/%d\n"),Page+1,Pages)+Text));
}
void UFILNStationControls::QueryOverview() { if(auto* R=Remote()) { R->Receiver=this; R->NetworkOverview(Station,OverviewPage); } }
void UFILNStationControls::NextPage() { if(OverviewPage+1<OverviewPages) { ++OverviewPage; QueryOverview(); } }
void UFILNStationControls::PreviousPage() { if(OverviewPage>0) { --OverviewPage; QueryOverview(); } }
void UFILNStationControls::EnrollVehicle() {
 const int32 Index=Vehicles->GetSelectedIndex();
 if(auto* R=Remote(); R && VehicleChoices.IsValidIndex(Index)) { R->Receiver=this; R->RegisterVehicle(Station,VehicleChoices[Index]); }
}
void UFILNStationControls::ToggleDispatcher() { if(auto* R=Remote()) { R->Receiver=this; R->AdminAction(Station,0); } }
void UFILNStationControls::PreviewLegacy() { if(auto* R=Remote()) { R->Receiver=this; R->AdminAction(Station,1); } }
void UFILNStationControls::ImportLegacy() { if(auto* R=Remote()) { R->Receiver=this; R->AdminAction(Station,2); } }
void UFILNStationControls::PrepareUninstall() { if(auto* R=Remote()) { R->Receiver=this; R->AdminAction(Station,3); } }
void UFILNStationControls::InstallHook() {
 if(WITH_EDITOR) return;
 SUBSCRIBE_METHOD_AFTER(AFGHUD::RequestInteractWidget,[](UFGInteractWidget* Widget,AFGHUD* HUD,TSubclassOf<UFGInteractWidget>,UObject* Object) {
  if(Widget && Widget->IsA<UFILNStationWindow>()) return;
  auto* Target=Cast<AFGBuildableDockingStation>(Object);
  if(!Widget || !Widget->WidgetTree || !Target || !HUD->GetOwningPlayerController()) return;
  UFILNStationControls* Controls=nullptr;
  Widget->WidgetTree->ForEachWidget([&](UWidget* Child){if(auto* Existing=Cast<UFILNStationControls>(Child)) Controls=Existing;});
  if(!Target->FindComponentByClass<UFILNStationMarker>()) {
   if(Controls) {
    Controls->RestoreLabels(); Controls->RemoveFromParent();
    if(auto* Wrapper=Cast<UVerticalBox>(Widget->WidgetTree->RootWidget); Wrapper && Wrapper->GetChildrenCount()==1) {
     auto* Original=Wrapper->GetChildAt(0); Wrapper->RemoveChild(Original); Widget->WidgetTree->RootWidget=Original;
    }
   }
   return;
  }
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
