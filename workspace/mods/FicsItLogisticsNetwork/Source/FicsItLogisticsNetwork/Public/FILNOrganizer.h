#pragma once
#include "CoreMinimal.h"
#include "UI/FGInteractWidget.h"
#include "FILNOrganizer.generated.h"

class FJsonObject;
class UButton;
class UScrollBox;
class UTextBlock;
class UVerticalBox;

UCLASS()
class FICSITLOGISTICSNETWORK_API UFILNOrganizer : public UFGInteractWidget {
 GENERATED_BODY()
public:
 explicit UFILNOrganizer(const FObjectInitializer& Initializer);
 static void InstallInput();
 static void RemoveInput();
 static void Open(APlayerController* PC);
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
 virtual void NativeConstruct() override;
 virtual void NativeDestruct() override;
 virtual void NativeTick(const FGeometry& Geometry,float Delta) override;
 virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
 virtual void OnEscapePressed_Implementation() override;
private:
 UPROPERTY(Transient) TObjectPtr<UScrollBox> Scroll;
 UPROPERTY(Transient) TObjectPtr<UVerticalBox> Rows;
 UPROPERTY(Transient) TObjectPtr<UTextBlock> Footer;
 UPROPERTY(Transient) TObjectPtr<UButton> CloseButton;
 UPROPERTY(Transient) TArray<TObjectPtr<UButton>> TabButtons;
 UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> TabLabels;
 TSharedPtr<const FJsonObject> Snapshot;
 int32 Tab=0;
 double NextRefresh=0;
 bool bOpen=false;
 float ScrollOffsets[3]={0,0,0};
 UFUNCTION() void ShowVehicles();
 UFUNCTION() void ShowStations();
 UFUNCTION() void ShowDiagnostics();
 UFUNCTION() void CloseOrganizer();
 void SelectTab(int32 Index);
 void Refresh();
 void Render();
 void RenderVehicles();
 void RenderStations();
 void RenderDiagnostics();
};
