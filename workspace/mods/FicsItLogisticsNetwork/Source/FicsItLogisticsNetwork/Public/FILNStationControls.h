#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "Components/ActorComponent.h"
#include "FGRemoteCallObject.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FILNStationControls.generated.h"

class AFGBuildableDockingStation;
class UFGItemDescriptor;
class UFGInteractWidget;
class UVerticalBox;
class UTextBlock;
class UButton;
class UWidget;
class UFILNResourceSelector;

// Transient registry marker. Selection is handled separately by the native UI.
UCLASS()
class FICSITLOGISTICSNETWORK_API UFILNStationMarker : public UActorComponent {
 GENERATED_BODY()
public:
 UFILNStationMarker() { SetIsReplicatedByDefault(true); PrimaryComponentTick.bCanEverTick=false; }
};

/** The only player mutation is an explicit requester resource selection. */
UCLASS()
class FICSITLOGISTICSNETWORK_API UFILNResourceRemote : public UFGRemoteCallObject {
 GENERATED_BODY()
public:
 void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
 uint32 NextRequestId();
 TWeakObjectPtr<UFILNResourceSelector> Receiver;
 UFUNCTION(Server,Reliable) void QueryResource(AFGBuildableDockingStation* Target,uint32 RequestId);
 UFUNCTION(Server,Reliable) void SelectResource(AFGBuildableDockingStation* Target,uint32 RequestId,TSubclassOf<UFGItemDescriptor> Resource);
 UFUNCTION(Client,Reliable) void ResourceReply(AFGBuildableDockingStation* Target,uint32 RequestId,bool Accepted,bool Editable,TSubclassOf<UFGItemDescriptor> Resource,const FText& Message);
private:
 UPROPERTY(Replicated) bool NetReady=true;
 uint32 ClientSequence=0;
 double LastMutation=-10;
 void HandleResource(AFGBuildableDockingStation* Target,uint32 RequestId,bool Mutate,TSubclassOf<UFGItemDescriptor> Resource);
};

UCLASS()
class FICSITLOGISTICSNETWORK_API UFILNResourceUIRegistration : public UGameInstanceSubsystem {
 GENERATED_BODY()
public:
 void Initialize(FSubsystemCollectionBase& Collection) override;
};

/** New child only: native station widget, original storage children and mode labels are preserved. */
UCLASS()
class FICSITLOGISTICSNETWORK_API UFILNResourceSelector : public UUserWidget {
 GENERATED_BODY()
public:
 static void InstallHook();
 bool BuildControls();
 void SetContext(AFGBuildableDockingStation* Target,UVerticalBox* Container);
 void ClearContext();
 bool HasContext(AFGBuildableDockingStation* Target) const;
 void Reply(AFGBuildableDockingStation* Target,uint32 RequestId,bool Accepted,bool Editable,TSubclassOf<UFGItemDescriptor> Resource,const FText& Message);
protected:
 void NativeDestruct() override;
private:
 UPROPERTY() TObjectPtr<UButton> StorageTab;
 UPROPERTY() TObjectPtr<UButton> FiltersTab;
 UPROPERTY() TObjectPtr<UComboBoxString> Parts;
 UPROPERTY() TMap<FString,TSubclassOf<UFGItemDescriptor>> Choices;
 FString ChoiceFor(TSubclassOf<UFGItemDescriptor> Resource);
 void PopulateChoices();
 void SetTabStyle(bool Filters);
 UPROPERTY() TObjectPtr<UTextBlock> Feedback;
 TWeakObjectPtr<AFGBuildableDockingStation> Station;
 TWeakObjectPtr<UVerticalBox> InventoryContainer;
 TArray<TPair<TWeakObjectPtr<UWidget>,ESlateVisibility>> StorageVisibility;
 uint32 PendingRequest=0;
 bool bFilterOpen=false, bSynchronizing=false, bEditable=false, bInitialized=false;
 void RestoreStorage();
 void Query();
 void Fail(const FText& Reason);
 bool ShowSelection(TSubclassOf<UFGItemDescriptor> Resource);
 UFILNResourceRemote* Remote() const;
 UFUNCTION() void OpenStorage();
 UFUNCTION() void OpenFilters();
 UFUNCTION() void NativeListChanged(FString SelectedItem,ESelectInfo::Type SelectionType);
 void NativeTabClicked(int32 ButtonIndex);
 UFUNCTION() UWidget* GenerateChoice(FString Item);
};
