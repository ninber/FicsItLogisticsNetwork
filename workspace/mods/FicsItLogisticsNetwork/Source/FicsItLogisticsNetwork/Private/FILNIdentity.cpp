#include "LogisticsSubsystem.h"
#include "FILNIdentityPolicy.h"
#include "WheeledVehicles/FGVehicleSubsystem.h"
#include "WheeledVehicles/FGWheeledVehicleIdentifier.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "Resources/FGItemDescriptor.h"
#include "UObject/UnrealType.h"
#include "Engine/World.h"

namespace {
// Compare actual factory display text, including localization, instead of
// accepting arbitrary prefixes such as "Truck", "Depot" or "Station".
bool FILNFactoryName(const FText& Current,const UObject* Object,const FText& IdentifierDefault) {
 if(Current.IsEmpty()) return true;
 if(!IdentifierDefault.IsEmpty() && Current.ToString()==IdentifierDefault.ToString()) return true;
 if(!IsValid(Object)) return false;
 for(auto* Class=Object->GetClass();Class;Class=Class->GetSuperClass()) {
  const auto* Default=Class->GetDefaultObject(false);
  const auto* Prop=FindFProperty<FTextProperty>(Class,TEXT("mDisplayName"));
  if(!Default || !Prop) continue;
  const auto& Name=*Prop->ContainerPtrToValuePtr<FText>(Default);
  if(!Name.IsEmpty() && Current.ToString()==Name.ToString()) return true;
 }
 return false;
}
FString FILNVehiclePrefix(const AFGWheeledVehicleIdentifier* V) {
 const auto Class=V->GetVehicleClass();
 const FString Name=Class?Class->GetName():FString();
 if(Name.Contains(TEXT("Tractor"),ESearchCase::IgnoreCase)) return TEXT("Tractor");
 if(Name.Contains(TEXT("Truck"),ESearchCase::IgnoreCase)) return TEXT("Truck");
 if(Name.Contains(TEXT("Explorer"),ESearchCase::IgnoreCase)) return TEXT("Explorer");
 return TEXT("Vehicle");
}
}
bool ALogisticsSubsystem::IsNetworkStation(AFGDockingStationIdentifier* ID) const {
 return IsValid(ID) && filn::identity::ProviderMember(
  StationConfigs.ContainsByPredicate([ID](const FFILNStationConfig& C){return C.Station==ID && C.Resource && !C.ExternalFilter;}),
  KnownNetworkProviders.Contains(ID) && IsValid(ID->GetStation()) && ID->GetStation()->GetIsInLoadMode(),IsDepot(ID));
}
void ALogisticsSubsystem::PruneNetworkIdentity() {
 NamedNetworkStations.RemoveAll([](const auto& ID){return !IsValid(ID);});
 NamedNetworkVehicles.RemoveAll([](const auto& V){return !IsValid(V);});
 KnownNetworkProviders.RemoveAll([](const auto& ID){return !IsValid(ID);});
 NetworkPaint.RemoveAll([](const FFILNNetworkPaint& P){return !IsValid(P.Identifier);});
}
FString ALogisticsSubsystem::AllocateNetworkName(const FString& Prefix) {
 auto* VS=AFGVehicleSubsystem::Get(GetWorld()); if(!VS) return FString();
 TSet<FString> Used;
 for(auto* ID:VS->GetDockingStations()) if(IsValid(ID)) Used.Add(ID->GetStationName().ToString().ToLower());
 for(auto* V:VS->GetAllVehicles()) if(IsValid(V)) Used.Add(V->GetVehicleName().ToString().ToLower());
 int32& Next=NetworkNameCounters.FindOrAdd(Prefix);
 // Monotonic per save, including after deletion; never overwrite a name in use.
 for(int32 Attempts=0;Attempts<=Used.Num() && Next<MAX_int32;++Attempts) {
  Next=FMath::Max(0,Next)+1;
  const FString Candidate=FString::Printf(TEXT("%s %02d"),*Prefix,Next);
  if(!Used.Contains(Candidate.ToLower())) return Candidate;
 }
 return FString();
}
void ALogisticsSubsystem::NameNetworkVehicle(AFGWheeledVehicleIdentifier* V) {
 if(!HasAuthority() || !IsValid(V) || V->GetWorld()!=GetWorld() || NamedNetworkVehicles.Contains(V)) return;
 auto* O=V->GetOwnerVehicle();
 if(!IsValid(O)) return;
 const bool Member=ByVehicle.Contains(V) || PendingDepotMembership.Contains(V);
 if(!Member) return;
 const FText Before=V->GetVehicleName();
 const auto* Default=GetDefault<AFGWheeledVehicleIdentifier>();
 const bool DefaultName=FILNFactoryName(Before,O,Default?Default->GetVehicleName():FText());
 if(!filn::identity::ShouldName(Member,false,DefaultName)) { NamedNetworkVehicles.AddUnique(V); return; }
 const FString Name=AllocateNetworkName(FILNVehiclePrefix(V));
 if(Name.IsEmpty() || V->GetVehicleName().ToString()!=Before.ToString()) return;
 // Generated names contain only our fixed prefix and digits; native internal
 // setter avoids asynchronous player-text filtering for these system names.
 V->Internal_SetVehicleName(FText::FromString(Name));
 if(V->GetVehicleName().ToString()==Name) {
  NamedNetworkVehicles.AddUnique(V); UE_LOG(LogFILN,Log,TEXT("FILN_AUTO_NAME vehicle=%s name=%s"),*V->GetPathName(),*Name);
 }
}
void ALogisticsSubsystem::NameNetworkStation(AFGDockingStationIdentifier* ID) {
 if(!HasAuthority() || !IsValid(ID) || ID->GetWorld()!=GetWorld() || NamedNetworkStations.Contains(ID) || !IsNetworkStation(ID)) return;
 auto* B=ID->GetStation(); if(!IsValid(B)) return;
 const FText Before=ID->GetStationName();
 const auto* Default=GetDefault<AFGDockingStationIdentifier>();
 const bool DefaultName=FILNFactoryName(Before,B,Default?Default->GetStationName():FText());
 if(!filn::identity::ShouldName(true,false,DefaultName)) { NamedNetworkStations.AddUnique(ID); return; }
 const auto Form=B->GetDockingStationResourceForm();
 const bool Fluid=Form==EResourceForm::RF_LIQUID || Form==EResourceForm::RF_GAS;
 const FString Prefix=IsDepot(ID)?(Fluid?TEXT("Fluid Depot"):TEXT("Depot")):(Fluid?TEXT("Fluid Station"):TEXT("Station"));
 const FString Name=AllocateNetworkName(Prefix);
 if(Name.IsEmpty() || ID->GetStationName().ToString()!=Before.ToString()) return;
 ID->Internal_SetStationName(FText::FromString(Name));
 if(ID->GetStationName().ToString()==Name) {
  NamedNetworkStations.AddUnique(ID); UE_LOG(LogFILN,Log,TEXT("FILN_AUTO_NAME station=%s name=%s"),*ID->GetPathName(),*Name);
 }
}
