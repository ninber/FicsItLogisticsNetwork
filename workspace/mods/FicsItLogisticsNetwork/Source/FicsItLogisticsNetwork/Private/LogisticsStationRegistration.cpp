// SPDX-License-Identifier: GPL-3.0-only
#include "LogisticsSubsystem.h"
#include "FILNStationControls.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "FGInventoryComponent.h"

bool ALogisticsSubsystem::RemoveStation(AFGDockingStationIdentifier* Station) {
 if(!HasAuthority() || !bRegistryValidated || bDispatchBlocked || bPreparingUninstall || bChangingFilters || !IsValid(Station) || Station->GetWorld()!=GetWorld()) return false;
 const int32 Index=StationConfigs.IndexOfByPredicate([Station](const FFILNStationConfig& C){return C.Station==Station;});
 if(Index==INDEX_NONE) return true;
 if(Reservations.StationBusy(TCHAR_TO_UTF8(*StationId(Station).ToString()))) return false;
 auto* B=Station->GetStation(); auto* I=IsValid(B)?B->GetInventory():nullptr;
 const auto& C=StationConfigs[Index];
 if(!I || B->GetDockedActor() || (!C.ExternalFilter && I->GetSizeLinear()!=C.OriginalFilters.Num())) return false;
 if(!RestoreStationFilters(TArray<int32>{Index})) return false;
 if(auto* Marker=B->FindComponentByClass<UFILNStationMarker>()) Marker->DestroyComponent();
 StationConfigs.RemoveAt(Index); ResetScan(); return true;
}
