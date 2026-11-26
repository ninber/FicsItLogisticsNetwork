#include "LogisticsSubsystem.h"
#include "FGColorInterface.h"
#include "Resources/FGItemDescriptor.h"
#include "FGCustomizationRecipe.h"
#include "FGRecipeManager.h"
#include "FGGameState.h"
#include "FGBuildableSubsystem.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "WheeledVehicles/FGWheeledVehicleIdentifier.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "Buildables/FGBuildableDockingStation.h"
#include "UObject/UnrealType.h"
#include "Engine/World.h"

namespace {
using FILNSwatch=TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch>;
// Strict unsigned decimal slot parser: no dependency on an engine-version
// specific LexFromString header, no overflow, sign or partial parse.
bool FILNParseSwatchSlot(const FString& Text,int32& Slot) {
 if(Text.IsEmpty() || Text.Len()>3) return false;
 int32 Value=0;
 for(const TCHAR Ch:Text) {
  if(Ch<TEXT('0') || Ch>TEXT('9')) return false;
  Value=Value*10+(Ch-TEXT('0'));
 }
 if(Value>=255) return false;
 Slot=Value; return true;
}
int32 FILNPaletteSize(UObject* Object,const TCHAR* Property) {
 if(!IsValid(Object)) return 0;
 const auto* Array=FindFProperty<FArrayProperty>(Object->GetClass(),Property);
 if(!Array) return 0;
 const auto* Inner=CastField<FStructProperty>(Array->Inner);
 if(!Inner || Inner->Struct!=FFactoryCustomizationColorSlot::StaticStruct()) return 0;
 return FScriptArrayHelper(Array,Array->ContainerPtrToValuePtr<void>(Object)).Num();
}
TArray<FILNSwatch> FILNRegularSwatches(UWorld* World) {
 TArray<FILNSwatch> Result;
 auto* Recipes=AFGRecipeManager::Get(World); if(!Recipes) return Result;
 auto* BS=AFGBuildableSubsystem::Get(World);
 auto* GS=World?World->GetGameState<AFGGameState>():nullptr;
 const int32 Count=FMath::Min(FILNPaletteSize(BS,TEXT("mColorSlots_Data")),FILNPaletteSize(GS,TEXT("mBuildingColorSlots_Data")));
 TArray<TSubclassOf<UFGCustomizationRecipe>> Available; Recipes->GetAllAvailableCustomizationRecipes(Available);
 for(const auto& Recipe:Available) {
  const auto Descriptor=UFGCustomizationRecipe::GetCustomizationDescriptor(Recipe);
  if(!Descriptor || !Descriptor->IsChildOf(UFGFactoryCustomizationDescriptor_Swatch::StaticClass()) ||
     Descriptor->IsChildOf(UFGFactoryCustomizationDescriptor_PaintFinish::StaticClass())) continue;
  const FILNSwatch Swatch(Descriptor.Get());
  const auto* Default=Swatch.GetDefaultObject();
  if(Default && Default->ID>=0 && Default->ID<255 && Default->ID<Count) Result.AddUnique(Swatch);
 }
 Result.Sort([](const FILNSwatch& A,const FILNSwatch& B){return A.GetDefaultObject()->ID<B.GetDefaultObject()->ID;});
 return Result;
}
AActor* FILNPaintTarget(AActor* ID) {
 if(auto* Station=Cast<AFGDockingStationIdentifier>(ID)) return Station->GetStation();
 if(auto* Vehicle=Cast<AFGWheeledVehicleIdentifier>(ID)) return Vehicle->GetOwnerVehicle();
 return nullptr;
}
}
void ALogisticsSubsystem::UpdateNetworkPaint(AActor* ID,AActor* Target,bool Member) {
 if(!HasAuthority() || !IsValid(ID) || ID->GetWorld()!=GetWorld()) return;
 if(!NetworkSwatch && NetworkPaint.IsEmpty()) return;
 const int32 Index=NetworkPaint.IndexOfByPredicate([ID](const FFILNNetworkPaint& P){return P.Identifier==ID;});
 if(!IsValid(Target) || !Target->HasAuthority()) return;
 auto* Color=Cast<IFGColorInterface>(Target);
 if(!Color || !Color->GetCanBeColored_Native()) return;
 auto Current=Color->GetCustomizationData_Native();
 if(!Member || !NetworkSwatch) {
  if(Index==INDEX_NONE) return;
  const auto& Previous=NetworkPaint[Index];
  // Never undo a player's later explicit repaint.
  if(Current.SwatchDesc!=Previous.Applied) { NetworkPaint.RemoveAtSwap(Index); return; }
  Current.SwatchDesc=Previous.Original; Current.OverrideColorData=Previous.OriginalOverride;
  Color->SetCustomizationData_Native(Current,true);
  const auto& Actual=Color->GetCustomizationData_Native();
  if(Actual.SwatchDesc==Current.SwatchDesc && Actual.OverrideColorData==Current.OverrideColorData)
   NetworkPaint.RemoveAtSwap(Index);
  return;
 }
 if(Index!=INDEX_NONE && NetworkPaint[Index].Applied==NetworkSwatch) return;
 // Preserve an explicit individual repaint after initial enrollment, unless
 // the player deliberately selects another network swatch.
 if(Index==INDEX_NONE && Current.SwatchDesc==NetworkSwatch) return;
 FFILNNetworkPaint Previous;
 if(Index!=INDEX_NONE && Current.SwatchDesc==NetworkPaint[Index].Applied) Previous=NetworkPaint[Index];
 else { Previous.Identifier=ID; Previous.Original=Current.SwatchDesc; Previous.OriginalOverride=Current.OverrideColorData; }
 Previous.Applied=NetworkSwatch;
 Current.SwatchDesc=NetworkSwatch;
 // Keep the complete current pattern, skin, material and rotation.
 Color->SetCustomizationData_Native(Current,true);
 if(Color->GetCustomizationData_Native().SwatchDesc==NetworkSwatch) {
  if(Index==INDEX_NONE) NetworkPaint.Add(Previous); else NetworkPaint[Index]=Previous;
  UE_LOG(LogFILN,Log,TEXT("FILN_SWATCH_APPLIED actor=%s slot=%d"),*ID->GetPathName(),NetworkSwatch.GetDefaultObject()->ID);
 }
}
bool ALogisticsSubsystem::RestoreAllNetworkPaint() {
 for(int32 I=NetworkPaint.Num()-1;I>=0;--I) {
  auto* ID=NetworkPaint[I].Identifier.Get();
  if(!IsValid(ID)) { NetworkPaint.RemoveAtSwap(I); continue; }
  UpdateNetworkPaint(ID,FILNPaintTarget(ID),false);
 }
 return NetworkPaint.IsEmpty();
}
void ALogisticsSubsystem::ExecuteNetworkSwatch(const TArray<FString>& Args) {
 if(!HasAuthority() || bPreparingUninstall || bUninstallPrepared) return;
 if(Args.Num()==1 && Args[0]==TEXT("off")) {
  NetworkSwatch=nullptr;
  const bool Restored=RestoreAllNetworkPaint();
  UE_LOG(LogFILN,Log,TEXT("FILN swatch disabled; original paint restored=%d; unavailable actors retry on discovery"),Restored); return;
 }
 const auto Choices=FILNRegularSwatches(GetWorld());
 if(Args.IsEmpty() || (Args.Num()==1 && Args[0]==TEXT("list"))) {
  UE_LOG(LogFILN,Log,TEXT("FILN swatch: choose an existing UNUSED regular swatch with filn.swatch <slot> [taxi]. Existing non-FILN objects sharing the slot also change when edited. No new slot is created."));
  for(const auto& Swatch:Choices) UE_LOG(LogFILN,Log,TEXT("slot=%d name=%s class=%s"),Swatch.GetDefaultObject()->ID,*UFGItemDescriptor::GetItemName(Swatch).ToString(),*Swatch->GetPathName());
  return;
 }
 int32 Slot=-1;
 if(Args.Num()>2 || !FILNParseSwatchSlot(Args[0],Slot) || Slot<0 || Slot>=255 || (Args.Num()==2 && Args[1]!=TEXT("taxi"))) return;
 FILNSwatch Chosen;
 for(const auto& Swatch:Choices) if(Swatch.GetDefaultObject()->ID==Slot) {
  if(Chosen && Chosen!=Swatch) { UE_LOG(LogFILN,Warning,TEXT("Ambiguous native slot; no change")); return; }
  Chosen=Swatch;
 }
 if(!Chosen) { UE_LOG(LogFILN,Warning,TEXT("Unavailable or invalid regular native swatch; no change")); return; }
 if(Args.Num()==2) {
  auto* GS=GetWorld()->GetGameState<AFGGameState>(); auto* BS=AFGBuildableSubsystem::Get(GetWorld());
  if(!GS || !BS) return;
  auto Data=GS->GetBuildingColorDataForSlot(static_cast<uint8>(Slot));
  Data.PrimaryColor=FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("FFC107")));
  Data.SecondaryColor=FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("202124")));
  // This is an explicit player-selected existing shared slot. Native edits
  // propagate to all objects referencing it, including any non-FILN objects.
  GS->Server_SetBuildingColorDataForSlot(static_cast<uint8>(Slot),Data);
  if(GS->GetBuildingColorDataForSlot(static_cast<uint8>(Slot))!=Data ||
     BS->GetColorSlot_Data(static_cast<uint8>(Slot))!=Data) {
   UE_LOG(LogFILN,Warning,TEXT("Native palette edit readback not confirmed; inspect Customizer. Binding withheld.")); return;
  }
 }
 NetworkSwatch=Chosen;
 UE_LOG(LogFILN,Log,TEXT("FILN shared existing swatch selected slot=%d; bounded discovery applies it; edit this slot in native Customizer"),Slot);
}
