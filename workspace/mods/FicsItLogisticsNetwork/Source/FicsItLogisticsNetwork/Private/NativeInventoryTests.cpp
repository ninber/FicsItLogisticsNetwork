// SPDX-License-Identifier: GPL-3.0-only
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "FGInventoryComponent.h"
#include "FILNInventoryCapacity.h"
#include "FILNDockFilterTransfer.h"
#include "Resources/FGItemDescriptor.h"

// Real native inventory operations, not a mocked FG implementation. The development
// StarterProject's generated FactoryGame bodies may fail these tests; keep that failure.
static bool InventoryAcceptance(FAutomationTestBase& Test,bool Fluid) {
 UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
 if(!Test.TestNotNull(TEXT("Isolated transient world"),World)) return false;
 AActor* Owner=World->SpawnActor<AActor>();
 if(!Owner) { World->DestroyWorld(false); Test.AddError(TEXT("Could not create isolated inventory owner")); return false; }
 auto* Inventory=NewObject<UFGInventoryComponent>(Owner);
 Inventory->RegisterComponent();
 Inventory->Resize(2);
 bool Passed=Test.TestEqual(TEXT("Native Resize must allocate two slots; generated StarterProject stubs do not implement inventory behavior"),Inventory->GetSizeLinear(),2);
 if(Passed) {
  const TCHAR* Path=Fluid?TEXT("/Game/FactoryGame/Resource/RawResources/Water/Desc_Water.Desc_Water_C"):TEXT("/Game/FactoryGame/Resource/Parts/IronPlate/Desc_IronPlate.Desc_IronPlate_C");
  auto Resource=LoadClass<UFGItemDescriptor>(nullptr,Path);
  auto Other=LoadClass<UFGItemDescriptor>(nullptr,TEXT("/Game/FactoryGame/Resource/Parts/CopperSheet/Desc_CopperSheet.Desc_CopperSheet_C"));
  Passed=Test.TestNotNull(TEXT("Native resource descriptor"),Resource) && Test.TestNotNull(TEXT("Other resource descriptor"),Other);
  if(Passed) {
   TArray<TSubclassOf<UFGItemDescriptor>> Original;
   for(int32 Slot=0;Slot<2;++Slot) Original.Add(Inventory->GetAllowedItemOnIndex(Slot));
   int64 EmptyCapacity=0;
   const bool EmptyRead=filn::inventory::TryCapacity(Inventory,Resource,EmptyCapacity);
   Passed&=Test.TestTrue(TEXT("Real native capacity on unmodified empty slots"),EmptyRead && EmptyCapacity>0);
   if(EmptyRead && EmptyCapacity>0) {
    Passed&=Test.TestEqual(TEXT("Add one real item"),Inventory->AddStack(FInventoryStack(1,Resource),true),1);
    int64 OccupiedCapacity=0;
    Passed&=Test.TestTrue(TEXT("Occupied and empty slots retain full native capacity"),filn::inventory::TryCapacity(Inventory,Resource,OccupiedCapacity) && OccupiedCapacity==EmptyCapacity);
    if(!Fluid && !Original[0]) {
     // Exercise the actual native setter on an occupied slot; no simulated cargo.
     const int32 Slot=Inventory->FindFirstIndexWithItemType(Resource);
     if(Test.TestTrue(TEXT("Real occupied slot exists"),Slot>=0 && !Inventory->GetAllowedItemOnIndex(Slot))) {
      FInventoryStack Before,After;
      Passed&=Test.TestTrue(TEXT("Read real cargo before legacy restriction"),Inventory->GetStackFromIndex(Slot,Before));
      Inventory->SetAllowedItemOnIndex(Slot,Resource);
      Passed&=Test.TestTrue(TEXT("Legacy restriction applied"),Inventory->GetAllowedItemOnIndex(Slot)==Resource);
      Inventory->SetAllowedItemOnIndex(Slot,nullptr);
      Passed&=Test.TestTrue(TEXT("Legacy restriction cleared"),!Inventory->GetAllowedItemOnIndex(Slot));
      Passed&=Test.TestTrue(TEXT("Read real cargo after repair"),Inventory->GetStackFromIndex(Slot,After));
      Passed&=Test.TestTrue(TEXT("Same real cargo in slot after repair"),Before.NumItems==After.NumItems && Before.Item.GetItemClass()==After.Item.GetItemClass());
      Passed&=Test.TestEqual(TEXT("Total real cargo unchanged"),Inventory->GetNumItems(Resource),1);
     } else Passed=false;
    }
    Inventory->Remove(Resource,1);
   }
   if(!Fluid && Passed) {
    auto* Source=NewObject<UFGInventoryComponent>(Owner);
    Source->RegisterComponent(); Source->Resize(2);
    Passed&=Test.TestEqual(TEXT("Native mixed source slots"),Source->GetSizeLinear(),2);
    Passed&=Test.TestEqual(TEXT("Insert matching cargo"),Source->AddStack(FInventoryStack(3,Resource),true),3);
    Passed&=Test.TestEqual(TEXT("Insert unrelated cargo"),Source->AddStack(FInventoryStack(7,Other),true),7);
    const auto Transfer=filn::dockfilter::SelectiveSolidUnload(Source,Inventory,Resource);
    Passed&=Test.TestTrue(TEXT("Actual filtered native transfer verified"),Transfer.Verified);
    Passed&=Test.TestEqual(TEXT("Actual selected quantity transferred"),Transfer.Amount,3);
    Passed&=Test.TestEqual(TEXT("Selected cargo removed from source"),Source->GetNumItems(Resource),0);
    Passed&=Test.TestEqual(TEXT("Unrelated cargo stays in source"),Source->GetNumItems(Other),7);
    Passed&=Test.TestEqual(TEXT("Selected cargo reaches target"),Inventory->GetNumItems(Resource),3);
    Passed&=Test.TestEqual(TEXT("Wrong cargo not added to target"),Inventory->GetNumItems(Other),0);
    Passed&=Test.TestEqual(TEXT("Source slot count unchanged"),Source->GetSizeLinear(),2);
   }
   Passed&=Test.TestEqual(TEXT("Target slot count unchanged"),Inventory->GetSizeLinear(),2);
   for(int32 Slot=0;Slot<2;++Slot)
    Passed&=Test.TestTrue(TEXT("Native slot descriptor unchanged"),Inventory->GetAllowedItemOnIndex(Slot)==Original[Slot]);
  }
 }
 World->DestroyWorld(false); return Passed;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFILNSolidInventory,"FILN.NativeInventory.Solid",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FFILNSolidInventory::RunTest(const FString&) { return InventoryAcceptance(*this,false); }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFILNFluidInventory,"FILN.NativeInventory.Fluid",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FFILNFluidInventory::RunTest(const FString&) { return InventoryAcceptance(*this,true); }
#endif
