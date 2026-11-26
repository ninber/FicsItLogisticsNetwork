// SPDX-License-Identifier: GPL-3.0-only
#include "FILNStationControls.h"
#include "FILNSelectorPolicy.h"
#include "LogisticsSubsystem.h"
#include "FGHUD.h"
#include "FGPlayerController.h"
#include "FGCharacterPlayer.h"
#include "FGRecipeManager.h"
#include "Resources/FGItemDescriptor.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "Registry/RemoteCallObjectRegistry.h"
#include "Subsystem/SubsystemActorManager.h"
#include "UI/FGInteractWidget.h"
#include "UI/FGGameUI.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "UObject/UnrealType.h"
#include "UObject/StructOnScope.h"
#include "Net/UnrealNetwork.h"
#include "Patching/NativeHookManager.h"
#include "HAL/PlatformTime.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

namespace {
// Exact serialized SML 3.12.1 asset exports, not widget-name substring guesses.
constexpr const TCHAR* FILNDockingWidget=TEXT("/Game/FactoryGame/Buildable/-Shared/Widgets/Widget_DockingStation.Widget_DockingStation_C");
constexpr const TCHAR* FILNTabWidget=TEXT("/Game/FactoryGame/Interface/UI/InGame/-Shared/Widget_TabsContainer.Widget_TabsContainer_C");
constexpr const TCHAR* FILNPartsWidget=TEXT("/Game/FactoryGame/Interface/UI/InGame/-Shared/InputPartsList/BPW_Input_PartsList.BPW_Input_PartsList_C");
constexpr const TCHAR* FILNTabStruct=TEXT("/Game/FactoryGame/Interface/UI/InGame/-Shared/ImageAndText.ImageAndText");

ALogisticsSubsystem* FILNSelectorSubsystem(UWorld* World) {
 auto* Manager=World?World->GetSubsystem<USubsystemActorManager>():nullptr;
 return Manager?Manager->GetSubsystemActor<ALogisticsSubsystem>():nullptr;
}
AFGBuildableDockingStation* FILNSelectorStation(UObject* Object) {
 if(!IsValid(Object)) return nullptr;
 if(auto* Station=Cast<AFGBuildableDockingStation>(Object)) return Station;
 if(auto* ID=Cast<AFGDockingStationIdentifier>(Object)) return ID->GetStation();
 if(auto* Component=Cast<UActorComponent>(Object)) return Cast<AFGBuildableDockingStation>(Component->GetOwner());
 if(auto* Hit=Cast<UFGHitResultWrapper>(Object)) return Cast<AFGBuildableDockingStation>(Hit->mHitResult.GetActor());
 return nullptr;
}
UVerticalBox* FILNSelectorContainer(UFGInteractWidget* Widget) {
 if(!IsValid(Widget) || Widget->GetClass()->GetPathName()!=FILNDockingWidget) return nullptr;
 return Cast<UVerticalBox>(Widget->GetWidgetFromName(TEXT("mInventoryContainer")));
}
UFILNResourceSelector* FILNFindSelector(UVerticalBox* Container) {
 if(!Container) return nullptr;
 for(int32 I=0;I<Container->GetChildrenCount();++I)
  if(auto* Found=Cast<UFILNResourceSelector>(Container->GetChildAt(I))) return Found;
 return nullptr;
}
bool FILNParameterCount(UFunction* Function,int32 Count) {
 if(!Function) return false;
 int32 Actual=0;
 for(TFieldIterator<FProperty> It(Function);It;++It) if(It->HasAnyPropertyFlags(CPF_Parm)) ++Actual;
 return Actual==Count;
}
template<class PropertyType,class ValueType>
bool FILNCallInput(UObject* Target,const TCHAR* FunctionName,const TCHAR* ParameterName,const ValueType& Value) {
 auto* Function=Target?Target->FindFunction(FunctionName):nullptr;
 auto* Property=Function?FindFProperty<PropertyType>(Function,ParameterName):nullptr;
 if(!FILNParameterCount(Function,1) || !Property || !Property->HasAnyPropertyFlags(CPF_Parm) || Property->HasAnyPropertyFlags(CPF_OutParm|CPF_ReturnParm)) return false;
 FStructOnScope Parameters(Function);
 Property->SetPropertyValue_InContainer(Parameters.GetStructMemory(),Value);
 Target->ProcessEvent(Function,Parameters.GetStructMemory());
 return true;
}
bool FILNCallNoInput(UObject* Target,const TCHAR* FunctionName) {
 auto* Function=Target?Target->FindFunction(FunctionName):nullptr;
 if(!FILNParameterCount(Function,0)) return false;
 Target->ProcessEvent(Function,nullptr); return true;
}
bool FILNClassArrayParameter(UObject* Target,const TCHAR* FunctionName,const TCHAR* ParameterName,bool Output,
 TArray<TSubclassOf<UFGItemDescriptor>>& Values) {
 auto* Function=Target?Target->FindFunction(FunctionName):nullptr;
 auto* Array=Function?FindFProperty<FArrayProperty>(Function,ParameterName):nullptr;
 auto* Inner=Array?CastField<FClassProperty>(Array->Inner):nullptr;
 if(!FILNParameterCount(Function,1) || !Array || !Inner || Inner->MetaClass!=UFGItemDescriptor::StaticClass() ||
    !Array->HasAnyPropertyFlags(CPF_Parm) || Array->HasAnyPropertyFlags(CPF_ReturnParm)) return false;
 // SetDescriptors is a by-reference array input (OutParm + ReferenceParm
 // in the serialized BP metadata); GetList is an output-only array.
 const bool Reference=Array->HasAnyPropertyFlags(CPF_ReferenceParm);
 const bool Out=Array->HasAnyPropertyFlags(CPF_OutParm);
 if(Output?(!Out || Reference):(Out && !Reference)) return false;
 FStructOnScope Parameters(Function);
 FScriptArrayHelper Data(Array,Array->ContainerPtrToValuePtr<void>(Parameters.GetStructMemory()));
 if(!Output) {
  for(const auto& Resource:Values) Inner->SetObjectPropertyValue(Data.GetRawPtr(Data.AddValue()),Resource.Get());
 }
 Target->ProcessEvent(Function,Parameters.GetStructMemory());
 if(Output) {
  Values.Reset();
  if(Data.Num()>1) return false;
  for(int32 I=0;I<Data.Num();++I) {
   auto* Resource=Cast<UClass>(Inner->GetObjectPropertyValue(Data.GetRawPtr(I)));
   if(!Resource || !Resource->IsChildOf(UFGItemDescriptor::StaticClass())) return false;
   Values.Add(Resource);
  }
 }
 return true;
}
bool FILNBindEvent(UObject* Target,const TCHAR* Name,UObject* Receiver,FName FunctionName,int32 Arguments) {
 auto* Property=Target?FindFProperty<FMulticastInlineDelegateProperty>(Target->GetClass(),Name):nullptr;
 if(!Property || !FILNParameterCount(Property->SignatureFunction,Arguments)) return false;
 if(Arguments==1) {
  const auto* Index=FindFProperty<FIntProperty>(Property->SignatureFunction,TEXT("ButtonIndex"));
  if(!Index || !Index->HasAnyPropertyFlags(CPF_Parm) || Index->HasAnyPropertyFlags(CPF_OutParm|CPF_ReturnParm)) return false;
 }
 auto* Event=Property->ContainerPtrToValuePtr<FMulticastScriptDelegate>(Target);
 FScriptDelegate Delegate; Delegate.BindUFunction(Receiver,FunctionName); Event->AddUnique(Delegate);
 return true;
}
bool FILNGenerateTabs(UUserWidget* Tabs) {
 auto* Array=Tabs?FindFProperty<FArrayProperty>(Tabs->GetClass(),TEXT("mButtons")):nullptr;
 auto* Inner=Array?CastField<FStructProperty>(Array->Inner):nullptr;
 if(!Inner || !Inner->Struct || Inner->Struct->GetPathName()!=FILNTabStruct) return false;
 auto* Label=FindFProperty<FTextProperty>(Inner->Struct,TEXT("Text_5_FD671CE446AF05CA543E228C049AB0F9"));
 if(!Label) return false;
 FScriptArrayHelper Buttons(Array,Array->ContainerPtrToValuePtr<void>(Tabs));
 Buttons.EmptyValues();
 for(const FText& Text:{NSLOCTEXT("FILN","StorageTab","STORAGE"),NSLOCTEXT("FILN","FiltersTab","FILTERS")})
  Label->SetPropertyValue_InContainer(Buttons.GetRawPtr(Buttons.AddValue()),Text);
 return FILNCallNoInput(Tabs,TEXT("GenerateButtons"));
}
bool FILNSelectorMedium(AFGBuildableDockingStation* Station,TSubclassOf<UFGItemDescriptor> Resource) {
 if(!Station || !Resource) return false;
 const auto Form=UFGItemDescriptor::GetForm(Resource), Medium=Station->GetDockingStationResourceForm();
 return (Medium==EResourceForm::RF_SOLID && Form==EResourceForm::RF_SOLID) ||
   ((Medium==EResourceForm::RF_LIQUID || Medium==EResourceForm::RF_GAS) &&
    (Form==EResourceForm::RF_LIQUID || Form==EResourceForm::RF_GAS));
}
}

void UFILNResourceUIRegistration::Initialize(FSubsystemCollectionBase& Collection) {
 Super::Initialize(Collection);
 Collection.InitializeDependency<URemoteCallObjectRegistry>();
 auto* Registry=GetGameInstance()->GetSubsystem<URemoteCallObjectRegistry>();
 if(Registry) Registry->RegisterRemoteCallObject(UFILNResourceRemote::StaticClass());
 else UE_LOG(LogFILN,Error,TEXT("FILN_SELECTOR_RCO_MISSING"));
}
void UFILNResourceRemote::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
 Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(UFILNResourceRemote,NetReady);
}
uint32 UFILNResourceRemote::NextRequestId() {
 if(++ClientSequence==0) ++ClientSequence;
 return ClientSequence;
}
void UFILNResourceRemote::QueryResource_Implementation(AFGBuildableDockingStation* Target,uint32 RequestId) {
 HandleResource(Target,RequestId,false,nullptr);
}
void UFILNResourceRemote::SelectResource_Implementation(AFGBuildableDockingStation* Target,uint32 RequestId,TSubclassOf<UFGItemDescriptor> Resource) {
 HandleResource(Target,RequestId,true,Resource);
}
void UFILNResourceRemote::HandleResource(AFGBuildableDockingStation* Target,uint32 RequestId,bool Mutate,TSubclassOf<UFGItemDescriptor> Resource) {
 auto* World=GetWorld(); auto* Player=GetOwnerPlayerCharacter();
 auto* ID=IsValid(Target)?Target->GetStationIdentifier():nullptr;
 auto* S=FILNSelectorSubsystem(World);
 if(!RequestId || !World || !IsValid(Player) || !IsValid(Target) || Target->GetWorld()!=World || !Target->HasAuthority() ||
    !IsValid(ID) || ID->GetStation()!=Target || FVector::DistSquared(Player->GetActorLocation(),Target->GetActorLocation())>FMath::Square(2000.f) ||
    !IsValid(S) || !S->bRegistryValidated || S->bDispatchBlocked || S->bPreparingUninstall || S->bUninstallPrepared) {
  UE_LOG(LogFILN,Warning,TEXT("FILN_SELECTOR_REJECTED invalid context or registry"));
  ResourceReply(Target,RequestId,false,false,nullptr,NSLOCTEXT("FILN","SelectorInvalid","FILN registry or station interaction unavailable"));
  return;
 }
 const auto Current=[&]() { return S->StationConfigs.FindByPredicate([ID](const FFILNStationConfig& C){return C.Station==ID;}); };
 const auto* Before=Current();
 const bool Editable=!Target->GetIsInLoadMode() && !S->IsDepot(ID) && (!Before || !Before->ExternalFilter);
 bool Accepted=!Mutate && Editable;
 FText Message=Editable?NSLOCTEXT("FILN","SelectorReady","Select one resource; changes are applied immediately"):
  NSLOCTEXT("FILN","SelectorNotEditable","Only a valid ordinary UNLOAD station can be configured");
 if(Mutate && Editable) {
  const double Now=FPlatformTime::Seconds();
  auto* Recipes=AFGRecipeManager::Get(World);
  if(Now-LastMutation<0.2) Message=NSLOCTEXT("FILN","SelectorRate","Selection rejected: request too soon");
  else {
   LastMutation=Now;
   if(Resource && (!Recipes || !Recipes->IsItemDescriptorAvailable(Resource) || !S->StationAcceptsResource(Target,Resource)))
    Message=NSLOCTEXT("FILN","SelectorIncompatible","Selection rejected: unavailable or incompatible resource");
   else {
    const bool Changed=Resource?S->ConfigureStation(ID,Resource):S->RemoveStation(ID);
    const auto* After=Current();
    Accepted=filn::selector::Confirmed(Changed,!Resource,After!=nullptr,After && S->StationResourceMatches(*After),After && After->Resource==Resource);
    if(Changed && !Accepted) {
     S->RegistryDiagnostics.AddUnique(FString::Printf(TEXT("Selector readback failed: %s"),*Target->GetPathName()));
     UE_LOG(LogFILN,Error,TEXT("FILN_SELECTOR_READBACK_FAILED %s"),*Target->GetPathName());
    }
    Message=Accepted?NSLOCTEXT("FILN","SelectorApplied","Resource selection confirmed by native inventory"):
      NSLOCTEXT("FILN","SelectorRejected","Selection rejected: empty, undocked, unreserved and compatible station required; check native filters");
   }
  }
 }
 const auto* After=Current();
 const bool Verified=!After || S->StationResourceMatches(*After);
 const bool CanEdit=Editable && !S->bPreparingUninstall;
 if(!Verified) { Accepted=false; Message=NSLOCTEXT("FILN","SelectorReadback","Native filter readback failed; configuration is not verified"); }
 if(Mutate) UE_LOG(LogFILN,Log,TEXT("FILN_SELECTOR_%s station=%s resource=%s"),Accepted?TEXT("CONFIRMED"):TEXT("REJECTED"),*Target->GetPathName(),*GetPathNameSafe(Resource.Get()));
 ResourceReply(Target,RequestId,Accepted,CanEdit,After?After->Resource:TSubclassOf<UFGItemDescriptor>(),Message);
}
void UFILNResourceRemote::ResourceReply_Implementation(AFGBuildableDockingStation* Target,uint32 RequestId,bool Accepted,bool Editable,TSubclassOf<UFGItemDescriptor> Resource,const FText& Message) {
 if(Receiver.IsValid()) Receiver->Reply(Target,RequestId,Accepted,Editable,Resource,Message);
}

UFILNResourceRemote* UFILNResourceSelector::Remote() const {
 auto* PC=Cast<AFGPlayerController>(GetOwningPlayer());
 return PC?PC->GetRemoteCallObjectOfClass<UFILNResourceRemote>():nullptr;
}
bool UFILNResourceSelector::BuildControls() {
 auto* PC=GetOwningPlayer();
 auto* TabClass=LoadClass<UUserWidget>(nullptr,FILNTabWidget);
 auto* PartClass=LoadClass<UUserWidget>(nullptr,FILNPartsWidget);
 if(!WidgetTree || !PC || !TabClass || !PartClass) return false;
 auto* Root=WidgetTree->ConstructWidget<UVerticalBox>(); WidgetTree->RootWidget=Root;
 Tabs=CreateWidget<UUserWidget>(PC,TabClass);
 Parts=CreateWidget<UUserWidget>(PC,PartClass);
 Feedback=WidgetTree->ConstructWidget<UTextBlock>();
 if(!Tabs || !Parts || !Feedback) return false;
 Feedback->SetAutoWrapText(true);
 Root->AddChildToVerticalBox(Tabs);
 Root->AddChildToVerticalBox(Parts);
 Root->AddChildToVerticalBox(Feedback);
 Parts->SetVisibility(ESlateVisibility::Collapsed); Feedback->SetVisibility(ESlateVisibility::Collapsed);
 return true;
}
bool UFILNResourceSelector::HasContext(AFGBuildableDockingStation* Target) const { return Station.Get()==Target && bInitialized; }
void UFILNResourceSelector::SetContext(AFGBuildableDockingStation* Target,UVerticalBox* Container) {
 ClearContext();
 if(!IsValid(Target) || !IsValid(Container) || !Tabs || !Parts || !Feedback) return;
 Station=Target; InventoryContainer=Container; SetVisibility(ESlateVisibility::Visible);
 TGuardValue<bool> Guard(bSynchronizing,true);
 if(!FILNBindEvent(Tabs,TEXT("OnButtonClicked"),this,GET_FUNCTION_NAME_CHECKED(UFILNResourceSelector,NativeTabClicked),1) ||
    !FILNBindEvent(Parts,TEXT("OnListChanged"),this,GET_FUNCTION_NAME_CHECKED(UFILNResourceSelector,NativeListChanged),0) ||
    !FILNCallInput<FIntProperty>(Parts,TEXT("SetMaxItemAmount"),TEXT("mMaxItemAmount"),1) ||
    !FILNCallInput<FTextProperty>(Parts,TEXT("SetName"),TEXT("mName"),NSLOCTEXT("FILN","ResourceLabel","Requested resource")) ||
    !FILNGenerateTabs(Tabs)) {
  Fail(NSLOCTEXT("FILN","SelectorContract","Native FILTERS widget contract mismatch")); return;
 }
 bInitialized=true;
 UE_LOG(LogFILN,Log,TEXT("FILN_SELECTOR_ATTACHED station=%s; selection not yet verified"),*Target->GetPathName());
}
void UFILNResourceSelector::RestoreStorage() {
 for(const auto& Entry:StorageVisibility) if(Entry.Key.IsValid()) Entry.Key->SetVisibility(Entry.Value);
 StorageVisibility.Reset();
}
void UFILNResourceSelector::ClearContext() {
 RestoreStorage();
 if(auto* R=Remote();R && R->Receiver.Get()==this) R->Receiver.Reset();
 PendingRequest=0; Station.Reset(); InventoryContainer.Reset(); bFilterOpen=false; bEditable=false; bInitialized=false;
 if(Parts) { Parts->SetIsEnabled(false); Parts->SetVisibility(ESlateVisibility::Collapsed); }
 if(Feedback) Feedback->SetVisibility(ESlateVisibility::Collapsed);
 SetVisibility(ESlateVisibility::Collapsed);
}
void UFILNResourceSelector::NativeDestruct() {
 ClearContext(); Super::NativeDestruct();
}
void UFILNResourceSelector::Fail(const FText& Reason) {
 bEditable=false; PendingRequest=0;
 if(Parts) Parts->SetIsEnabled(false);
 if(Feedback) { Feedback->SetVisibility(ESlateVisibility::Visible); Feedback->SetText(Reason); }
 UE_LOG(LogFILN,Error,TEXT("FILN_SELECTOR_ERROR %s"),*Reason.ToString());
}
void UFILNResourceSelector::NativeTabClicked(int32 ButtonIndex) {
 if(bSynchronizing || !bInitialized || !Station.IsValid()) return;
 if(ButtonIndex!=0 && ButtonIndex!=1) { Fail(NSLOCTEXT("FILN","BadTab","Unexpected native tab index")); return; }
 if(ButtonIndex==0) {
  RestoreStorage(); bFilterOpen=false; bEditable=false; PendingRequest=0;
  Parts->SetVisibility(ESlateVisibility::Collapsed); Feedback->SetVisibility(ESlateVisibility::Collapsed); return;
 }
 auto* Container=InventoryContainer.Get();
 if(!Container) { Fail(NSLOCTEXT("FILN","MissingContainer","Native storage container no longer available")); return; }
 if(!bFilterOpen) for(int32 I=0;I<Container->GetChildrenCount();++I) {
  auto* Child=Container->GetChildAt(I);
  if(Child && Child!=this) { StorageVisibility.Emplace(Child,Child->GetVisibility()); Child->SetVisibility(ESlateVisibility::Collapsed); }
 }
 bFilterOpen=true; Parts->SetVisibility(ESlateVisibility::Visible); Feedback->SetVisibility(ESlateVisibility::Visible);
 Query();
}
void UFILNResourceSelector::Query() {
 bEditable=false; Parts->SetIsEnabled(false); Feedback->SetText(NSLOCTEXT("FILN","SelectorQuery","Reading authoritative resource selection"));
 auto* R=Remote();
 if(!R || !Station.IsValid()) { Fail(NSLOCTEXT("FILN","MissingRemote","Native resource server unavailable")); return; }
 R->Receiver=this; PendingRequest=R->NextRequestId(); R->QueryResource(Station.Get(),PendingRequest);
}
bool UFILNResourceSelector::ShowSelection(TSubclassOf<UFGItemDescriptor> Resource) {
 TGuardValue<bool> Guard(bSynchronizing,true);
 TArray<TSubclassOf<UFGItemDescriptor>> Values;
 if(Resource) Values.Add(Resource);
 if(!FILNClassArrayParameter(Parts,TEXT("SetDescriptors"),TEXT("mDescriptors"),false,Values)) return false;
 TArray<TSubclassOf<UFGItemDescriptor>> Actual;
 return FILNClassArrayParameter(Parts,TEXT("GetList"),TEXT("Items"),true,Actual) && Actual==Values;
}
void UFILNResourceSelector::Reply(AFGBuildableDockingStation* Target,uint32 RequestId,bool Accepted,bool Editable,TSubclassOf<UFGItemDescriptor> Resource,const FText& Message) {
 if(!bFilterOpen || !filn::selector::ReplyMatches(PendingRequest,RequestId,Station.Get()==Target)) return;
 PendingRequest=0;
 if(!ShowSelection(Resource)) { Fail(NSLOCTEXT("FILN","SelectorUIReadback","Native selector failed to display authoritative resource")); return; }
 bEditable=Editable; Parts->SetIsEnabled(Editable); Feedback->SetText(Message);
 if(!Accepted) UE_LOG(LogFILN,Warning,TEXT("FILN_SELECTOR_REPLY_REJECTED %s"),*Message.ToString());
}
void UFILNResourceSelector::NativeListChanged() {
 if(bSynchronizing || !bFilterOpen || !bEditable || PendingRequest || !Station.IsValid()) return;
 TArray<TSubclassOf<UFGItemDescriptor>> Values;
 if(!FILNClassArrayParameter(Parts,TEXT("GetList"),TEXT("Items"),true,Values)) {
  Fail(NSLOCTEXT("FILN","SelectorListInvalid","Native selection must contain zero or one valid item")); return;
 }
 const auto Resource=Values.IsEmpty()?TSubclassOf<UFGItemDescriptor>():Values[0];
 if(Resource && !FILNSelectorMedium(Station.Get(),Resource)) {
  Query(); Feedback->SetText(NSLOCTEXT("FILN","SelectorWrongMedium","Incompatible resource rejected; reading previous selection")); return;
 }
 auto* R=Remote();
 if(!R) { Fail(NSLOCTEXT("FILN","MissingRemote2","Native resource server unavailable")); return; }
 bEditable=false; Parts->SetIsEnabled(false);
 Feedback->SetText(NSLOCTEXT("FILN","SelectorPending","Waiting for native inventory confirmation"));
 R->Receiver=this; PendingRequest=R->NextRequestId(); R->SelectResource(Station.Get(),PendingRequest,Resource);
}

namespace {
void FILNAttachSelector(UFGInteractWidget* Widget,UObject* Object) {
 if(!IsInGameThread() || !IsValid(Widget) || Widget->GetClass()->GetPathName()!=FILNDockingWidget) return;
 auto* PC=Widget->GetOwningPlayer(); auto* Target=FILNSelectorStation(Object);
 if(!IsValid(PC) || !PC->IsLocalController() || !IsValid(Target) || Target->GetWorld()!=Widget->GetWorld()) return;
 auto* Container=FILNSelectorContainer(Widget);
 if(!Container) { UE_LOG(LogFILN,Error,TEXT("FILN_SELECTOR_ERROR native mInventoryContainer missing or not VerticalBox")); return; }
 if(auto* Existing=FILNFindSelector(Container)) {
  if(Existing->HasContext(Target)) return;
  Existing->ClearContext(); Existing->RemoveFromParent();
 }
 auto* Selector=CreateWidget<UFILNResourceSelector>(PC);
 if(!Selector || !Selector->BuildControls() || !Container->AddChildToVerticalBox(Selector)) {
  UE_LOG(LogFILN,Error,TEXT("FILN_SELECTOR_ERROR native controls could not be attached")); return;
 }
 Selector->SetContext(Target,Container);
}
}
void UFILNResourceSelector::InstallHook() {
 if(WITH_EDITOR) return;
 SUBSCRIBE_METHOD_AFTER(AFGHUD::RequestInteractWidget,[](UFGInteractWidget* Widget,AFGHUD*,TSubclassOf<UFGInteractWidget>,UObject* Object) {
  FILNAttachSelector(Widget,Object);
 });
 SUBSCRIBE_METHOD_AFTER(UFGInteractWidget::Native_Init,[](UFGInteractWidget* Widget) {
  if(IsValid(Widget)) FILNAttachSelector(Widget,Widget->mInteractObject.Get());
 });
 SUBSCRIBE_METHOD_AFTER(UFGGameUI::RemoveInteractWidget,[](UFGGameUI*,UFGInteractWidget* Widget) {
  if(auto* Selector=FILNFindSelector(FILNSelectorContainer(Widget))) { Selector->ClearContext(); Selector->RemoveFromParent(); }
 });
}
