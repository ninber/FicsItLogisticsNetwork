// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FGRemoteCallObject.h"
#include "Components/ActorComponent.h"
#include "Components/TextBlock.h"
#include "UI/FGInteractWidget.h"
#include "FILNStationControls.generated.h"
class AFGBuildableDockingStation;
class UFGItemDescriptor;
class UComboBoxString;
class UTextBlock;
class AFGWheeledVehicleIdentifier;
class UFILNStationControls;

// Runtime replication marker only; RF_Transient instances are rebuilt from the saved registry.
UCLASS()
class FICSITLOGISTICSNETWORK_API UFILNStationMarker : public UActorComponent {
 GENERATED_BODY()
public:
 UFILNStationMarker() { SetIsReplicatedByDefault(true); PrimaryComponentTick.bCanEverTick=false; }
};
UCLASS()
class FICSITLOGISTICSNETWORK_API UFILNStationWindow : public UFGInteractWidget {
 GENERATED_BODY()
protected:
 void NativeOnInitialized() override;
 void NativeConstruct() override;
private:
 UPROPERTY() TObjectPtr<UFILNStationControls> Controls;
};

UCLASS()
class FICSITLOGISTICSNETWORK_API UFILNLabelBinding : public UObject {
 GENERATED_BODY()
public:
 UWidget::FGetText Original;
 UPROPERTY() FText Fallback;
 UPROPERTY() TObjectPtr<UTextBlock> Label;
 bool Enrolled=false;
 UFUNCTION() FText Read();
};

/** Actor references and resource requests are validated by the server, never executed as console text. */
UCLASS()
class FICSITLOGISTICSNETWORK_API UFILNRemoteControl : public UFGRemoteCallObject {
 GENERATED_BODY()
public:
 void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
 UFUNCTION(Server,Reliable) void QueryStation(AFGBuildableDockingStation* Station);
 UFUNCTION(Server,Reliable) void Configure(AFGBuildableDockingStation* Station,TSubclassOf<UFGItemDescriptor> Resource,bool Remove);
 UFUNCTION(Server,Reliable) void NetworkOverview(AFGBuildableDockingStation* Station,int32 Page);
 UFUNCTION(Client,Reliable) void OverviewReply(AFGBuildableDockingStation* Station,int32 Page,int32 Pages,const FText& Text);
 UFUNCTION(Server,Reliable) void RegisterVehicle(AFGBuildableDockingStation* Station,AFGWheeledVehicleIdentifier* Vehicle);
 UFUNCTION(Server,Reliable) void RehomeVehicle(AFGBuildableDockingStation* Station,AFGWheeledVehicleIdentifier* Vehicle);
 UFUNCTION(Server,Reliable) void AdminAction(AFGBuildableDockingStation* Station,uint8 Action);
 UFUNCTION(Client,Reliable) void StationReply(AFGBuildableDockingStation* Station,bool Enrolled,TSubclassOf<UFGItemDescriptor> Resource,const FText& Message);
 TWeakObjectPtr<UFILNStationControls> Receiver;
private:
 UPROPERTY(Replicated) bool NetReady=true;
 double LastRequest=-10;
 bool ValidateContext(AFGBuildableDockingStation* Station);
};
UCLASS()
class FICSITLOGISTICSNETWORK_API UFILNUIRegistration : public UGameInstanceSubsystem {
 GENERATED_BODY()
public:
 void Initialize(FSubsystemCollectionBase& Collection) override;
};
/** Optional extension to the vanilla interaction tree; the original switch remains intact. */
UCLASS()
class FICSITLOGISTICSNETWORK_API UFILNStationControls : public UUserWidget {
 GENERATED_BODY()
public:
 void SetContext(AFGBuildableDockingStation* Station,UUserWidget* VanillaWidget);
 void ShowOverview(AFGBuildableDockingStation* Station,int32 Page,int32 Pages,const FText& Text);
 void RestoreLabels();
 void Reply(AFGBuildableDockingStation* Station,bool Enrolled,TSubclassOf<UFGItemDescriptor> Resource,const FText& Message);
 static void InstallHook();
protected:
 void NativeOnInitialized() override;
private:
 UPROPERTY() TObjectPtr<AFGBuildableDockingStation> Station;
 UPROPERTY() TObjectPtr<UComboBoxString> Selector;
 UPROPERTY() TObjectPtr<UTextBlock> Feedback;
 UPROPERTY() TObjectPtr<UComboBoxString> Vehicles;
 UPROPERTY() TObjectPtr<UTextBlock> Overview;
 int32 OverviewPage=0;
 int32 OverviewPages=1;
 UPROPERTY() TArray<TObjectPtr<AFGWheeledVehicleIdentifier>> VehicleChoices;
 UPROPERTY() TArray<TSubclassOf<UFGItemDescriptor>> Resources;
 UPROPERTY() TArray<TObjectPtr<UFILNLabelBinding>> LabelBindings;
 UFUNCTION() void QueryOverview();
 UFUNCTION() void NextPage();
 UFUNCTION() void PreviousPage();
 UFUNCTION() void EnrollVehicle();
 UFUNCTION() void RecoverVehicle();
 UFUNCTION() void RegisterDepot();
 UFUNCTION() void RemoveDepot();
 UFUNCTION() void ToggleDispatcher();
 UFUNCTION() void PrepareUninstall();
 UFUNCTION() void Apply();
 UFUNCTION() void Remove();
 UFILNRemoteControl* Remote() const;
};
