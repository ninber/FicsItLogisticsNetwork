// SPDX-License-Identifier: GPL-3.0-only
#include "LogisticsSubsystem.h"
#include "FILNBlueprintSettings.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "FGRecipe.h"
#include "Resources/FGItemDescriptor.h"
#include "Resources/FGBuildingDescriptor.h"
#include "FGRecipeManager.h"
#include "Registry/ModContentRegistry.h"
#include "Engine/World.h"

namespace {
constexpr const TCHAR* FILNSolidDepotPath=TEXT("/FicsItLogisticsNetwork/Depot/Recipe_FILNDepot.Recipe_FILNDepot_C");
constexpr const TCHAR* FILNFluidDepotPath=TEXT("/FicsItLogisticsNetwork/Depot/Recipe_FILNFluidDepot.Recipe_FILNFluidDepot_C");
constexpr const TCHAR* FILNSolidStationPath=TEXT("/FicsItLogisticsNetwork/Depot/Recipe_FILNStation.Recipe_FILNStation_C");
constexpr const TCHAR* FILNFluidStationPath=TEXT("/FicsItLogisticsNetwork/Depot/Recipe_FILNFluidStation.Recipe_FILNFluidStation_C");
}
bool ALogisticsSubsystem::IsAuthoredDepot(const AFGDockingStationIdentifier* Station) {
 auto* B=IsValid(Station)?Station->GetStation():nullptr;
 if(!IsValid(B)) return false;
 const auto Recipe=B->GetBuiltWithRecipe();
 const FString BuildClass=B->GetClass()->GetPathName();
 // New depot placements have their own actor identity. The recipe identity
 // also recognizes older vanilla-actor FILN depots without rewriting saves.
 return BuildClass==TEXT("/FicsItLogisticsNetwork/Depot/Build_FILNDepot.Build_FILNDepot_C") ||
  BuildClass==TEXT("/FicsItLogisticsNetwork/Depot/Build_FILNFluidDepot.Build_FILNFluidDepot_C") ||
  (Recipe && (Recipe->GetPathName()==FILNSolidDepotPath || Recipe->GetPathName()==FILNFluidDepotPath));
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
 const auto Reject=[&](const TCHAR* Path,const FString& Reason) {
  AllReady=false;
  const FString Key=FString(Path)+TEXT(": ")+Reason;
  if(!DepotRecipeFailures.Contains(Key)) {
   DepotRecipeFailures.Add(Key);
   UE_LOG(LogFILN,Error,TEXT("FILN_STATION_RECIPE_REJECTED %s"),*Key);
  }
 };
 for(const TCHAR* Path : {FILNSolidDepotPath,FILNFluidDepotPath,FILNFluidStationPath,FILNSolidStationPath}) {
  auto* Recipe=FindObject<UClass>(nullptr,Path);
  auto* Parent=Recipe?Recipe->GetSuperClass():nullptr;
  FGameObjectRegistration Entry;
  if(!Recipe || !Recipe->IsChildOf(UFGRecipe::StaticClass()) || !Parent ||
     !Parent->IsChildOf(UFGRecipe::StaticClass()) || !Registry->GetRecipeInfo(Recipe,Entry)) {
   Reject(Path,TEXT("Required recipe or registration missing")); continue;
  }
  if(!Manager->IsRecipeAvailable(Parent)) { AllReady=false; continue; }
  const FString Expected=Path==FILNSolidDepotPath?TEXT("FILN Depot"):
    Path==FILNFluidDepotPath?TEXT("FILN Fluid Depot"):
    Path==FILNFluidStationPath?TEXT("FILN Fluid Logistics Station"):TEXT("FILN Logistics Station");
  const auto Products=UFGRecipe::GetProducts(Recipe);
  if(Products.Num()!=1 || !Products[0].ItemClass || Products[0].Amount!=1) {
   Reject(Path,TEXT("Invalid native recipe product")); continue;
  }
  const auto* ProductClass=Products[0].ItemClass.Get();
  if(!ProductClass || !ProductClass->IsChildOf(UFGBuildingDescriptor::StaticClass())) {
   Reject(Path,TEXT("Recipe product is not a building descriptor")); continue;
  }
  const auto BuildClass=UFGBuildingDescriptor::GetBuildableClass(TSubclassOf<UFGBuildingDescriptor>(Products[0].ItemClass.Get()));
  const TCHAR* ExpectedActor=Path==FILNSolidDepotPath?TEXT("/FicsItLogisticsNetwork/Depot/Build_FILNDepot.Build_FILNDepot_C"):
    Path==FILNFluidDepotPath?TEXT("/FicsItLogisticsNetwork/Depot/Build_FILNFluidDepot.Build_FILNFluidDepot_C"):
    Path==FILNFluidStationPath?TEXT("/FicsItLogisticsNetwork/Depot/Build_FILNFluidStation.Build_FILNFluidStation_C"):
    TEXT("/FicsItLogisticsNetwork/Depot/Build_FILNStation.Build_FILNStation_C");
  if(!BuildClass || !BuildClass->IsChildOf(AFGBuildableDockingStation::StaticClass()) || BuildClass->GetPathName()!=ExpectedActor) {
   Reject(Path,FString::Printf(TEXT("Expected buildable '%s'; runtime descriptor builds '%s'"),ExpectedActor,BuildClass?*BuildClass->GetPathName():TEXT("<null>"))); continue;
  }
  if(Path==FILNSolidStationPath || Path==FILNFluidStationPath) {
   if(!FFILNStationSettings::HasProperty(Cast<AFGBuildableDockingStation>(BuildClass->GetDefaultObject()))) {
    Reject(Path,TEXT("Cooked station missing FILNResource SaveGame class property; reauthor assets and repackage")); continue;
   }
  }
  const FString RecipeName=UFGRecipe::GetRecipeName(Recipe).ToString();
  const FString ItemName=UFGItemDescriptor::GetItemName(Products[0].ItemClass).ToString();
  if(RecipeName!=Expected || ItemName!=Expected) {
   Reject(Path,FString::Printf(TEXT("Expected '%s'; effective recipe='%s', descriptor='%s'"),*Expected,*RecipeName,*ItemName)); continue;
  }
  if(!Manager->IsRecipeAvailable(Recipe)) Manager->AddAvailableRecipe(Recipe);
  TArray<TSubclassOf<UFGRecipe>> Available;
  Manager->GetAvailableRecipesForProducer(BuildGun,Available);
  if(!Manager->IsRecipeAvailable(Recipe) || !Available.Contains(TSubclassOf<UFGRecipe>(Recipe))) Reject(Path,TEXT("Recipe not available to the build gun after registration"));
 }
 // Solid recipe may become available earlier than fluid; continue checking
 // until all vanilla prerequisites are unlocked. Never load classes on tick.
 bDepotRecipeReady=AllReady;
 if(AllReady) UE_LOG(LogFILN,Log,TEXT("FILN_STATION_RECIPES_READY effective_recipe_descriptor_names_and_build_class_verified=true; actual menu and placement remain runtime acceptance"));
}
