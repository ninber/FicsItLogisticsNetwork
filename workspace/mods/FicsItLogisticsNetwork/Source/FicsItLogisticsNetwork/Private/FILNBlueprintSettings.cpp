// SPDX-License-Identifier: GPL-3.0-only
#include "FILNBlueprintSettings.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "Resources/FGItemDescriptor.h"
#include "Engine/Blueprint.h"
#include "UObject/UnrealType.h"
#if WITH_EDITOR
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraphSchema_K2.h"
#endif
namespace {
const FName FILNResourcePropertyName(TEXT("FILNResource"));
FClassProperty* FILNFindResourceProperty(const UClass* Class) {
 auto* P=Class?FindFProperty<FClassProperty>(Class,FILNResourcePropertyName):nullptr;
 return P && P->MetaClass==UFGItemDescriptor::StaticClass() && P->HasAnyPropertyFlags(CPF_SaveGame)
  && !P->HasAnyPropertyFlags(CPF_Transient) ? P : nullptr;
}
}
bool FFILNStationSettings::IsDedicated(const AFGBuildableDockingStation* Station) {
 if(!IsValid(Station)) return false;
 const FString Path=Station->GetClass()->GetPathName();
 return Path==TEXT("/FicsItLogisticsNetwork/Depot/Build_FILNStation.Build_FILNStation_C") ||
        Path==TEXT("/FicsItLogisticsNetwork/Depot/Build_FILNFluidStation.Build_FILNFluidStation_C");
}
bool FFILNStationSettings::HasProperty(const AFGBuildableDockingStation* Station) {
 return IsDedicated(Station) && FILNFindResourceProperty(Station->GetClass());
}
TSubclassOf<UFGItemDescriptor> FFILNStationSettings::Read(const AFGBuildableDockingStation* Station) {
 if(!HasProperty(Station)) return nullptr;
 auto* P=FILNFindResourceProperty(Station->GetClass());
 return TSubclassOf<UFGItemDescriptor>(Cast<UClass>(P->GetObjectPropertyValue_InContainer(Station)));
}
bool FFILNStationSettings::Write(AFGBuildableDockingStation* Station,TSubclassOf<UFGItemDescriptor> Resource) {
 if(!HasProperty(Station) || !Station->HasAuthority()) return false;
 auto* P=FILNFindResourceProperty(Station->GetClass());
 P->SetObjectPropertyValue_InContainer(Station,Resource.Get());
 return Read(Station)==Resource;
}
bool UFILNBlueprintSettings::EnsureStationResourceProperty(UBlueprint* Blueprint,bool ValidateOnly) {
#if WITH_EDITOR
 if(!IsValid(Blueprint) || !Blueprint->GeneratedClass ||
    !Blueprint->GeneratedClass->IsChildOf(AFGBuildableDockingStation::StaticClass())) return false;
 if(FILNFindResourceProperty(Blueprint->GeneratedClass)) return true;
 if(ValidateOnly) return false;
 // Never replace a conflicting existing member or alter an inherited class.
 if(FindFProperty<FProperty>(Blueprint->GeneratedClass,FILNResourcePropertyName)) return false;
 FEdGraphPinType Pin;
 Pin.PinCategory=UEdGraphSchema_K2::PC_Class;
 Pin.PinSubCategoryObject=UFGItemDescriptor::StaticClass();
 if(!FBlueprintEditorUtils::AddMemberVariable(Blueprint,FILNResourcePropertyName,Pin)) return false;
 FBlueprintEditorUtils::SetVariableSaveGameFlag(Blueprint,FILNResourcePropertyName,true);
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
 FKismetEditorUtilities::CompileBlueprint(Blueprint);
 return FILNFindResourceProperty(Blueprint->GeneratedClass)!=nullptr;
#else
 return false;
#endif
}
