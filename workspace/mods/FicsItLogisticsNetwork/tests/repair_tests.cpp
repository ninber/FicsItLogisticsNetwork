// SPDX-License-Identifier: GPL-3.0-only
#include "FILNRepairPolicy.h"
#include <cassert>
#include <iostream>
int main() {
 using namespace filn::repair;
 assert(SlotMatches(0,0));
 assert(SlotMatches(1,1));
 assert(!SlotMatches(0,1));
 assert(!SlotMatches(1,0));
 assert(!SlotMatches(1,2));
 assert(CanRestoreJob(true,true,true,true));
 assert(!CanRestoreJob(true,false,true,true));
 assert(!CanRestoreJob(true,true,false,true));
 assert(!CanRestoreJob(true,true,true,false));
 std::cout<<"9 local validation/recovery policy checks passed; native inventory and gameplay not verified\n";
}
