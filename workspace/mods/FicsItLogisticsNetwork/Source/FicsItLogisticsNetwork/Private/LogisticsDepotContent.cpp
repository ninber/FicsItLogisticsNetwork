// SPDX-License-Identifier: GPL-3.0-only
#include "LogisticsSubsystem.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "FGRecipe.h"
#include "FGRecipeManager.h"
#include "Registry/ModContentRegistry.h"
#include "Engine/World.h"

namespace {
constexpr const TCHAR* FILNSolidDepotPath=TEXT("/FicsItLogisticsNetwork/Depot/Recipe_FILNDepot.Recipe_FILNDepot_C");
constexpr const TCHAR* FILNFluidDepotPath=TEXT("/FicsItLogisticsNetwork/Depot/Recipe_FILNFluidDepot.Recipe_FILNFluidDepot_C");
constexpr const TCHAR* FILNFluidStationPath=TEXT("/FicsItLogisticsNetwork/Depot/Recipe_FILNFluidStation.Recipe_FILNFluidStation_C");
}
bool ALogisticsSubsystem::IsAuthoredDepot(const AFGDockingStationIdentifier* Station) {
 auto* B=IsValid(Station)?Station->GetStation():nullptr;
 if(!IsValid(B)) return false;
 const auto Recipe=B->GetBuiltWithRecipe();
 return Recipe && (Recipe->GetPathName()==FILNSolidDepotPath || Recipe->GetPathName()==FILNFluidDepotPath);
}
void ALogisticsSubsystem::EnsureDepotRecipe() {
 if(!HasAuthority() || !bRegistryValidated || bPreparingUninstall || bUninstallPrepared || bDepotRecipeReady) return;
 const double Now=GetWorld()->GetTimeSeconds();
 if(Now<NextDepotRecipeCheck) return;
 NextDepotRecipeCheck=Now+15.;
 auto* Registry=UModContentRegistry::Get(GetWorld());
 auto* Manager=AFGRecipeManager::Get(this);
 auto* BuildGun=FindObject<UClass>(nullptr,TEXT("/Game/FactoryGame/Equipment/BuildGun/BP_BuildGun.BP_BuildGun_C"));
 if(!Registry || !IsValid(Manager) || !BuildGun) return;
 bool AllReady=true;
 for(const TCHAR* Path : {FILNSolidDepotPath,FILNFluidDepotPath,FILNFluidStationPath}) {
  auto* Recipe=FindObject<UClass>(nullptr,Path);
  auto* Parent=Recipe?Recipe->GetSuperClass():nullptr;
  FGameObjectRegistration Entry;
  if(!Recipe || !Recipe->IsChildOf(UFGRecipe::StaticClass()) || !Parent ||
     !Parent->IsChildOf(UFGRecipe::StaticClass()) || !Registry->GetRecipeInfo(Recipe,Entry) ||
     !Manager->IsRecipeAvailable(Parent)) { AllReady=false; continue; }
  if(!Manager->IsRecipeAvailable(Recipe)) Manager->AddAvailableRecipe(Recipe);
  TArray<TSubclassOf<UFGRecipe>> Available;
  Manager->GetAvailableRecipesForProducer(BuildGun,Available);
  if(!Manager->IsRecipeAvailable(Recipe) || !Available.Contains(TSubclassOf<UFGRecipe>(Recipe))) AllReady=false;
 }
 // Solid recipe may become available earlier than fluid; continue checking
 // until all vanilla prerequisites are unlocked. Never load classes on tick.
 bDepotRecipeReady=AllReady;
 if(AllReady) UE_LOG(LogFILN,Log,TEXT("FILN_STATION_RECIPES_READY buildgun_listed=true; placement remains runtime acceptance"));
}
