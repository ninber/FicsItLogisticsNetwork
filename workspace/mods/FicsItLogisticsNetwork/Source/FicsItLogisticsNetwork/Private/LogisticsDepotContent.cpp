// SPDX-License-Identifier: GPL-3.0-only
#include "LogisticsSubsystem.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "FGRecipe.h"
#include "FGRecipeManager.h"
#include "Registry/ModContentRegistry.h"
#include "Engine/World.h"

namespace {
constexpr const TCHAR* DepotRecipePath=TEXT("/FicsItLogisticsNetwork/Depot/Recipe_FILNDepot.Recipe_FILNDepot_C");
constexpr const TCHAR* TruckRecipePath=TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_TruckStation.Recipe_TruckStation_C");
}
bool ALogisticsSubsystem::IsAuthoredDepot(const AFGDockingStationIdentifier* Station) {
 auto* B=IsValid(Station)?Station->GetStation():nullptr;
 if(!IsValid(B)) return false;
 const auto Recipe=B->GetBuiltWithRecipe();
 return Recipe && Recipe->GetPathName()==DepotRecipePath;
}
void ALogisticsSubsystem::EnsureDepotRecipe() {
 if(!HasAuthority() || !bRegistryValidated || bPreparingUninstall || bUninstallPrepared || bDepotRecipeReady) return;
 const double Now=GetWorld()->GetTimeSeconds();
 if(Now<NextDepotRecipeCheck) return;
 NextDepotRecipeCheck=Now+15.;
 // No LoadClass on the game tick: the authored SML root module owns the
 // hard reference and registers during Initialization. Missing content fails closed.
 auto* Depot=FindObject<UClass>(nullptr,DepotRecipePath);
 auto* Truck=FindObject<UClass>(nullptr,TruckRecipePath);
 if(!Depot || !Depot->IsChildOf(UFGRecipe::StaticClass()) || !Truck || !Truck->IsChildOf(UFGRecipe::StaticClass())) return;
 auto* Registry=UModContentRegistry::Get(GetWorld());
 FGameObjectRegistration Entry;
 if(!Registry || !Registry->GetRecipeInfo(Depot,Entry)) return;
 auto* Manager=AFGRecipeManager::Get(this);
 // A depot unlocks only when the original truck station is available.
 if(!IsValid(Manager) || !Manager->IsRecipeAvailable(Truck)) return;
 if(!Manager->IsRecipeAvailable(Depot)) Manager->AddAvailableRecipe(Depot);
 auto* BuildGun=FindObject<UClass>(nullptr,TEXT("/Game/FactoryGame/Equipment/BuildGun/BP_BuildGun.BP_BuildGun_C"));
 if(!BuildGun || !Manager->IsRecipeAvailable(Depot)) return;
 TArray<TSubclassOf<UFGRecipe>> Recipes;
 Manager->GetAvailableRecipesForProducer(BuildGun,Recipes);
 bDepotRecipeReady=Recipes.Contains(TSubclassOf<UFGRecipe>(Depot));
 if(bDepotRecipeReady) UE_LOG(LogFILN,Log,TEXT("FILN_DEPOT_RECIPE_READY buildgun_listed=true; actual placement remains runtime acceptance"));
}
