// SPDX-License-Identifier: GPL-3.0-only
#include "LogisticsSubsystem.h"
#include "FILNStationControls.h"
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
}
class FFicsItLogisticsNetworkModule : public IModuleInterface {
 TArray<IConsoleObject*> Commands;
public:
 virtual void StartupModule() override {
  ALogisticsSubsystem::InstallParkingHook();
  for(const FString& Verb:TArray<FString>{TEXT("list"),TEXT("pair"),TEXT("unpair"),TEXT("dispatch"),TEXT("reset"),TEXT("rehome"),TEXT("status"),TEXT("station"),TEXT("prepareUninstall"),TEXT("enable"),TEXT("depot"),TEXT("removeStation")}) {
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
  for(auto* Command:Commands) IConsoleManager::Get().UnregisterConsoleObject(Command,false);
  Commands.Empty();
 }
};
IMPLEMENT_MODULE(FFicsItLogisticsNetworkModule,FicsItLogisticsNetwork)
