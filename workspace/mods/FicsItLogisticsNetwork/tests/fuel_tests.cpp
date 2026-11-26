// SPDX-License-Identifier: GPL-3.0-only
#include "FILNFuelPolicy.h"
#include <cstdlib>
#include <iostream>
using namespace filn::fuel;
#define CHECK(x) do { ++n; if(!(x)) { std::cerr<<__LINE__<<": "<<#x<<'\n'; return EXIT_FAILURE; } } while(false)
int main(){
 int n=0; Snapshot s{true,true,true,false,false,5,10};
 CHECK(Decide(s)==Action::RequestDock); s.pending=true; CHECK(Decide(s)==Action::Observe);
 s.pending=false; s.cooldown=true; CHECK(Decide(s)==Action::Wait);
 s.cooldown=false; s.supplied=false; CHECK(Decide(s)==Action::Wait);
 s.quantity=10; CHECK(Decide(s)==Action::Ready);
 s.sameFuel=false; CHECK(Decide(s)==Action::Ready);
 s.quantity=9; CHECK(Decide(s)==Action::Reject);
 s.sameFuel=true; s.quantity=11; CHECK(Decide(s)==Action::Reject);
 s.quantity=-1; CHECK(Decide(s)==Action::Reject);
 CHECK(Observe(true,true,5,6,1)==Receipt::Received);
 CHECK(Observe(true,false,5,6,1)==Receipt::Cancelled);
 CHECK(Observe(false,true,5,6,1)==Receipt::Cancelled);
 CHECK(Observe(true,true,5,5,44)==Receipt::Pending);
 CHECK(Observe(true,true,5,5,45)==Receipt::TimedOut);
 CHECK(Observe(true,true,5,4,45)==Receipt::TimedOut);
 std::cout<<n<<" fuel policy checks passed\n";
}
