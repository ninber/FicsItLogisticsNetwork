// SPDX-License-Identifier: GPL-3.0-only
#include "FILNRepairPolicy.h"
#include <cassert>
#include <iostream>
int main() {
 using namespace filn::repair;
 assert(Slot(0,0,1)==SlotAction::Keep);
 assert(Slot(0,1,1)==SlotAction::RemoveLegacyRestriction);
 assert(Slot(0,2,1)==SlotAction::Retry);
 assert(Slot(2,1,1)==SlotAction::Retry);
 assert(Slot(2,2,1)==SlotAction::Keep);
 assert(Slot(0,2,0)==SlotAction::Retry);
 assert(CargoUnchanged(0,0,false));
 assert(CargoUnchanged(1,1,true));
 assert(!CargoUnchanged(1,0,true));
 assert(!CargoUnchanged(1,1,false));
 assert(!CargoUnchanged(-1,-1,true));
 assert(CanRestoreJob(true,true,true,true));
 assert(!CanRestoreJob(true,false,true,true));
 assert(!CanRestoreJob(true,true,false,true));
 assert(!CanRestoreJob(true,true,true,false));
 std::cout<<"15 local recovery policy checks passed; native inventory and gameplay not verified\n";
}
