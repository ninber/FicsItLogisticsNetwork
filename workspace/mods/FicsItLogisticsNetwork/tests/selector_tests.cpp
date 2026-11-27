#include "FILNSelectorPolicy.h"
#include <cstdlib>
#include <iostream>
#define CHECK(v) do { if(!(v)) { std::cerr << __LINE__ << ": " #v "\n"; return EXIT_FAILURE; } } while(false)
int main() {
 using namespace filn::selector;
 CHECK(!ReplyMatches(0,0,true));
 CHECK(!ReplyMatches(1,2,true));
 CHECK(!ReplyMatches(1,1,false));
 CHECK(ReplyMatches(2,2,true));
 CHECK(CanRefresh(false,0,20,0));
 CHECK(!CanRefresh(false,3,4.99,5));
 CHECK(CanRefresh(false,3,5,5));
 CHECK(!CanRefresh(true,0,20,0));
 CHECK(!CanRefresh(true,3,20,5));
 CHECK(!SelectionAcknowledged(false,true));
 CHECK(!SelectionAcknowledged(true,false));
 CHECK(SelectionAcknowledged(true,true));
 // Exhaustive combination of docking, leases and compatibility.
 for(int docked=0;docked<2;++docked)
  for(int reserved=0;reserved<2;++reserved)
   for(int compatible=0;compatible<2;++compatible)
    CHECK(CanReconcile(docked,reserved,compatible)==(!docked && !reserved && compatible));
 // New dispatch must not use a superseded resource, including a clear.
 int active=1, desired=2;
 CHECK(!NewDispatchAllowed(desired==active));
 desired=3;
 CHECK(!ReplyMatches(3,2,true));
 CHECK(!SelectionAcknowledged(true,2==desired));
 CHECK(!NewDispatchAllowed(desired==active));
 desired=0;
 CHECK(!NewDispatchAllowed(desired==active));
 CHECK(!CanReconcile(false,true,true));
 CHECK(!CanReconcile(false,false,false));
 CHECK(CanReconcile(false,false,true));
 active=desired;
 CHECK(NewDispatchAllowed(desired==active));
 std::cout << "Selector and deferred application policy checks passed (portable; no Unreal UI/RPC/save exercised)\n";
}
