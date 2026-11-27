#include "FILNOrganizer.h"
#include "FILNOrganizerPolicy.h"
#include "LogisticsSubsystem.h"
#include "FGCharacterPlayer.h"
#include "FGHUD.h"
#include "UI/FGGameUI.h"
#include "Resources/FGItemDescriptor.h"
#include "Subsystem/SubsystemActorManager.h"
#include "ModLoading/PluginModuleLoader.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/InputComponent.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Brushes/SlateColorBrush.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/PlatformTime.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "Misc/DateTime.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/IInputProcessor.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Console.h"
#include "Widgets/SViewport.h"

namespace {
using FILNORow=const FJsonObject*;
using FILNOArray=TArray<TSharedPtr<FJsonValue>>;
FDelegateHandle FILNOInputHandle;
TSharedPtr<IInputProcessor> FILNOInputProcessor;
struct FFILNOShortcut {
 FKey Key=EKeys::L;
 bool Ctrl=true,Shift=true,Alt=false;
};
FFILNOShortcut FILNOReadShortcut() {
 FString KeyName=TEXT("L"); FFILNOShortcut Shortcut;
 if(GConfig) {
  GConfig->GetString(TEXT("FILN.Organizer"),TEXT("Key"),KeyName,GGameIni);
  GConfig->GetBool(TEXT("FILN.Organizer"),TEXT("Ctrl"),Shortcut.Ctrl,GGameIni);
  GConfig->GetBool(TEXT("FILN.Organizer"),TEXT("Shift"),Shortcut.Shift,GGameIni);
  GConfig->GetBool(TEXT("FILN.Organizer"),TEXT("Alt"),Shortcut.Alt,GGameIni);
 }
 Shortcut.Key=FKey(FName(*KeyName));
 return Shortcut;
}
class FFILNOInputProcessor final : public IInputProcessor {
 FFILNOShortcut Shortcut=FILNOReadShortcut();
public:
 void Tick(const float DeltaTime,FSlateApplication& SlateApp,TSharedRef<ICursor> Cursor) override {}
 bool HandleKeyDownEvent(FSlateApplication& SlateApp,const FKeyEvent& Event) override {
  if(Event.IsRepeat() || Event.GetKey()!=Shortcut.Key ||
     Event.IsControlDown()!=Shortcut.Ctrl || Event.IsShiftDown()!=Shortcut.Shift ||
     Event.IsAltDown()!=Shortcut.Alt || Event.IsCommandDown()) return false;
  auto* Viewport=GEngine?GEngine->GameViewport:nullptr;
  if(!IsValid(Viewport) || !Viewport->GetWorld()) return false;
  if(Viewport->ViewportConsole && Viewport->ViewportConsole->ConsoleActive()) return false;
  auto* PC=Viewport->GetWorld()->GetFirstPlayerController();
  auto* HUD=IsValid(PC)?Cast<AFGHUD>(PC->GetHUD()):nullptr;
  auto* UI=HUD?HUD->GetGameUI():nullptr;
  // Never steal the shortcut from a focused text field or native dialog.
  const auto GameWidget=Viewport->GetGameViewportWidget();
  const auto Focused=SlateApp.GetKeyboardFocusedWidget();
  if(!GameWidget.IsValid() || (Focused.IsValid() && Focused.Get()!=GameWidget.Get())) {
   UE_LOG(LogFILN,Log,TEXT("FILN_ORGANIZER_SHORTCUT viewport not focused")); return false;
  }
  UE_LOG(LogFILN,Log,TEXT("FILN_ORGANIZER_SHORTCUT slate"));
  const bool WasOpen=IsValid(UI) && UI->HasActiveInteractWidget();
  UFILNOrganizer::Open(PC);
  return !WasOpen && IsValid(UI) && UI->HasActiveInteractWidget() && !UI->IsPauseMenuOpen();
 }
};
void FILNOInstallSlateInput() {
 if(FILNOInputProcessor.IsValid() || !FSlateApplication::IsInitialized() || IsRunningDedicatedServer() || IsRunningCommandlet()) return;
 auto Processor=MakeShared<FFILNOInputProcessor>();
 if(FSlateApplication::Get().RegisterInputPreProcessor(Processor)) {
  FILNOInputProcessor=Processor;
  UE_LOG(LogFILN,Log,TEXT("FILN_ORGANIZER_INPUT slate registered"));
 } else UE_LOG(LogFILN,Warning,TEXT("FILN_ORGANIZER_INPUT slate registration failed"));
}
// Weak refs avoid retaining a character or its input component across worlds.
TSet<TWeakObjectPtr<UInputComponent>> FILNOInputBindings;

FLinearColor FILNOColor(const TCHAR* Hex) { return FLinearColor::FromSRGBColor(FColor::FromHex(Hex)); }
FLinearColor FILNOAccent() { return FILNOColor(TEXT("F2A044")); }
FLinearColor FILNOInk() { return FILNOColor(TEXT("E8EBED")); }
FLinearColor FILNOMuted() { return FILNOColor(TEXT("A2ADB4")); }
FLinearColor FILNOWarn() { return FILNOColor(TEXT("E88770")); }
FLinearColor FILNOSupply() { return FILNOColor(TEXT("A3C4A1")); }
FLinearColor FILNORowColor() { return FILNOColor(TEXT("292E33")); }

FString FILNOString(FILNORow R,const TCHAR* K) {
 FString S; if(R) R->TryGetStringField(K,S); return S;
}
double FILNONumber(FILNORow R,const TCHAR* K,double Default=0) {
 double V=Default; if(R) R->TryGetNumberField(K,V); return V;
}
bool FILNOBool(FILNORow R,const TCHAR* K) {
 bool V=false; if(R) R->TryGetBoolField(K,V); return V;
}
FILNORow FILNOObject(FILNORow R,const TCHAR* K) {
 const TSharedPtr<FJsonObject>* P=nullptr;
 return R && R->TryGetObjectField(K,P) && P && P->IsValid()?P->Get():nullptr;
}
const FILNOArray& FILNOValues(FILNORow R,const TCHAR* K) {
 static const FILNOArray Empty; const FILNOArray* P=nullptr;
 return R && R->TryGetArrayField(K,P) && P?*P:Empty;
}
FILNORow FILNOAsRow(const TSharedPtr<FJsonValue>& V) {
 return V && V->Type==EJson::Object?V->AsObject().Get():nullptr;
}
FString FILNOName(FILNORow R) {
 FString S=FILNOString(R,TEXT("name"));
 if(S.IsEmpty()) S=FILNOString(R,TEXT("id"));
 return S;
}
FString FILNOQuantity(double V) { return FText::AsNumber(static_cast<int64>(V)).ToString(); }
FString FILNOCounts(double Amount,double Capacity) {
 return FILNOQuantity(Amount)+TEXT(" / ")+(filn::organizer::HasCapacity(Capacity)?FILNOQuantity(Capacity):TEXT("—"));
}
TSubclassOf<UFGItemDescriptor> FILNODescriptor(const FString& Path) {
 if(Path.IsEmpty()) return nullptr;
 UClass* Class=FindObject<UClass>(nullptr,*Path);
 return Class && Class->IsChildOf(UFGItemDescriptor::StaticClass())?TSubclassOf<UFGItemDescriptor>(Class):TSubclassOf<UFGItemDescriptor>();
}
FString FILNOItemName(const FString& Path) {
 if(auto Class=FILNODescriptor(Path)) {
  const auto Name=UFGItemDescriptor::GetItemName(Class);
  if(!Name.IsEmpty()) return Name.ToString();
 }
 // Do not invent localized names when the native class is not loaded.
 return Path.IsEmpty()?TEXT("Unknown resource"):FPackageName::ObjectPathToObjectName(Path);
}
UTextBlock* FILNOText(UWidgetTree* Tree,const FString& Text,int32 Size,FLinearColor Tint,bool Bold=false) {
 auto* Label=Tree->ConstructWidget<UTextBlock>();
 Label->SetText(FText::FromString(Text));
 Label->SetFont(FCoreStyle::GetDefaultFontStyle(Bold?TEXT("Bold"):TEXT("Regular"),Size));
 Label->SetColorAndOpacity(FSlateColor(Tint)); Label->SetAutoWrapText(true); return Label;
}
UBorder* FILNOPanel(UWidgetTree* Tree,FLinearColor Tint,const FMargin& Padding) {
 auto* B=Tree->ConstructWidget<UBorder>(); B->SetBrush(FSlateColorBrush(FLinearColor::White));
 B->SetBrushColor(Tint); B->SetPadding(Padding); return B;
}
void FILNOFill(UHorizontalBoxSlot* Slot,float Weight=1.f) {
 FSlateChildSize Size(ESlateSizeRule::Fill); Size.Value=Weight; Slot->SetSize(Size);
}
void FILNOAdd(UVerticalBox* Box,UWidget* Widget,float Bottom=0) {
 Box->AddChildToVerticalBox(Widget)->SetPadding(FMargin(0,0,0,Bottom));
}
void FILNOAddH(UHorizontalBox* Box,UWidget* Widget,float Right=0) {
 auto* S=Box->AddChildToHorizontalBox(Widget); S->SetPadding(FMargin(0,0,Right,0)); S->SetVerticalAlignment(VAlign_Center);
}
void FILNOButtonStyle(UButton* Button,bool Active=false) {
 FButtonStyle Style;
 Style.SetNormal(FSlateColorBrush(Active?FILNOAccent():FILNOColor(TEXT("32383D"))));
 Style.SetHovered(FSlateColorBrush(Active?FILNOColor(TEXT("FFC071")):FILNOColor(TEXT("46515A"))));
 Style.SetPressed(FSlateColorBrush(FILNOColor(TEXT("BD772E"))));
 Style.SetNormalPadding(FMargin(14,8)); Style.SetPressedPadding(FMargin(14,8));
 Button->SetStyle(Style); Button->SetBackgroundColor(FLinearColor::White);
}
UWidget* FILNOResource(UWidgetTree* Tree,const FString& Path,const FString& Quantity,bool Strong=false) {
 auto* Line=Tree->ConstructWidget<UHorizontalBox>();
 if(auto Class=FILNODescriptor(Path)) {
  if(auto* Icon=UFGItemDescriptor::GetSmallIcon(Class)) {
   auto* Box=Tree->ConstructWidget<USizeBox>(); Box->SetWidthOverride(28); Box->SetHeightOverride(28);
   auto* Image=Tree->ConstructWidget<UImage>(); Image->SetBrushFromTexture(Icon,true); Box->AddChild(Image);
   FILNOAddH(Line,Box,8);
  }
 }
 auto* Name=FILNOText(Tree,FILNOItemName(Path),Strong?17:14,Strong?FILNOAccent():FILNOInk(),Strong);
 auto* NS=Line->AddChildToHorizontalBox(Name); FILNOFill(NS); NS->SetVerticalAlignment(VAlign_Center);
 if(!Quantity.IsEmpty()) FILNOAddH(Line,FILNOText(Tree,Quantity,14,FILNOMuted()));
 return Line;
}
UWidget* FILNOBar(UWidgetTree* Tree,double Amount,double Capacity,FLinearColor Fill) {
 auto* Box=Tree->ConstructWidget<USizeBox>(); Box->SetHeightOverride(4);
 auto* Bar=Tree->ConstructWidget<UProgressBar>(); FProgressBarStyle Style;
 Style.SetBackgroundImage(FSlateColorBrush(FILNOColor(TEXT("424B52"))));
 Style.SetFillImage(FSlateColorBrush(FLinearColor::White)); Bar->SetWidgetStyle(Style);
 Bar->SetFillColorAndOpacity(Fill); Bar->SetPercent(static_cast<float>(FMath::Clamp(Amount/Capacity,0.,1.)));
 Box->AddChild(Bar); return Box;
}
ALogisticsSubsystem* FILNOFindSubsystem(UWorld* World) {
 if(!World || World->GetNetMode()==NM_Client) return nullptr;
 auto* Manager=World->GetSubsystem<USubsystemActorManager>();
 return Manager?Manager->GetSubsystemActor<ALogisticsSubsystem>():nullptr;
}
void FILNOBindInput(AFGCharacterPlayer* Character,UInputComponent* Input) {
 if(!IsValid(Character) || !IsValid(Input)) return;
 FILNOInstallSlateInput();
 // Setup can be called again on possession. Avoid duplicate delegate bindings
 // while allowing a freshly rebuilt component to bind again.
 for(auto It=FILNOInputBindings.CreateIterator();It;++It) if(!(*It).IsValid()) It.RemoveCurrent();
 if(FILNOInputBindings.Contains(Input)) return;
 const auto Shortcut=FILNOReadShortcut();
 if(!Shortcut.Key.IsValid()) { UE_LOG(LogFILN,Warning,TEXT("FILN_ORGANIZER_INVALID_KEY %s"),*Shortcut.Key.ToString()); return; }
 FInputKeyBinding Binding(FInputChord(Shortcut.Key,Shortcut.Shift,Shortcut.Ctrl,Shortcut.Alt,false),IE_Pressed);
 Binding.bConsumeInput=false;
 const TWeakObjectPtr<AFGCharacterPlayer> Weak(Character);
 auto& Delegate=Binding.KeyDelegate.GetDelegateForManualSet();
 Delegate.BindLambda([Weak]() { if(auto* C=Weak.Get()) {
  UE_LOG(LogFILN,Log,TEXT("FILN_ORGANIZER_SHORTCUT character"));
  UFILNOrganizer::Open(Cast<APlayerController>(C->GetController()));
 } });
 FILNOInputBindings.Add(Input);
 Input->KeyBindings.Add(MoveTemp(Binding));
 UE_LOG(LogFILN,Log,TEXT("FILN_ORGANIZER_INPUT character bound key=%s ctrl=%d shift=%d alt=%d"),*Shortcut.Key.ToString(),Shortcut.Ctrl,Shortcut.Shift,Shortcut.Alt);
}
}

UFILNOrganizer::UFILNOrganizer(const FObjectInitializer& Initializer):Super(Initializer) {
 mUseKeyboard=true; mUseMouse=true; mDisablePlayerActions=true; mDisableBuildGunActions=true;
 mDisablePlayerEquipmentManagement=true; mDisableVisualizationModeActions=true;
 mSupportsCaching=false; mSupportsStacking=false;
 SetDesiredHorizontalAlignment(HAlign_Fill); SetDesiredVerticalAlignment(VAlign_Fill);
 SetDesiredAlignmentSize(FSlateChildSize(ESlateSizeRule::Fill));
 SetIsFocusable(true);
}
void UFILNOrganizer::InstallInput() {
 if(!FILNOInputHandle.IsValid()) FILNOInputHandle=AFGCharacterPlayer::OnPlayerInputInitialized.AddStatic(&FILNOBindInput);
 FILNOInstallSlateInput();
}
void UFILNOrganizer::RemoveInput() {
 if(FILNOInputHandle.IsValid()) AFGCharacterPlayer::OnPlayerInputInitialized.Remove(FILNOInputHandle);
 FILNOInputHandle.Reset();
 if(FILNOInputProcessor.IsValid() && FSlateApplication::IsInitialized()) FSlateApplication::Get().UnregisterInputPreProcessor(FILNOInputProcessor);
 FILNOInputProcessor.Reset();
 // Input components are owned by their game-world characters and destroyed
 // with them. No native hot-unload is supported by this mod.
 FILNOInputBindings.Reset();
}
void UFILNOrganizer::Open(APlayerController* PC) {
 if(!IsValid(PC) || !PC->IsLocalController() || !PC->GetPawn()) {
  UE_LOG(LogFILN,Log,TEXT("FILN_ORGANIZER_OPEN_BLOCKED no local possessed controller")); return;
 }
 auto* W=PC->GetWorld();
 if(!W || !W->IsGameWorld() || W->bIsTearingDown || FPluginModuleLoader::IsMainMenuWorld(W)) {
  UE_LOG(LogFILN,Log,TEXT("FILN_ORGANIZER_OPEN_BLOCKED no active gameplay world")); return;
 }
 auto* HUD=Cast<AFGHUD>(PC->GetHUD()); auto* UI=HUD?HUD->GetGameUI():nullptr;
 if(!IsValid(UI) || UI->IsPauseMenuOpen() || UI->HasActiveInteractWidget() || PC->bShowMouseCursor) {
  UE_LOG(LogFILN,Log,TEXT("FILN_ORGANIZER_OPEN_BLOCKED ui=%d pause=%d interact=%d cursor=%d"),IsValid(UI),IsValid(UI) && UI->IsPauseMenuOpen(),IsValid(UI) && UI->HasActiveInteractWidget(),PC->bShowMouseCursor); return;
 }
 UE_LOG(LogFILN,Log,TEXT("FILN_ORGANIZER_OPEN_REQUEST"));
 HUD->OpenInteractUI(StaticClass(),PC);
}
TSharedRef<SWidget> UFILNOrganizer::RebuildWidget() {
 if(!WidgetTree) WidgetTree=NewObject<UWidgetTree>(this,TEXT("FILNOrganizerTree"));
 if(!WidgetTree->RootWidget) {
  auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Canvas;
  auto* Shade=FILNOPanel(WidgetTree,FLinearColor(0,0,0,0.65f),FMargin(0));
  auto* ShadeSlot=Canvas->AddChildToCanvas(Shade); ShadeSlot->SetAnchors(FAnchors(0,0,1,1)); ShadeSlot->SetOffsets(FMargin(0));
  auto* Frame=FILNOPanel(WidgetTree,FILNOColor(TEXT("171C20")),FMargin(18,14));
  auto* FrameSlot=Canvas->AddChildToCanvas(Frame); FrameSlot->SetAnchors(FAnchors(.07f,.08f,.93f,.92f)); FrameSlot->SetOffsets(FMargin(0));
  auto* Main=WidgetTree->ConstructWidget<UVerticalBox>(); Frame->AddChild(Main);
  auto* Header=WidgetTree->ConstructWidget<UHorizontalBox>();
  auto* Title=FILNOText(WidgetTree,TEXT("FILN  /  LOGISTICS ORGANIZER"),22,FILNOAccent(),true);
  auto* TitleSlot=Header->AddChildToHorizontalBox(Title); FILNOFill(TitleSlot); TitleSlot->SetVerticalAlignment(VAlign_Center);
  CloseButton=WidgetTree->ConstructWidget<UButton>(); FILNOButtonStyle(CloseButton);
  CloseButton->AddChild(FILNOText(WidgetTree,TEXT("CLOSE  [ESC]"),13,FILNOInk(),true));
  CloseButton->OnClicked.AddDynamic(this,&UFILNOrganizer::CloseOrganizer); FILNOAddH(Header,CloseButton);
  FILNOAdd(Main,Header,14);
  auto* Tabs=WidgetTree->ConstructWidget<UHorizontalBox>();
  const TCHAR* Names[]={TEXT("VEHICLES"),TEXT("STATIONS"),TEXT("DIAGNOSTICS")};
  for(int32 I=0;I<3;++I) {
   auto* B=WidgetTree->ConstructWidget<UButton>(); auto* Label=FILNOText(WidgetTree,Names[I],14,FILNOInk(),true);
   FILNOButtonStyle(B,I==0); B->AddChild(Label); TabButtons.Add(B); TabLabels.Add(Label);
   auto* S=Tabs->AddChildToHorizontalBox(B); FILNOFill(S); S->SetPadding(FMargin(0,0,I==2?0:4,0));
  }
  TabButtons[0]->OnClicked.AddDynamic(this,&UFILNOrganizer::ShowVehicles);
  TabButtons[1]->OnClicked.AddDynamic(this,&UFILNOrganizer::ShowStations);
  TabButtons[2]->OnClicked.AddDynamic(this,&UFILNOrganizer::ShowDiagnostics);
  FILNOAdd(Main,Tabs,10);
  Scroll=WidgetTree->ConstructWidget<UScrollBox>(); Rows=WidgetTree->ConstructWidget<UVerticalBox>(); Scroll->AddChild(Rows);
  FSlateChildSize Fill(ESlateSizeRule::Fill); Main->AddChildToVerticalBox(Scroll)->SetSize(Fill);
  Footer=FILNOText(WidgetTree,TEXT("Waiting for logistics snapshot…"),12,FILNOMuted()); FILNOAdd(Main,Footer);
  SetDefaultFocusWidget(CloseButton);
 }
 return Super::RebuildWidget();
}
void UFILNOrganizer::NativeConstruct() {
 Super::NativeConstruct(); bOpen=true; NextRefresh=0; Snapshot.Reset(); Tab=0;
 for(auto& Offset:ScrollOffsets) Offset=0;
 SelectTab(0); Refresh();
 UE_LOG(LogFILN,Log,TEXT("FILN_ORGANIZER_OPEN"));
}
void UFILNOrganizer::NativeDestruct() {
 bOpen=false; Snapshot.Reset(); Super::NativeDestruct();
 UE_LOG(LogFILN,Log,TEXT("FILN_ORGANIZER_CLOSE"));
}
void UFILNOrganizer::NativeTick(const FGeometry& Geometry,float Delta) {
 Super::NativeTick(Geometry,Delta);
 if(bOpen && FPlatformTime::Seconds()>=NextRefresh) Refresh();
}
FReply UFILNOrganizer::NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) {
 if(Event.GetKey()==EKeys::Escape) { CloseOrganizer(); return FReply::Handled(); }
 return Super::NativeOnPreviewKeyDown(Geometry,Event);
}
void UFILNOrganizer::OnEscapePressed_Implementation() { CloseOrganizer(); }
void UFILNOrganizer::CloseOrganizer() {
 auto* PC=GetOwningPlayer(); auto* HUD=PC?Cast<AFGHUD>(PC->GetHUD()):nullptr;
 if(auto* UI=HUD?HUD->GetGameUI():nullptr) {
  if(!UI->PopWidget(this)) { UE_LOG(LogFILN,Warning,TEXT("FILN_ORGANIZER_POP_FAILED")); }
 }
}
void UFILNOrganizer::ShowVehicles() { SelectTab(0); }
void UFILNOrganizer::ShowStations() { SelectTab(1); }
void UFILNOrganizer::ShowDiagnostics() { SelectTab(2); }
void UFILNOrganizer::SelectTab(int32 Index) {
 if(Index<0 || Index>=3 || !Rows || !Scroll) return;
 ScrollOffsets[Tab]=Scroll->GetScrollOffset(); Tab=Index;
 for(int32 I=0;I<TabButtons.Num();++I) {
  FILNOButtonStyle(TabButtons[I],I==Tab);
  TabLabels[I]->SetColorAndOpacity(FSlateColor(I==Tab?FILNOColor(TEXT("171C20")):FILNOInk()));
 }
 Render(); Scroll->SetScrollOffset(ScrollOffsets[Tab]);
}
void UFILNOrganizer::Refresh() {
 NextRefresh=FPlatformTime::Seconds()+0.5;
 if(!Footer) return;
 auto* S=FILNOFindSubsystem(GetWorld());
 auto Latest=IsValid(S)?S->OrganizerSnapshot():TSharedPtr<const FJsonObject>();
 if(Latest!=Snapshot) {
  if(Scroll) ScrollOffsets[Tab]=Scroll->GetScrollOffset();
  Snapshot=MoveTemp(Latest); Render(); if(Scroll) Scroll->SetScrollOffset(ScrollOffsets[Tab]);
 }
 if(!Snapshot) {
  Footer->SetText(FText::FromString(GetWorld() && GetWorld()->GetNetMode()==NM_Client?
   TEXT("Server snapshot unavailable on remote clients."):TEXT("Waiting for the first complete logistics snapshot…")));
  return;
 }
 const auto Age=FMath::Max<int64>(0,FDateTime::UtcNow().ToUnixTimestamp()-static_cast<int64>(FILNONumber(Snapshot.Get(),TEXT("exported_at"))));
 Footer->SetText(FText::FromString(FString::Printf(TEXT("LIVE SNAPSHOT  #%lld    •    %lld s since collection    •    Read only"),
  static_cast<long long>(FILNONumber(Snapshot.Get(),TEXT("sequence"))),static_cast<long long>(Age))));
}
void UFILNOrganizer::Render() {
 if(!Rows) return;
 Rows->ClearChildren();
 if(!Snapshot) { FILNOAdd(Rows,FILNOText(WidgetTree,TEXT("No complete snapshot available yet."),16,FILNOMuted())); return; }
 if(Tab==0) RenderVehicles(); else if(Tab==1) RenderStations(); else RenderDiagnostics();
}
void UFILNOrganizer::RenderVehicles() {
 TMap<FString,FString> Names;
 for(const auto& V:FILNOValues(Snapshot.Get(),TEXT("stations"))) if(auto* R=FILNOAsRow(V)) Names.Add(FILNOString(R,TEXT("guid")),FILNOName(R));
 TArray<FILNORow> Vehicles;
 for(const auto& V:FILNOValues(Snapshot.Get(),TEXT("vehicles"))) if(auto* R=FILNOAsRow(V)) Vehicles.Add(R);
 // Use stable row order without relying on game pointer identity.
 Vehicles.Sort([](const FJsonObject& A,const FJsonObject& B) {
  const auto AN=FILNOName(&A),BN=FILNOName(&B); return AN==BN?FILNOString(&A,TEXT("id"))<FILNOString(&B,TEXT("id")):AN<BN;
 });
 const bool Healthy=FILNOBool(FILNOObject(Snapshot.Get(),TEXT("dispatcher")),TEXT("registry_validated"));
 FILNOAdd(Rows,FILNOText(WidgetTree,FString::Printf(TEXT("%d VEHICLES"),Vehicles.Num()),12,FILNOMuted(),true),8);
 for(auto* V:Vehicles) {
  auto* Panel=FILNOPanel(WidgetTree,FILNORowColor(),FMargin(10,7)); auto* Body=WidgetTree->ConstructWidget<UVerticalBox>(); Panel->AddChild(Body);
  auto* Head=WidgetTree->ConstructWidget<UHorizontalBox>();
  auto* Name=Head->AddChildToHorizontalBox(FILNOText(WidgetTree,FILNOName(V),16,FILNOInk(),true)); FILNOFill(Name);
  auto* Job=FILNOObject(V,TEXT("job")); auto* Cargo=FILNOObject(V,TEXT("cargo"));
  const bool DepotWarning=FILNOBool(V,TEXT("depot_capacity_warning"));
  FString Status;
  if(DepotWarning) Status=TEXT("NO FREE DEPOT");
  else if(FILNOBool(V,TEXT("driver"))) Status=TEXT("Player driving");
  else if(!FILNOBool(V,TEXT("autopilot"))) Status=TEXT("Autopilot off");
  else if(FILNOBool(V,TEXT("registration_pending"))) Status=TEXT("Registration pending");
  else if(!FILNOBool(V,TEXT("managed"))) Status=TEXT("Not registered");
  else if(!Healthy) Status=TEXT("Native route");
  else if(FILNOBool(V,TEXT("held"))) Status=TEXT("Waiting at depot");
  else { Status=FILNOString(Job,TEXT("state")); if(Status.IsEmpty()) Status=TEXT("Autopilot on"); }
  FILNOAddH(Head,FILNOText(WidgetTree,Status,13,DepotWarning?FILNOWarn():FILNOMuted(),DepotWarning)); FILNOAdd(Body,Head,4);
  if(DepotWarning) FILNOAdd(Body,FILNOText(WidgetTree,TEXT("Waiting for a free compatible depot · retrying automatically"),13,FILNOWarn()),4);
  const auto Reason=FILNOString(Job,TEXT("error"));
  if(Status==TEXT("Relocating") || Status==TEXT("Waiting at depot")) {
   FILNOAdd(Body,FILNOText(WidgetTree,Reason.IsEmpty()?TEXT("No dispatch/parking reason recorded"):Reason,13,Status==TEXT("Relocating")?FILNOWarn():FILNOMuted()),3);
  }
  auto* Route=WidgetTree->ConstructWidget<UWrapBox>(); Route->SetInnerSlotPadding(FVector2D(4,2));
  const auto& Nodes=FILNOValues(V,TEXT("route")); const int32 Target=static_cast<int32>(FILNONumber(V,TEXT("target"),-1));
  if(Nodes.IsEmpty()) Route->AddChildToWrapBox(FILNOText(WidgetTree,TEXT("No route"),13,FILNOMuted()));
  for(int32 I=0;I<Nodes.Num();++I) {
   if(I>0) Route->AddChildToWrapBox(FILNOText(WidgetTree,TEXT("›"),13,FILNOMuted()));
   const FString ID=Nodes[I]->AsString(); const auto* N=Names.Find(ID);
   const bool Current=filn::organizer::CurrentTarget(I,Target,Nodes.Num());
   Route->AddChildToWrapBox(FILNOText(WidgetTree,N?*N:ID,13,Current?FILNOAccent():FILNOMuted(),Current));
  }
  FILNOAdd(Body,Route,4);
  const auto& Items=FILNOValues(Cargo,TEXT("items"));
  if(!FILNOBool(Cargo,TEXT("valid"))) FILNOAdd(Body,FILNOText(WidgetTree,TEXT("Cargo readback incomplete"),13,FILNOWarn()),3);
  if(Items.IsEmpty() && FILNOBool(Cargo,TEXT("valid"))) FILNOAdd(Body,FILNOText(WidgetTree,TEXT("Empty cargo"),13,FILNOMuted()));
  for(const auto& Item:Items) if(auto* R=FILNOAsRow(Item)) {
   FString Qty=FILNOQuantity(FILNONumber(R,TEXT("amount")));
   if(Items.Num()==1) Qty=FILNOCounts(FILNONumber(R,TEXT("amount")),FILNONumber(Cargo,TEXT("capacity")));
   if(FILNOString(V,TEXT("medium"))==TEXT("fluid")) Qty+=TEXT(" (native units)");
   FILNOAdd(Body,FILNOResource(WidgetTree,FILNOString(R,TEXT("item")),Qty),2);
  }
  if(Items.Num()==1 && FILNOBool(Cargo,TEXT("valid")) && FILNONumber(Cargo,TEXT("capacity"))>0) FILNOAdd(Body,FILNOBar(WidgetTree,FILNONumber(Cargo,TEXT("amount")),FILNONumber(Cargo,TEXT("capacity")),FILNOAccent()));
  FILNOAdd(Rows,Panel,4);
 }
}
void UFILNOrganizer::RenderStations() {
 struct FILNOGroup { TArray<FILNORow> Providers,Requesters; };
 TMap<FString,FILNOGroup> Groups;
 for(const auto& V:FILNOValues(Snapshot.Get(),TEXT("stations"))) if(auto* R=FILNOAsRow(V)) {
  const FString Saved=FILNOString(R,TEXT("saved_item")),Observed=FILNOString(R,TEXT("observed_item"));
  const auto Side=filn::organizer::StationSide(FILNOBool(R,TEXT("is_depot")),FILNOString(R,TEXT("role"))==TEXT("provider"),TCHAR_TO_UTF8(*Saved),TCHAR_TO_UTF8(*Observed));
  if(Side==filn::organizer::Side::None) continue;
  auto& Group=Groups.FindOrAdd(Saved.IsEmpty()?Observed:Saved);
  (Side==filn::organizer::Side::Provider?Group.Providers:Group.Requesters).Add(R);
 }
 TArray<FString> Keys; Groups.GetKeys(Keys);
 Keys.Sort([](const FString& A,const FString& B) { const auto AN=FILNOItemName(A),BN=FILNOItemName(B); return AN==BN?A<B:AN<BN; });
 FILNOAdd(Rows,FILNOText(WidgetTree,FString::Printf(TEXT("%d RESOURCES"),Keys.Num()),12,FILNOMuted(),true),8);
 for(const auto& Key:Keys) {
  auto& Group=Groups.FindChecked(Key);
  auto* Panel=FILNOPanel(WidgetTree,FILNORowColor(),FMargin(10,8)); auto* Body=WidgetTree->ConstructWidget<UVerticalBox>(); Panel->AddChild(Body);
  FILNOAdd(Body,FILNOResource(WidgetTree,Key,TEXT(""),true),8);
  auto* Columns=WidgetTree->ConstructWidget<UHorizontalBox>();
  for(int32 Side=0;Side<2;++Side) {
   auto& Stations=Side==0?Group.Providers:Group.Requesters;
   Stations.Sort([](const FJsonObject& A,const FJsonObject& B){return FILNOName(&A)<FILNOName(&B);});
   auto* Column=WidgetTree->ConstructWidget<UVerticalBox>(); auto* ColSlot=Columns->AddChildToHorizontalBox(Column);
   FILNOFill(ColSlot); ColSlot->SetPadding(FMargin(0,0,Side==0?12:0,0));
   const auto Tint=Side==0?FILNOSupply():FILNOAccent();
   FILNOAdd(Column,FILNOText(WidgetTree,Side==0?TEXT("PRODUCTION"):TEXT("REQUESTS"),12,Tint,true),4);
   if(Stations.IsEmpty()) FILNOAdd(Column,FILNOText(WidgetTree,TEXT("—"),14,FILNOMuted()));
   for(auto* R:Stations) {
    auto* Line=WidgetTree->ConstructWidget<UHorizontalBox>();
    const auto Name=FILNOName(R); const bool Ready=FILNOBool(R,TEXT("configured")) && FILNOBool(R,TEXT("powered")) && !FILNOBool(R,TEXT("standby"));
    FILNOFill(Line->AddChildToHorizontalBox(FILNOText(WidgetTree,Name,13,Ready?FILNOInk():FILNOMuted())));
    const double Amount=FILNONumber(R,TEXT("display_amount")),Capacity=FILNONumber(R,TEXT("display_capacity"));
    FILNOAddH(Line,FILNOText(WidgetTree,FILNOCounts(Amount,Capacity),13,FILNOMuted()));
    FILNOAdd(Column,Line,3);
    if(Capacity>0 && FILNOBool(R,TEXT("stock_valid"))) FILNOAdd(Column,FILNOBar(WidgetTree,Amount,Capacity,Tint),4);
    if(FILNONumber(R,TEXT("amount"))!=Amount) FILNOAdd(Column,FILNOText(WidgetTree,TEXT("Other cargo present"),12,FILNOWarn()),3);
    if(FILNOString(R,TEXT("medium"))==TEXT("fluid")) FILNOAdd(Column,FILNOText(WidgetTree,TEXT("Native fluid units"),11,FILNOMuted()),2);
   }
  }
  FILNOAdd(Body,Columns); FILNOAdd(Rows,Panel,6);
 }
}
void UFILNOrganizer::RenderDiagnostics() {
 auto* Dispatcher=FILNOObject(Snapshot.Get(),TEXT("dispatcher"));
 const bool Blocked=FILNOBool(Dispatcher,TEXT("dispatch_blocked"));
 const bool Valid=FILNOBool(Dispatcher,TEXT("registry_validated"));
 const bool Enabled=FILNOBool(Dispatcher,TEXT("enabled"));
 FILNOAdd(Rows,FILNOText(WidgetTree,Blocked?TEXT("DISPATCHER BLOCKED"):Enabled?TEXT("DISPATCHER ACTIVE"):TEXT("DISPATCHER DISABLED"),20,Blocked?FILNOWarn():FILNOAccent(),true),8);
 FILNOAdd(Rows,FILNOText(WidgetTree,Valid?TEXT("Registry validated"):TEXT("Registry not validated"),15,Valid?FILNOSupply():FILNOWarn()),8);
 for(const auto& V:FILNOValues(Snapshot.Get(),TEXT("registry_errors"))) FILNOAdd(Rows,FILNOText(WidgetTree,V->AsString(),14,FILNOWarn()),5);
 TMap<FString,FString> Names;
 for(const auto& V:FILNOValues(Snapshot.Get(),TEXT("stations"))) if(auto* R=FILNOAsRow(V)) Names.Add(FILNOString(R,TEXT("guid")),FILNOName(R));
 const auto& Errors=FILNOValues(Snapshot.Get(),TEXT("errors"));
 FILNOAdd(Rows,FILNOText(WidgetTree,FString::Printf(TEXT("SNAPSHOT ERRORS  /  %d"),Errors.Num()),14,FILNOAccent(),true),6);
 for(const auto& V:Errors) if(auto* R=FILNOAsRow(V)) {
  const auto ID=FILNOString(R,TEXT("id")); const auto* N=Names.Find(ID);
  FILNOAdd(Rows,FILNOText(WidgetTree,(N?*N:ID)+TEXT(" — ")+FILNOString(R,TEXT("error")),13,FILNOInk()),4);
 }
 FILNOAdd(Rows,FILNOText(WidgetTree,TEXT("VEHICLE STATUS DETAILS"),14,FILNOAccent(),true),6);
 for(const auto& V:FILNOValues(Snapshot.Get(),TEXT("vehicles"))) if(auto* R=FILNOAsRow(V)) {
  auto Why=FILNOString(FILNOObject(R,TEXT("job")),TEXT("error"));
  if(Why.IsEmpty()) Why=FILNOString(R,TEXT("native_pending_reason"));
  if(Why.IsEmpty()) Why=FILNOBool(R,TEXT("managed"))?TEXT("No recorded diagnostic"):TEXT("Not enrolled");
  auto* Row=FILNOPanel(WidgetTree,FILNORowColor(),FMargin(8,5));
  Row->AddChild(FILNOText(WidgetTree,FILNOName(R)+TEXT(" — ")+Why+FString::Printf(TEXT("  [native AP=%d]"),static_cast<int32>(FILNONumber(R,TEXT("native_error")))),13,FILNOInk()));
  FILNOAdd(Rows,Row,3);
 }
}
