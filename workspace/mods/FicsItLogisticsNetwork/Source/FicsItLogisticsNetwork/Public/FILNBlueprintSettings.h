#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FILNBlueprintSettings.generated.h"
class AFGBuildableDockingStation;
class UFGItemDescriptor;
class UBlueprint;

// A SaveGame class property on the existing authored station actor, not an
// external world registry entry or a physical inventory restriction.
struct FFILNStationSettings {
 static bool IsDedicated(const AFGBuildableDockingStation* Station);
 static bool HasProperty(const AFGBuildableDockingStation* Station);
 static TSubclassOf<UFGItemDescriptor> Read(const AFGBuildableDockingStation* Station);
 static bool Write(AFGBuildableDockingStation* Station,TSubclassOf<UFGItemDescriptor> Resource);
};
UCLASS()
class FICSITLOGISTICSNETWORK_API UFILNBlueprintSettings : public UBlueprintFunctionLibrary {
 GENERATED_BODY()
public:
 // Editor authoring only. Never mutate a generated class in a shipping world.
 UFUNCTION(BlueprintCallable, Category="FILN|Authoring")
 static bool EnsureStationResourceProperty(UBlueprint* Blueprint,bool ValidateOnly);
};
