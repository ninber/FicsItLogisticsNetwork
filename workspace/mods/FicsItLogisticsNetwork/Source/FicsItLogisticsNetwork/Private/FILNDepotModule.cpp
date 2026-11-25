// SPDX-License-Identifier: GPL-3.0-only
#include "FILNDepotModule.h"
#include "LogisticsSubsystem.h"
#include "FGRecipe.h"
#include "Registry/ModContentRegistry.h"
#include "UObject/UObjectGlobals.h"

UFILNDepotModule::UFILNDepotModule() { bRootModule=true; }

void UFILNDepotModule::DispatchLifecycleEvent(ELifecyclePhase Phase) {
 if(Phase==ELifecyclePhase::INITIALIZATION) {
  auto* Registry=UModContentRegistry::Get(GetWorld());
  for(const TCHAR* Path : {
   TEXT("/FicsItLogisticsNetwork/Depot/Recipe_FILNDepot.Recipe_FILNDepot_C"),
   TEXT("/FicsItLogisticsNetwork/Depot/Recipe_FILNFluidDepot.Recipe_FILNFluidDepot_C"),
   TEXT("/FicsItLogisticsNetwork/Depot/Recipe_FILNFluidStation.Recipe_FILNFluidStation_C")}) {
   // Load only during initialization, never on the scheduler tick.
   auto* Recipe=LoadClass<UFGRecipe>(nullptr,Path);
   if(Registry && Recipe) Registry->RegisterRecipe(GetOwnerModReference(),Recipe);
   else UE_LOG(LogFILN,Error,TEXT("FILN_STATION_CONTENT_MISSING %s"),Path);
  }
 }
 Super::DispatchLifecycleEvent(Phase);
}
