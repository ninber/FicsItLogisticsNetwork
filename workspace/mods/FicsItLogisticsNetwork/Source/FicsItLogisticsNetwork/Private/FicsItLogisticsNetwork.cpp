#include "LogisticsSubsystem.h"
#include "FILNStationControls.h"
#include "FILNOrganizer.h"
#include "FGHUD.h"
#include "WheeledVehicles/FGVehicleSubsystem.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "GameFramework/PlayerController.h"
#include "Modules/ModuleManager.h"
#include "HAL/IConsoleManager.h"
#include "Subsystem/SubsystemActorManager.h"
#include "ModLoading/PluginModuleLoader.h"
#include "Engine/World.h"

bool ULogisticsBootstrap::ShouldCreateSubsystem(UObject* Outer) const {
 auto* World=Cast<UWorld>(Outer);
 return World && World->IsGameWorld() && World->GetNetMode()!=NM_Client &&
  FPluginModuleLoader::ShouldLoadModulesForWorld(World) && !FPluginModuleLoader::IsMainMenuWorld(World);
}
void ULogisticsBootstrap::OnWorldBeginPlay(UWorld& World) {
 Super::OnWorldBeginPlay(World);
 if(auto* Manager=World.GetSubsystem<USubsystemActorManager>()) Manager->RegisterSubsystemActor(ALogisticsSubsystem::StaticClass());
 else UE_LOG(LogFILN,Error,TEXT("FILN_NATIVE_SUBSYSTEM_MANAGER_MISSING"));
}
class FFicsItLogisticsNetworkModule : public IModuleInterface {
 TArray<IConsoleObject*> Commands;
public:
 virtual void StartupModule() override {
  UE_LOG(LogFILN,Log,TEXT("FILN_NATIVE_MODULE_STARTUP"));
  UFILNResourceSelector::InstallHook();
  UFILNOrganizer::InstallInput();
  Commands.Add(IConsoleManager::Get().RegisterConsoleCommand(TEXT("filn.organizer"),TEXT("Open read-only FILN Organizer"),
   FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&,UWorld* World) {
    if(World) UFILNOrganizer::Open(World->GetFirstPlayerController());
   }),ECVF_Default));
  ALogisticsSubsystem::InstallParkingHook();
  ALogisticsSubsystem::InstallDepotWarningHook();
  ALogisticsSubsystem::InstallDockReceiptObserver();
  for(const FString& Verb:TArray<FString>{TEXT("swatch"),TEXT("list"),TEXT("pair"),TEXT("unpair"),TEXT("dispatch"),TEXT("reset"),TEXT("rehome"),TEXT("status"),TEXT("station"),TEXT("prepareUninstall"),TEXT("enable"),TEXT("depot"),TEXT("removeStation")}) {
   Commands.Add(IConsoleManager::Get().RegisterConsoleCommand(
    *(TEXT("filn.")+Verb),TEXT("FILN server prototype; see README for exact object-path arguments"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([Verb](const TArray<FString>& Args,UWorld* World) {
     if(!World || World->GetNetMode()==NM_Client || !World->IsGameWorld() || FPluginModuleLoader::IsMainMenuWorld(World)) {
      UE_LOG(LogFILN,Warning,TEXT("Command requires authoritative game world")); return;
     }
     auto* Manager=World->GetSubsystem<USubsystemActorManager>();
     auto* Subsystem=Manager?Manager->GetSubsystemActor<ALogisticsSubsystem>():nullptr;
     if(Subsystem) Subsystem->Execute(Verb,Args);
     else UE_LOG(LogFILN,Error,TEXT("Subsystem unavailable: inspect bootstrap registration and save lifecycle"));
    }),ECVF_Default));
  }
 }
 virtual void ShutdownModule() override {
  UFILNOrganizer::RemoveInput();
  for(auto* Command:Commands) IConsoleManager::Get().UnregisterConsoleObject(Command,false);
  Commands.Empty();
 }
};
IMPLEMENT_MODULE(FFicsItLogisticsNetworkModule,FicsItLogisticsNetwork)
