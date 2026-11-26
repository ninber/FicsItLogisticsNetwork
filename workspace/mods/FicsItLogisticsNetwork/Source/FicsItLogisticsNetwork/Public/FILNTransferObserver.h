#pragma once
#include "CoreMinimal.h"
#include "FILNTransferObserver.generated.h"
class UFGItemDescriptor;
class UFGInventoryComponent;
class AFGWheeledVehicleIdentifier;
class AFGWheeledVehicle;
class ALogisticsSubsystem;
UCLASS()
class FICSITLOGISTICSNETWORK_API UFILNTransferObserver : public UObject {
 GENERATED_BODY()
public:
 void Bind(ALogisticsSubsystem* Subsystem,AFGWheeledVehicleIdentifier* Vehicle,UFGInventoryComponent* Inventory,const FGuid& Truck);
 void Unbind();
 bool IsBoundToTruck(const FGuid& Truck) const { return TruckId==Truck; }
 bool Matches(AFGWheeledVehicleIdentifier* V,AFGWheeledVehicle* O,UFGInventoryComponent* I) const;
private:
 void Forward(TSubclassOf<UFGItemDescriptor> Item,int32 Amount,UFGInventoryComponent* Other,bool Added);
 TWeakObjectPtr<ALogisticsSubsystem> Owner;
 TWeakObjectPtr<AFGWheeledVehicleIdentifier> Vehicle;
 TWeakObjectPtr<UFGInventoryComponent> Inventory;
 TWeakObjectPtr<AFGWheeledVehicle> BoundOwner;
 FGuid TruckId;
 UFUNCTION() void Destroyed(AActor* Actor);
 UFUNCTION() void Added(TSubclassOf<UFGItemDescriptor> Item,int32 Amount,UFGInventoryComponent* Other);
 UFUNCTION() void Removed(TSubclassOf<UFGItemDescriptor> Item,int32 Amount,UFGInventoryComponent* Other);
};
