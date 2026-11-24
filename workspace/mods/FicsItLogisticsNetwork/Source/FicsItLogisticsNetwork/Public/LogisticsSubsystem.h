// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "CoreMinimal.h"
#include "Subsystem/ModSubsystem.h"
#include "FGSaveInterface.h"
#include "Subsystems/WorldSubsystem.h"
#include "FILNCore.h"
#include "LogisticsSubsystem.generated.h"
class AFGWheeledVehicleIdentifier;
class AFGDockingStationIdentifier;
DECLARE_LOG_CATEGORY_EXTERN(LogFILN, Log, All);

USTRUCT()
struct FFILNStation {
 GENERATED_BODY()
 UPROPERTY(SaveGame) FGuid InstanceId;
 UPROPERTY(SaveGame) TObjectPtr<AFGDockingStationIdentifier> Station;
};
USTRUCT()
struct FFILNRegistration {
 GENERATED_BODY()
 UPROPERTY(SaveGame) FGuid TruckId;
 UPROPERTY(SaveGame) FGuid BayId;
 UPROPERTY(SaveGame) TObjectPtr<AFGWheeledVehicleIdentifier> Vehicle;
 UPROPERTY(SaveGame) TObjectPtr<AFGDockingStationIdentifier> Home;
 UPROPERTY(SaveGame) TObjectPtr<AFGDockingStationIdentifier> Provider;
 UPROPERTY(SaveGame) TObjectPtr<AFGDockingStationIdentifier> Requester;
 UPROPERTY(SaveGame) FGuid JobId;
 // Values correspond to filn::State; unknown values fail closed.
 UPROPERTY(SaveGame) uint8 State = 0;
 UPROPERTY(SaveGame) TArray<FGuid> RouteIntent;
 UPROPERTY(SaveGame) FString Diagnostic;
 UPROPERTY(SaveGame) float Elapsed = 0;
 double LastObserved = 0;
};
/** Native server-only prototype. Dedicated empty vanilla LOAD station is the home bay fallback. */
UCLASS()
class FICSITLOGISTICSNETWORK_API ALogisticsSubsystem : public AModSubsystem, public IFGSaveInterface {
 GENERATED_BODY()
public:
 ALogisticsSubsystem();
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual bool ShouldSave_Implementation() const override { return true; }
 virtual bool NeedTransform_Implementation() override { return false; }
 virtual void PostLoadGame_Implementation(int32 SaveVersion, int32 GameVersion) override;
 void Execute(const FString& Command, const TArray<FString>& Args);
private:
 UPROPERTY(SaveGame) int32 SchemaVersion = 1;
 UPROPERTY(SaveGame) TArray<FFILNStation> Stations;
 UPROPERTY(SaveGame) TArray<FFILNRegistration> Registrations;
 filn::Ownership Ownership;
 filn::Reservations Reservations;
 TMap<AFGWheeledVehicleIdentifier*, int32> ByVehicle;
 TMap<AFGDockingStationIdentifier*, FGuid> StationIds;
 // Contains only jobs that need observation, not idle fleet members.
 TArray<int32> Active;
 int32 Cursor = 0;
 bool bDispatchBlocked = false;
 FTimerHandle Watchdog;
 void Rebuild();
 void Observe();
 void Recover(FFILNRegistration& R, const FString& Why);
 bool Pair(AFGWheeledVehicleIdentifier* Vehicle, AFGDockingStationIdentifier* Home);
 bool Dispatch(FFILNRegistration& R, AFGDockingStationIdentifier* Provider, AFGDockingStationIdentifier* Requester);
 bool Parked(const FFILNRegistration& R) const;
 bool ValidHome(AFGDockingStationIdentifier* Home) const;
 FGuid StationId(AFGDockingStationIdentifier* Station);
};

/** Registers the saved SML actor once, after game world actor initialization. */
UCLASS()
class FICSITLOGISTICSNETWORK_API ULogisticsBootstrap : public UWorldSubsystem {
 GENERATED_BODY()
public:
 virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
 virtual void OnWorldBeginPlay(UWorld& World) override;
};
