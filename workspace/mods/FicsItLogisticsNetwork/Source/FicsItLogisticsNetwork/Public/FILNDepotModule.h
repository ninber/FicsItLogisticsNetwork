// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "CoreMinimal.h"
#include "Module/GameWorldModule.h"
#include "FILNDepotModule.generated.h"

// Native root: SML discovers loaded native classes with bRootModule=true.
// No generated root Blueprint or dependency on its asset-registry tag.
UCLASS(NotBlueprintable)
class FICSITLOGISTICSNETWORK_API UFILNDepotModule : public UGameWorldModule {
 GENERATED_BODY()
public:
 UFILNDepotModule();
 virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;
};
