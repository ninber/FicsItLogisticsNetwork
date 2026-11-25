// SPDX-License-Identifier: GPL-3.0-only
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "FGInventoryComponent.h"
#include "FILNInventoryCapacity.h"
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
   for(int32 Slot=0;Slot<2;++Slot) Inventory->SetAllowedItemOnIndex(Slot,Resource);
   Passed&=Test.TestTrue(TEXT("Selected resource allowed"),Inventory->IsItemAllowed(Resource,0));
   int64 EmptyCapacity=0;
   const bool EmptyRead=filn::inventory::TryCapacity(Inventory,Resource,EmptyCapacity);
   Passed&=Test.TestTrue(TEXT("Real native capacity on empty selected slots"),EmptyRead && EmptyCapacity>0);
   if(EmptyRead && EmptyCapacity>0) {
    Passed&=Test.TestEqual(TEXT("Add one selected item"),Inventory->AddStack(FInventoryStack(1,Resource),true),1);
    int64 OccupiedCapacity=0;
    Passed&=Test.TestTrue(TEXT("Occupied and empty slots retain full native capacity"),filn::inventory::TryCapacity(Inventory,Resource,OccupiedCapacity) && OccupiedCapacity==EmptyCapacity);
    Inventory->Remove(Resource,1);
   }
   Passed&=Test.TestFalse(TEXT("Wrong resource rejected"),Inventory->IsItemAllowed(Other,0));
   Passed&=Test.TestEqual(TEXT("Wrong cargo cannot enter"),Inventory->AddStack(FInventoryStack(1,Other),true),0);
   Passed&=Test.TestTrue(TEXT("No cargo deletion/contamination"),Inventory->IsEmpty());
   for(int32 Slot=0;Slot<2;++Slot) Inventory->SetAllowedItemOnIndex(Slot,nullptr);
   Passed&=Test.TestTrue(TEXT("Restored vanilla filter accepts other descriptor"),Inventory->IsItemAllowed(Other,0));
  }
 }
 World->DestroyWorld(false); return Passed;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFILNSolidInventory,"FILN.NativeInventory.Solid",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FFILNSolidInventory::RunTest(const FString&) { return InventoryAcceptance(*this,false); }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFILNFluidInventory,"FILN.NativeInventory.Fluid",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FFILNFluidInventory::RunTest(const FString&) { return InventoryAcceptance(*this,true); }
#endif
