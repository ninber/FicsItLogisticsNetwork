// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FILNStationControls.generated.h"

// Runtime marker retained for existing registry compatibility; no UI hooks.
UCLASS()
class FICSITLOGISTICSNETWORK_API UFILNStationMarker : public UActorComponent {
 GENERATED_BODY()
public:
 UFILNStationMarker() { SetIsReplicatedByDefault(true); PrimaryComponentTick.bCanEverTick=false; }
};
