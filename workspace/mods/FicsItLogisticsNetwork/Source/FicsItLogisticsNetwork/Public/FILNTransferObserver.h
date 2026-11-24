// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "CoreMinimal.h"
#include "FILNTransferObserver.generated.h"
class UFGItemDescriptor;
class UFGInventoryComponent;
class AFGWheeledVehicleIdentifier;
class ALogisticsSubsystem;
UCLASS()
class FICSITLOGISTICSNETWORK_API UFILNTransferObserver : public UObject {
 GENERATED_BODY()
public:
 void Bind(ALogisticsSubsystem* Subsystem,AFGWheeledVehicleIdentifier* Vehicle,UFGInventoryComponent* Inventory);
 void Unbind();
 bool IsBoundTo(AFGWheeledVehicleIdentifier* V) const;
private:
 TWeakObjectPtr<ALogisticsSubsystem> Owner;
 TWeakObjectPtr<AFGWheeledVehicleIdentifier> Vehicle;
 TWeakObjectPtr<UFGInventoryComponent> Inventory;
 UFUNCTION() void Added(TSubclassOf<UFGItemDescriptor> Item,int32 Amount,UFGInventoryComponent* Other);
 UFUNCTION() void Removed(TSubclassOf<UFGItemDescriptor> Item,int32 Amount,UFGInventoryComponent* Other);
};
