// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "CoreMinimal.h"
#include "Async/Future.h"
#include "Subsystem/ModSubsystem.h"
#include "FGSaveInterface.h"
#include "Subsystems/WorldSubsystem.h"
#include "FILNCore.h"
#include "FILNRoadCache.h"
#include "FILNDispatchPolicy.h"
#include "FILNRecovery.h"
#include "FILNFuelPolicy.h"
#include "FILNIdlePolicy.h"
#include "LogisticsSubsystem.generated.h"
class AFGWheeledVehicleIdentifier;
class AFGWheeledVehicle;
class AFGDockingStationIdentifier;
class UFGItemDescriptor;
class UFILNTransferObserver;
class AFGBuildableDockingStation;
class FJsonObject;
class FJsonValue;
DECLARE_LOG_CATEGORY_EXTERN(LogFILN, Log, All);

USTRUCT()
struct FFILNStation {
 GENERATED_BODY()
 UPROPERTY(SaveGame) FGuid InstanceId;
 UPROPERTY(SaveGame) TObjectPtr<AFGDockingStationIdentifier> Station;
};
USTRUCT()
struct FFILNStationConfig {
 GENERATED_BODY()
 UPROPERTY(SaveGame) TObjectPtr<AFGDockingStationIdentifier> Station;
 UPROPERTY(SaveGame) TSubclassOf<UFGItemDescriptor> Resource;
 // Preserve pre-FILN slot restrictions for explicit uninstall preparation.
 UPROPERTY(SaveGame) TArray<TSubclassOf<UFGItemDescriptor>> OriginalFilters;
 // Retained only to deserialize old own-state records without treating an
 // unowned filter as an owned one. Never create new records with this flag.
 UPROPERTY(SaveGame) bool ExternalFilter=false;
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
 UPROPERTY(SaveGame) bool RetainedCargo = false;
 UPROPERTY(SaveGame) bool CargoOriginKnown = false;
 UPROPERTY(SaveGame) bool CargoOriginStorage = false;
 UPROPERTY(SaveGame) bool Automatic = false;
 UPROPERTY(SaveGame) bool AbortReturn = false;
 UPROPERTY(SaveGame) bool DepartedHome = false;
 // Stable reservation identities survive endpoint destruction and save/load.
 UPROPERTY(SaveGame) FGuid ProviderLease;
 UPROPERTY(SaveGame) FGuid RequesterLease;
 UPROPERTY(SaveGame) bool Held = false;
 UPROPERTY(SaveGame) bool Relocating = false;
 UPROPERTY(SaveGame) TArray<FGuid> IdleRoute;
 UPROPERTY(SaveGame) TSubclassOf<UFGItemDescriptor> Resource;
 UPROPERTY(SaveGame) int32 ExpectedCargo = 0;
 UPROPERTY(SaveGame) int32 LoadedCargo = 0;
 UPROPERTY(SaveGame) int32 DeliveredCargo = 0;
 UPROPERTY(SaveGame) int32 LastCargo = 0;
 double LastObserved = 0;
 double RouteActivationDeadline = 0;
 double ReceiptMismatchSince=-1;
 // Runtime-only clock: saved journey identity and phase remain authoritative on reload.
 filn::recovery::RetryClock NavigationRetry;
 filn::recovery::Progress TravelProgress;
 FVector ProgressLocation=FVector::ZeroVector;
 int32 ProgressWaypoint=INDEX_NONE;
 // Runtime-only native fuel service observation; inventory is authoritative.
 bool FuelPending=false;
 TSubclassOf<UFGItemDescriptor> ObservedFuel;
 int32 FuelBefore=0;
 double FuelStarted=0, FuelRetryAt=0;
};
/** Native server-only prototype. Dedicated empty vanilla LOAD station is the home bay fallback. */
UCLASS()
class FICSITLOGISTICSNETWORK_API ALogisticsSubsystem : public AModSubsystem, public IFGSaveInterface {
 GENERATED_BODY()
public:
 ALogisticsSubsystem();
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual bool ShouldSave_Implementation() const override;
 virtual bool NeedTransform_Implementation() override { return false; }
 virtual void PostLoadGame_Implementation(int32 SaveVersion, int32 GameVersion) override;
 void Execute(const FString& Command, const TArray<FString>& Args);
 bool HoldVehicle(AFGWheeledVehicleIdentifier* Vehicle);
 static void InstallParkingHook();
 static void InstallDockReceiptObserver();
private:
 UPROPERTY(SaveGame) int32 SchemaVersion = 1;
 UPROPERTY(SaveGame) TArray<FFILNStation> Stations;
 UPROPERTY(SaveGame) TArray<FFILNStationConfig> StationConfigs;
 UPROPERTY(SaveGame) TArray<TObjectPtr<AFGDockingStationIdentifier>> DepotStations;
 UPROPERTY(SaveGame) TArray<FFILNRegistration> Registrations;
 friend class UFILNTransferObserver;
 UPROPERTY() TArray<TObjectPtr<UFILNTransferObserver>> TransferObservers;
 TMap<FGuid,TWeakObjectPtr<UFILNTransferObserver>> TransferByTruck;
 bool WatchTransfers(FFILNRegistration& R);
 bool HasTransferObserver(const FFILNRegistration& R) const;
 void Transfer(AFGWheeledVehicleIdentifier* Vehicle,TSubclassOf<UFGItemDescriptor> Item,int32 Amount,class UFGInventoryComponent* Other,bool Added);
 bool ArmDockReceipts(FFILNRegistration& Registration);
 void DisarmDockReceipts(const FGuid& Job);
 void ClearDockReceipts();
 void ConsumeDockReceipts();
 bool HasPendingDockReceipts(const FGuid& Job) const;
 TSet<TWeakObjectPtr<AFGDockingStationIdentifier>> HomeStations;
 TSet<TWeakObjectPtr<AFGDockingStationIdentifier>> DepotIndex;
 void TrackDepot(AFGDockingStationIdentifier* Station);
 bool SetDepot(AFGDockingStationIdentifier* Station,bool Remove);
 uint64 ScanRoadRevision=0;
 int32 FuelCursor=0;
 bool RemoveStation(AFGDockingStationIdentifier* Station);
 void ServiceFuel();
 bool FuelReady(const FFILNRegistration& R) const;
 filn::Ownership Ownership;
 filn::Reservations Reservations;
 TMap<AFGWheeledVehicleIdentifier*, int32> ByVehicle;
 TMap<TWeakObjectPtr<AFGDockingStationIdentifier>, FGuid> StationIds;
 TSet<FGuid> StationIdentityIndex;
 bool bRegistryValidated=false;
 // Contains only jobs that need observation, not idle fleet members.
 TArray<int32> Active;
 int32 Cursor = 0;
 bool bDispatchBlocked = false;
 FTimerHandle Watchdog;
 FTimerHandle DispatchTimer;
 FTimerHandle DeferredRebuild;
 UPROPERTY(SaveGame) bool bAutoDispatchEnabled = true;
 FFILNRoadCache RoadCache;
 int32 RequestCursor=0, ScanVehicle=0, ScanProvider=-1;
 int32 BestVehicle=INDEX_NONE, BestProvider=INDEX_NONE;
 double FirstEligibleProviderDistance=-1;
 struct FProviderRanking { int32 Cursor=0; bool Sorted=false; TArray<TPair<double,int32>> Entries; };
 TMap<const UFGVehiclePathPreset*,FProviderRanking> Rankings;
 // Snapshot native identifiers, not stock. Weak references survive deletion during a scan.
 TArray<TWeakObjectPtr<AFGDockingStationIdentifier>> ProviderCandidates;
 bool bCheckingRetained=true;
 bool bScanning=false;
 double NextDispatch=0;
 std::optional<filn::dispatch::Choice> BestOffer;
 void Schedule();
 void SampleTelemetry();
 void ResetTelemetry();
 double NextTelemetryDemandCheck=0, NextTelemetryStart=0;
 bool bTelemetryDemand=false, bTelemetryCollecting=false;
 int32 TelemetryStationCursor=0, TelemetryVehicleCursor=0;
 int64 TelemetryStarted=0, TelemetrySequence=0;
 FString TelemetryWorld;
 TArray<TWeakObjectPtr<AFGDockingStationIdentifier>> TelemetryStationQueue;
 TArray<TWeakObjectPtr<AFGWheeledVehicleIdentifier>> TelemetryVehicleQueue;
 TArray<TSharedPtr<FJsonValue>> TelemetryStations, TelemetryVehicles, TelemetryErrors, TelemetryCapacityRows;
 TArray<TWeakObjectPtr<UClass>> TelemetryResources;
 TSet<TWeakObjectPtr<UClass>> TelemetryResourceSet;
 struct FTelemetryCapacity {
  TWeakObjectPtr<class UFGInventoryComponent> Inventory;
  int32 Slots=0;
  TSharedPtr<FJsonObject> ByItem;
  bool Fluid=false;
 };
 TArray<FTelemetryCapacity> TelemetryTypes;
 TSet<FString> TelemetryTypeKeys;
 int32 TelemetryTypeCursor=0, TelemetryResourceCursor=0;
 TFuture<bool> TelemetryWrite;
 void DiscoverDepots();
 void EnsureDepotRecipe();
 static bool IsAuthoredDepot(const AFGDockingStationIdentifier* Station);
 double NextDepotRecipeCheck=0;
 bool bDepotRecipeReady=false;
 void DiscoverHomeVehicles();
 bool AdoptPlayerHome(AFGWheeledVehicleIdentifier* Vehicle,AFGDockingStationIdentifier* Requested);
 bool StationResourceMatches(const FFILNStationConfig& Config) const;
 static bool StationAcceptsResource(AFGBuildableDockingStation* Station,TSubclassOf<UFGItemDescriptor> Resource);
 static bool VehicleStationCompatible(AFGWheeledVehicle* Vehicle,AFGBuildableDockingStation* Station);
 TArray<TWeakObjectPtr<AFGDockingStationIdentifier>> PlayerStationScan;
 TArray<TWeakObjectPtr<AFGWheeledVehicleIdentifier>> PlayerVehicleScan;
 int32 PlayerStationCursor=0, PlayerVehicleCursor=0;
 double NextPlayerStationScan=0, NextPlayerVehicleScan=0;
 TMap<TWeakObjectPtr<AFGWheeledVehicleIdentifier>,int32> OverflowCursor;
 TMap<TWeakObjectPtr<AFGWheeledVehicleIdentifier>,FString> PendingHomeReason;
 bool BuildOffer(int32 VehicleIndex,int32 RequesterIndex,int32 ProviderIndex,filn::dispatch::Offer& Offer,bool& Pending);
 bool CommitOffer(int32 VehicleIndex,int32 RequesterIndex,int32 ProviderIndex);
 void ResetScan();
 void Rebuild();
 bool ConfigureStation(AFGDockingStationIdentifier* Station, TSubclassOf<UFGItemDescriptor> Resource);
 bool PrepareStationsForUninstall();
 bool RestoreStationFilters(const TArray<int32>& Indices);
 bool ApplyStationFilter(const FFILNStationConfig& Config,TSubclassOf<UFGItemDescriptor> PriorResource=nullptr);
 bool ChangeStationFilters(const TArray<FFILNStationConfig>& Configs,bool Restore,TSubclassOf<UFGItemDescriptor> PriorResource=nullptr);
 bool bChangingFilters=false;
 bool bPreparingUninstall=false;
 UPROPERTY(SaveGame) bool bUninstallPrepared=false;
 bool SetDispatchEnabled(bool Enabled);
 bool EndpointConfigurationValid(const FFILNRegistration& R) const;
 void Observe();
 bool RetryNavigation(FFILNRegistration& R, double Now, bool Stalled=false);
 bool RestoreHomeRoute(FFILNRegistration& R);
 bool RehomeParkedVehicle(int32 Index,AFGDockingStationIdentifier* Home);
 bool TryAbortToHome(FFILNRegistration& R,double Now);
 bool RestoreJobReservation(FFILNRegistration& R);
 void Recover(FFILNRegistration& R, const FString& Why);
 struct FDestroyedVehicleProof {
  TWeakObjectPtr<AFGWheeledVehicle> BoundOwner, LastOwner;
 };
 TMap<FGuid,FDestroyedVehicleProof> DestroyedVehicles;
 void NoteVehicleDestroyed(const FGuid& Truck,AFGWheeledVehicleIdentifier* Vehicle,TWeakObjectPtr<AFGWheeledVehicle> BoundOwner);
 bool RemoveLostRegistration(int32 Index);
 bool RemoveRegistrationAt(int32 Index);
 bool UnpairParkedVehicle(int32 Index,AFGDockingStationIdentifier* Home);
 void AuditLostRegistrations();
 void AuditIdleFleet();
 int32 IdleCursor=0;
 int32 LostCursor=0;
 bool Pair(AFGWheeledVehicleIdentifier* Vehicle, AFGDockingStationIdentifier* Home);
 bool Dispatch(FFILNRegistration& R, AFGDockingStationIdentifier* Provider, AFGDockingStationIdentifier* Requester);
 bool Parked(const FFILNRegistration& R) const;
 bool ValidHome(AFGDockingStationIdentifier* Home) const;
 bool IsDepot(AFGDockingStationIdentifier* Station) const;
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
