// SPDX-License-Identifier: GPL-3.0-only
#include "FILNDepotModule.h"
#include "LogisticsSubsystem.h"
#include "FGRecipe.h"
#include "Registry/ModContentRegistry.h"

void UFILNDepotModule::DispatchLifecycleEvent(ELifecyclePhase Phase) {
 if(Phase==ELifecyclePhase::INITIALIZATION) {
  auto* Registry=UModContentRegistry::Get(GetWorld());
  if(Registry && DepotRecipe) Registry->RegisterRecipe(GetOwnerModReference(),DepotRecipe);
  else UE_LOG(LogFILN,Error,TEXT("FILN_DEPOT_CONTENT_MISSING: generated root module has no recipe or registry"));
 }
 Super::DispatchLifecycleEvent(Phase);
}
