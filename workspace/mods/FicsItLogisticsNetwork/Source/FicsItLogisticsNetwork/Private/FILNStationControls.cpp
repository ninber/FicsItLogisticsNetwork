#include "FILNStationControls.h"
#include "FILNBlueprintSettings.h"
#include "FILNSelectorPolicy.h"
#include "LogisticsSubsystem.h"
#include "FGHUD.h"
#include "FGPlayerController.h"
#include "FGCharacterPlayer.h"
#include "FGRecipeManager.h"
#include "FGRecipe.h"
#include "Resources/FGItemDescriptor.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "Registry/RemoteCallObjectRegistry.h"
#include "Subsystem/SubsystemActorManager.h"
#include "UI/FGInteractWidget.h"
#include "UI/FGGameUI.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "Brushes/SlateColorBrush.h"
#include "Styling/CoreStyle.h"
#include "UObject/UnrealType.h"
#include "UObject/StructOnScope.h"
#include "Net/UnrealNetwork.h"
#include "Patching/NativeHookManager.h"
#include "HAL/PlatformTime.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

namespace {
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
bool FILNSelectorIsDepot(AFGBuildableDockingStation* Target) {
 if(!IsValid(Target)) return false;
 const FString Class=Target->GetClass()->GetPathName();
 const auto Recipe=Target->GetBuiltWithRecipe();
 return Class==TEXT("/FicsItLogisticsNetwork/Depot/Build_FILNDepot.Build_FILNDepot_C") ||
  Class==TEXT("/FicsItLogisticsNetwork/Depot/Build_FILNFluidDepot.Build_FILNFluidDepot_C") ||
  (Recipe && (Recipe->GetPathName()==TEXT("/FicsItLogisticsNetwork/Depot/Recipe_FILNDepot.Recipe_FILNDepot_C") ||
              Recipe->GetPathName()==TEXT("/FicsItLogisticsNetwork/Depot/Recipe_FILNFluidDepot.Recipe_FILNFluidDepot_C")));
}
// Resolve the actual inventory container by instance or reflected property.
 // Do not reject a solid/fluid widget merely because its BP class path differs.
 UVerticalBox* FILNNativeStorageContainer(UFGInteractWidget* Widget) {
  if(!IsValid(Widget)) return nullptr;
  if(auto* Box=Cast<UVerticalBox>(Widget->GetWidgetFromName(TEXT("mInventoryContainer")))) return Box;
  if(auto* Prop=FindFProperty<FObjectPropertyBase>(Widget->GetClass(),TEXT("mInventoryContainer")))
   if(auto* Box=Cast<UVerticalBox>(Prop->GetObjectPropertyValue_InContainer(Widget))) return Box;
  return nullptr;
 }
 // Fluid interaction widgets need not expose the solid inventory box.
 // Prefer a shallow native vertical layout and keep all its children visible.
 UVerticalBox* FILNSelectorContainer(UFGInteractWidget* Widget) {
  if(auto* Box=FILNNativeStorageContainer(Widget)) return Box;
  if(!IsValid(Widget)) return nullptr;
  TArray<UWidget*> Queue;
  if(auto* Root=Widget->GetRootWidget()) Queue.Add(Root);
  for(int32 I=0;I<Queue.Num();++I) {
   auto* Child=Queue[I];
   if(auto* Box=Cast<UVerticalBox>(Child)) return Box;
   if(auto* Panel=Cast<UPanelWidget>(Child))
    for(int32 J=0;J<Panel->GetChildrenCount();++J)
     if(auto* Item=Panel->GetChildAt(J)) Queue.Add(Item);
  }
  return nullptr;
 }
 UFILNResourceSelector* FILNFindSelector(UVerticalBox* Container) {
  if(!Container) return nullptr;
  for(int32 I=0;I<Container->GetChildrenCount();++I)
   if(auto* Found=Cast<UFILNResourceSelector>(Container->GetChildAt(I))) return Found;
  return nullptr;
 }
 FLinearColor FILNFilterColor(const TCHAR* Hex) { return FLinearColor::FromSRGBColor(FColor::FromHex(Hex)); }
 UTextBlock* FILNFilterLabel(UWidgetTree* Tree,const FText& Text,int32 Size=14) {
  auto* Label=Tree->ConstructWidget<UTextBlock>(); Label->SetText(Text);
  Label->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"),Size));
  Label->SetColorAndOpacity(FSlateColor(FILNFilterColor(TEXT("E8EBED"))));
  return Label;
 }
 void FILNFilterTabStyle(UButton* Button,bool Active) {
  FButtonStyle Style;
  Style.SetNormal(FSlateColorBrush(FILNFilterColor(Active?TEXT("EFA248"):TEXT("32383D"))));
  Style.SetHovered(FSlateColorBrush(FILNFilterColor(Active?TEXT("FFC071"):TEXT("46515A"))));
  Style.SetPressed(FSlateColorBrush(FILNFilterColor(TEXT("BD772E"))));
  Style.SetNormalPadding(FMargin(12,7)); Style.SetPressedPadding(FMargin(12,7));
  Button->SetStyle(Style);
  if(auto* Label=Cast<UTextBlock>(Button->GetChildAt(0))) Label->SetColorAndOpacity(FSlateColor(FILNFilterColor(Active?TEXT("171C20"):TEXT("E8EBED"))));
 }
bool FILNSelectorMedium(AFGBuildableDockingStation* Station,TSubclassOf<UFGItemDescriptor> Resource) {
 if(!Station || !Resource) return false;
 const auto Form=UFGItemDescriptor::GetForm(Resource);
 auto Medium=Station->GetDockingStationResourceForm();
 // Designer instances can precede initialization of native storage.
 if(FFILNStationSettings::IsDedicated(Station))
  Medium=Station->GetClass()->GetPathName().Contains(TEXT("Build_FILNFluidStation"))?EResourceForm::RF_LIQUID:EResourceForm::RF_SOLID;
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
 auto* S=FILNSelectorSubsystem(World);
 const bool Valid=RequestId && World && IsValid(Player) && IsValid(Target) && Target->GetWorld()==World && Target->HasAuthority() &&
  FVector::DistSquared(Player->GetActorLocation(),Target->GetActorLocation())<=FMath::Square(2000.f) &&
  !FILNSelectorIsDepot(Target);
 bool Accepted=Valid;
 if(Valid && Mutate) {
  // Validate only the choice's type, never current stock or operational readiness.
  Accepted=!Resource || FILNSelectorMedium(Target,Resource);
  if(Accepted) {
   if(FFILNStationSettings::HasProperty(Target)) {
    Accepted=FFILNStationSettings::Write(Target,Resource);
    if(Accepted && IsValid(S)) S->ResetScan();
   } else Accepted=IsValid(S) && S->StoreDesiredStationResource(Target,Resource);
  }
 }
 if(Valid && !Mutate && !FFILNStationSettings::HasProperty(Target) && (!IsValid(S) || Target->IsBuildableInsideBlueprintDesigner())) Accepted=false;
 const auto Desired=Valid?(FFILNStationSettings::HasProperty(Target)?FFILNStationSettings::Read(Target):
  (IsValid(S)?S->DesiredStationResource(Target):TSubclassOf<UFGItemDescriptor>())):TSubclassOf<UFGItemDescriptor>();
 if(!Accepted) UE_LOG(LogFILN,Warning,TEXT("FILN_DESIRED_RESOURCE_NOT_STORED station=%s mutate=%d property=%d"),*GetPathNameSafe(Target),Mutate,FFILNStationSettings::HasProperty(Target));
 ResourceReply(Target,RequestId,Accepted,true,Desired,FText::GetEmpty());
}
void UFILNResourceRemote::ResourceReply_Implementation(AFGBuildableDockingStation* Target,uint32 RequestId,bool Accepted,bool Editable,TSubclassOf<UFGItemDescriptor> Resource,const FText& Message) {
 if(Receiver.IsValid()) Receiver->Reply(Target,RequestId,Accepted,Editable,Resource,Message);
}

UFILNResourceRemote* UFILNResourceSelector::Remote() const {
 auto* PC=Cast<AFGPlayerController>(GetOwningPlayer());
 return PC?PC->GetRemoteCallObjectOfClass<UFILNResourceRemote>():nullptr;
}
bool UFILNResourceSelector::BuildControls() {
 if(!GetOwningPlayer()) return false;
 if(!WidgetTree) WidgetTree=NewObject<UWidgetTree>(this,TEXT("FILNFilterTree"));
 auto* Root=WidgetTree->ConstructWidget<UVerticalBox>(); WidgetTree->RootWidget=Root;
 auto* Bar=WidgetTree->ConstructWidget<UHorizontalBox>();
 StorageTab=WidgetTree->ConstructWidget<UButton>();
 FiltersTab=WidgetTree->ConstructWidget<UButton>();
 StorageTab->AddChild(FILNFilterLabel(WidgetTree,NSLOCTEXT("FILN","StorageTab","STORAGE")));
 FiltersTab->AddChild(FILNFilterLabel(WidgetTree,NSLOCTEXT("FILN","FiltersTab","FILTERS")));
 FSlateChildSize Fill(ESlateSizeRule::Fill);
 auto* Left=Bar->AddChildToHorizontalBox(StorageTab); Left->SetSize(Fill); Left->SetPadding(FMargin(0,0,3,0));
 Bar->AddChildToHorizontalBox(FiltersTab)->SetSize(Fill);
 StorageTab->OnClicked.AddDynamic(this,&UFILNResourceSelector::OpenStorage);
 FiltersTab->OnClicked.AddDynamic(this,&UFILNResourceSelector::OpenFilters);
 Root->AddChildToVerticalBox(Bar);
 Parts=WidgetTree->ConstructWidget<UComboBoxString>();
 Parts->SetMaxListHeight(360.f);
 FComboBoxStyle Combo=Parts->GetWidgetStyle();
 FComboButtonStyle ComboButton=Combo.ComboButtonStyle;
 FButtonStyle Button=ComboButton.ButtonStyle;
 Button.SetNormal(FSlateColorBrush(FILNFilterColor(TEXT("32383D"))));
 Button.SetHovered(FSlateColorBrush(FILNFilterColor(TEXT("46515A"))));
 Button.SetPressed(FSlateColorBrush(FILNFilterColor(TEXT("BD772E"))));
 ComboButton.SetButtonStyle(Button); Combo.SetComboButtonStyle(ComboButton); Parts->SetWidgetStyle(Combo);
 Parts->OnSelectionChanged.AddDynamic(this,&UFILNResourceSelector::NativeListChanged);
 Parts->OnGenerateWidgetEvent.BindDynamic(this,&UFILNResourceSelector::GenerateChoice);
 Feedback=FILNFilterLabel(WidgetTree, FText::GetEmpty(),13); Feedback->SetAutoWrapText(true);
 Root->AddChildToVerticalBox(Parts)->SetPadding(FMargin(0,8,0,4));
 Root->AddChildToVerticalBox(Feedback);
 Parts->SetVisibility(ESlateVisibility::Collapsed); Feedback->SetVisibility(ESlateVisibility::Collapsed);
 SetTabStyle(false);
 return true;
}
bool UFILNResourceSelector::HasContext(AFGBuildableDockingStation* Target) const { return Station.Get()==Target && bInitialized; }
void UFILNResourceSelector::SetTabStyle(bool Filters) {
 if(StorageTab) FILNFilterTabStyle(StorageTab,!Filters);
 if(FiltersTab) FILNFilterTabStyle(FiltersTab,Filters);
}
FString UFILNResourceSelector::ChoiceFor(TSubclassOf<UFGItemDescriptor> Resource) {
 if(!Resource) return TEXT("No filter");
 for(const auto& Entry:Choices) if(Entry.Value==Resource) return Entry.Key;
 FString Name=UFGItemDescriptor::GetItemName(Resource).ToString();
 if(Name.IsEmpty()) Name=Resource->GetName();
 if(Choices.Contains(Name)) Name+=TEXT(" [")+Resource->GetName()+TEXT("]");
 Choices.Add(Name,Resource); Parts->AddOption(Name); return Name;
}
void UFILNResourceSelector::PopulateChoices() {
 TGuardValue<bool> Guard(bSynchronizing,true);
 Choices.Reset(); Parts->ClearOptions();
 Choices.Add(TEXT("No filter"),TSubclassOf<UFGItemDescriptor>()); Parts->AddOption(TEXT("No filter"));
 auto* Recipes=AFGRecipeManager::Get(GetWorld());
 bChoicesReady=Recipes && Station.IsValid();
 if(!bChoicesReady) return;
 TArray<TSubclassOf<UFGItemDescriptor>> Available;
 for(const auto& Resource:Recipes->GetAllAvailableItemDescriptors())
  if(Resource && FILNSelectorMedium(Station.Get(),Resource)) Available.AddUnique(Resource);
 Available.Sort([](const TSubclassOf<UFGItemDescriptor>& A,const TSubclassOf<UFGItemDescriptor>& B) {
  return UFGItemDescriptor::GetItemName(A).ToString()<UFGItemDescriptor::GetItemName(B).ToString();
 });
 for(const auto& Resource:Available) ChoiceFor(Resource);
}
UWidget* UFILNResourceSelector::GenerateChoice(FString Item) {
 auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();
 if(const auto* Resource=Choices.Find(Item); Resource && *Resource) {
  if(auto* Icon=UFGItemDescriptor::GetSmallIcon(*Resource)) {
   auto* Size=WidgetTree->ConstructWidget<USizeBox>(); Size->SetWidthOverride(26); Size->SetHeightOverride(26);
   auto* Image=WidgetTree->ConstructWidget<UImage>(); Image->SetBrushFromTexture(Icon,true); Size->AddChild(Image);
   Row->AddChildToHorizontalBox(Size)->SetPadding(FMargin(0,0,8,0));
  }
 }
 Row->AddChildToHorizontalBox(FILNFilterLabel(WidgetTree,FText::FromString(Item)))->SetVerticalAlignment(VAlign_Center);
 return Row;
}
void UFILNResourceSelector::SetContext(AFGBuildableDockingStation* Target,UVerticalBox* Container,bool HideStorage) {
 ClearContext();
 if(!IsValid(Target) || !IsValid(Container) || !Parts || !Feedback) return;
 Station=Target; InventoryContainer=Container; bHideStorage=HideStorage; SetVisibility(ESlateVisibility::Visible);
 PopulateChoices(); ShowSelection(FFILNStationSettings::Read(Target)); SetTabStyle(false); bInitialized=true;
 UE_LOG(LogFILN,Log,TEXT("FILN_SELECTOR_ATTACHED station=%s"),*Target->GetPathName());
}
void UFILNResourceSelector::OpenStorage() { NativeTabClicked(0); }
void UFILNResourceSelector::OpenFilters() { NativeTabClicked(1); }
void UFILNResourceSelector::RestoreStorage() {
 for(const auto& Entry:StorageVisibility) if(Entry.Key.IsValid()) Entry.Key->SetVisibility(Entry.Value);
 StorageVisibility.Reset();
}
void UFILNResourceSelector::ClearContext() {
 RestoreStorage();
 StopRefresh();
 if(auto* R=Remote();R && R->Receiver.Get()==this) R->Receiver.Reset();
 PendingRequest=0; Station.Reset(); InventoryContainer.Reset(); bFilterOpen=false; bInitialized=false;
 bAwaitingSelection=false; bNeedsQuery=true; bChoicesReady=false; DesiredResource=nullptr;
 if(Parts) Parts->SetVisibility(ESlateVisibility::Collapsed);
 if(Feedback) Feedback->SetVisibility(ESlateVisibility::Collapsed);
 SetVisibility(ESlateVisibility::Collapsed);
}
void UFILNResourceSelector::NativeDestruct() {
 ClearContext(); Super::NativeDestruct();
}
void UFILNResourceSelector::Fail(const FText& Reason) {
 PendingRequest=0;
 UE_LOG(LogFILN,Warning,TEXT("FILN_SELECTOR_ERROR %s"),*Reason.ToString());
}
void UFILNResourceSelector::NativeTabClicked(int32 ButtonIndex) {
 if(bSynchronizing || !bInitialized || !Station.IsValid()) return;
 if(ButtonIndex!=0 && ButtonIndex!=1) { Fail(NSLOCTEXT("FILN","BadTab","Unexpected native tab index")); return; }
 SetTabStyle(ButtonIndex==1);
 if(ButtonIndex==0) {
  RestoreStorage(); StopRefresh(); bFilterOpen=false; PendingRequest=0;
  Parts->SetVisibility(ESlateVisibility::Collapsed); Feedback->SetVisibility(ESlateVisibility::Collapsed); return;
 }
 auto* Container=InventoryContainer.Get();
 if(!Container) { Fail(NSLOCTEXT("FILN","MissingContainer","Native storage container no longer available")); return; }
 // Re-clicking the active tab must not restart the query or close its dropdown.
 if(bFilterOpen) return;
 if(bHideStorage) for(int32 I=0;I<Container->GetChildrenCount();++I) {
  auto* Child=Container->GetChildAt(I);
  if(Child && Child!=this) { StorageVisibility.Emplace(Child,Child->GetVisibility()); Child->SetVisibility(ESlateVisibility::Collapsed); }
 }
 bFilterOpen=true; Parts->SetVisibility(ESlateVisibility::Visible); Feedback->SetVisibility(ESlateVisibility::Collapsed);
 if(auto* World=GetWorld()) World->GetTimerManager().SetTimer(RefreshTimer,this,&UFILNResourceSelector::RefreshIfBlocked,2.f,true);
 if(bAwaitingSelection) SendSelection(); else { bNeedsQuery=true; Query(); }
}
void UFILNResourceSelector::StopRefresh() {
 if(auto* World=GetWorld()) World->GetTimerManager().ClearTimer(RefreshTimer);
 RequestDeadline=0;
}
void UFILNResourceSelector::RefreshIfBlocked() {
 if(!bFilterOpen || !bInitialized || !Station.IsValid()) { StopRefresh(); return; }
 if(!Parts || Parts->IsOpen() || (PendingRequest && FPlatformTime::Seconds()<RequestDeadline)) return;
 PendingRequest=0;
 if(!bChoicesReady && AFGRecipeManager::Get(GetWorld())) {
  const FString Previous=Parts->GetSelectedOption();
  const auto* Entry=Choices.Find(Previous);
  const auto Selected=Entry?*Entry:TSubclassOf<UFGItemDescriptor>();
  PopulateChoices(); ShowSelection(Selected);
 }
 if(bAwaitingSelection) SendSelection();
 else if(bNeedsQuery) Query(true);
}
void UFILNResourceSelector::Query(bool Background) {
 auto* R=Remote();
 if(!R || !Station.IsValid()) return;
 R->Receiver=this; PendingRequest=R->NextRequestId(); RequestDeadline=FPlatformTime::Seconds()+5.;
 R->QueryResource(Station.Get(),PendingRequest);
}
void UFILNResourceSelector::SendSelection() {
 auto* R=Remote();
 if(!R || !Station.IsValid()) return;
 R->Receiver=this; PendingRequest=R->NextRequestId(); RequestDeadline=FPlatformTime::Seconds()+5.;
 R->SelectResource(Station.Get(),PendingRequest,DesiredResource);
}
bool UFILNResourceSelector::ShowSelection(TSubclassOf<UFGItemDescriptor> Resource) {
 TGuardValue<bool> Guard(bSynchronizing,true);
 const FString Name=ChoiceFor(Resource);
 if(Parts->GetSelectedOption()!=Name) Parts->SetSelectedOption(Name);
 return Parts->GetSelectedOption()==Name;
}
void UFILNResourceSelector::Reply(AFGBuildableDockingStation* Target,uint32 RequestId,bool Accepted,bool Editable,TSubclassOf<UFGItemDescriptor> Resource,const FText& Message) {
 if(!filn::selector::ReplyMatches(PendingRequest,RequestId,Station.Get()==Target)) return;
 PendingRequest=0;
 if(!Accepted) return; // retry the latest intent, never revert the visible selection
 if(bAwaitingSelection) {
  if(Resource==DesiredResource) { bAwaitingSelection=false; bNeedsQuery=false; }
  return;
 }
 if(Parts->IsOpen()) { bNeedsQuery=true; return; }
 bNeedsQuery=!ShowSelection(Resource);
}
void UFILNResourceSelector::NativeListChanged(FString SelectedItem,ESelectInfo::Type SelectionType) {
 if(bSynchronizing || !bFilterOpen || !Station.IsValid() || SelectionType==ESelectInfo::Direct) return;
 const auto* Choice=Choices.Find(SelectedItem);
 if(!Choice || (*Choice && !FILNSelectorMedium(Station.Get(),*Choice))) return;
 DesiredResource=*Choice; bAwaitingSelection=true; bNeedsQuery=false; PendingRequest=0;
 SendSelection();
}

namespace {
void FILNAttachSelector(UFGInteractWidget* Widget,UObject* Object) {
 if(!IsInGameThread() || !IsValid(Widget)) return;
 auto* PC=Widget->GetOwningPlayer(); auto* Target=FILNSelectorStation(Object);
 if(!Target) Target=FILNSelectorStation(Widget->mInteractObject.Get());
 if(!IsValid(PC) || !PC->IsLocalController() || !IsValid(Target) || Target->GetWorld()!=Widget->GetWorld()) return;
 auto* Container=FILNSelectorContainer(Widget);
 if(!Container) { UE_LOG(LogFILN,Error,TEXT("FILN_SELECTOR_ERROR class=%s native mInventoryContainer unavailable"),*Widget->GetClass()->GetPathName()); return; }
 if(auto* Existing=FILNFindSelector(Container)) {
  if(!FILNSelectorIsDepot(Target) && Existing->HasContext(Target)) return;
  Existing->ClearContext(); Existing->RemoveFromParent();
 }
 if(FILNSelectorIsDepot(Target)) return;
 auto* Selector=CreateWidget<UFILNResourceSelector>(PC);
 if(!Selector || !Selector->BuildControls() || !Container->AddChildToVerticalBox(Selector)) {
  UE_LOG(LogFILN,Error,TEXT("FILN_SELECTOR_ERROR native controls could not be attached")); return;
 }
 Selector->SetContext(Target,Container,Container==FILNNativeStorageContainer(Widget));
}
}
void UFILNResourceSelector::InstallHook() {
 if(WITH_EDITOR) return;
 SUBSCRIBE_METHOD_AFTER(AFGHUD::RequestInteractWidget,([](UFGInteractWidget* Widget,AFGHUD*,TSubclassOf<UFGInteractWidget>,UObject* Object) {
  FILNAttachSelector(Widget,Object);
  // RequestInteractWidget can run before the BP has built/pushed its tree.
  // Retry once after native UI initialization, only on the same active station.
  auto* Resolved=FILNSelectorStation(Object);
  if(!Resolved && IsValid(Widget)) Resolved=FILNSelectorStation(Widget->mInteractObject.Get());
  if(IsValid(Widget) && IsValid(Resolved) && Widget->GetWorld()) {
   const TWeakObjectPtr<UFGInteractWidget> Weak(Widget);
   const TWeakObjectPtr<AFGBuildableDockingStation> Target(Resolved);
   Widget->GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([Weak,Target]() {
    auto* W=Weak.Get(); auto* PC=IsValid(W)?W->GetOwningPlayer():nullptr;
    auto* HUD=PC?Cast<AFGHUD>(PC->GetHUD()):nullptr; auto* UI=HUD?HUD->GetGameUI():nullptr;
    if(IsValid(UI) && UI->GetInteractWidgetStack().Contains(W) && Target.IsValid()) FILNAttachSelector(W,Target.Get());
   }));
  }
 }));
 SUBSCRIBE_METHOD_AFTER(UFGInteractWidget::Native_Init,[](UFGInteractWidget* Widget) {
  if(IsValid(Widget)) FILNAttachSelector(Widget,Widget->mInteractObject.Get());
 });
 SUBSCRIBE_METHOD_AFTER(UFGGameUI::RemoveInteractWidget,[](UFGGameUI*,UFGInteractWidget* Widget) {
  if(auto* Selector=FILNFindSelector(FILNSelectorContainer(Widget))) { Selector->ClearContext(); Selector->RemoveFromParent(); }
 });
}
