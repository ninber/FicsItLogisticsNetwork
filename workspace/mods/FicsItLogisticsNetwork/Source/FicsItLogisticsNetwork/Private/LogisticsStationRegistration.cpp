// SPDX-License-Identifier: GPL-3.0-only
#include "LogisticsSubsystem.h"
#include "FILNStationControls.h"
#include "FILNBlueprintSettings.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "FGInventoryComponent.h"

bool ALogisticsSubsystem::RemoveStation(AFGDockingStationIdentifier* Station) {
 if(!HasAuthority() || !bRegistryValidated || bDispatchBlocked || bPreparingUninstall || bChangingFilters || !IsValid(Station) || Station->GetWorld()!=GetWorld()) return false;
 const int32 Index=StationConfigs.IndexOfByPredicate([Station](const FFILNStationConfig& C){return C.Station==Station;});
 auto* B=Station->GetStation();
 if(!IsValid(B) || B->IsBuildableInsideBlueprintDesigner() || (FFILNStationSettings::IsDedicated(B) && !FFILNStationSettings::HasProperty(B))) return false;
 if(Index==INDEX_NONE) return !FFILNStationSettings::IsDedicated(B) || FFILNStationSettings::Write(B,nullptr);
 if(Reservations.StationBusy(TCHAR_TO_UTF8(*StationId(Station).ToString()))) return false;
 auto* I=B->GetInventory();
 const auto& C=StationConfigs[Index];
 if(!I || B->GetDockedActor() || (!C.ExternalFilter && I->GetSizeLinear()!=C.OriginalFilters.Num())) return false;
 if(!C.ExternalFilter && !ValidateStationSlots(C)) return false;
 if(!RestoreStationFilters(TArray<int32>{Index})) return false;
 if(auto* Marker=B->FindComponentByClass<UFILNStationMarker>()) Marker->DestroyComponent();
 StationConfigs.RemoveAt(Index); ResetScan();
 return !FFILNStationSettings::IsDedicated(B) || FFILNStationSettings::Write(B,nullptr);
}
