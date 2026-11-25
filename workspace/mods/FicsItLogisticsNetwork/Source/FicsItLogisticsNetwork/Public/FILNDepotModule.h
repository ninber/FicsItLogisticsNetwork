// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "CoreMinimal.h"
#include "Module/GameWorldModule.h"
#include "FILNDepotModule.generated.h"
class UFGRecipe;

// The authored root Blueprint hard-references the recipe, ensuring it is cooked.
// Register in SML Initialization, not after the registry has frozen.
UCLASS(Blueprintable)
class FICSITLOGISTICSNETWORK_API UFILNDepotModule : public UGameWorldModule {
 GENERATED_BODY()
public:
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FILN")
 TSubclassOf<UFGRecipe> DepotRecipe;
 virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;
};
