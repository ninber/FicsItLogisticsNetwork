// SPDX-License-Identifier: GPL-3.0-only
#include "FILNRecovery.h"
#include <cstdlib>
#include <iostream>
using namespace filn::recovery;
#define CHECK(x) do { ++checks; if(!(x)) { std::cerr<<__LINE__<<": "<<#x<<'\n'; return EXIT_FAILURE; } } while(false)
int main() {
 int checks=0;
 RetryClock r;
 Snapshot s{true,false,true,true,false,true,false,true};
 CHECK(Decide(s,r,0)==Action::FuelWait);
 CHECK(r.exponent==0);
 s.fuel=true;
 CHECK(Decide(s,r,0)==Action::Replan);
 r.Attempt(0); CHECK(r.next==5 && r.waiting);
 CHECK(Decide(s,r,1)==Action::RetryWait);
 CHECK(Decide(s,r,5)==Action::Replan);
 r.Attempt(5); CHECK(r.next==15);
 s.driver=true; CHECK(Decide(s,r,15)==Action::DriverWait); s.driver=false;
 s.autopilot=false; CHECK(Decide(s,r,15)==Action::Stop); s.autopilot=true;
 s.routeMatches=false; CHECK(Decide(s,r,15)==Action::Stop); s.routeMatches=true;
 s.cargoMatches=false; CHECK(Decide(s,r,15)==Action::Stop); s.cargoMatches=true;
 s.travelling=false; CHECK(Decide(s,r,15)==Action::Stop); s.travelling=true;
 s.docking=true; CHECK(Decide(s,r,15)==Action::Stop); s.docking=false;
 s.nativeFault=false; CHECK(Decide(s,r,15)==Action::Resume); s.nativeFault=true;
 for(int i=0;i<40;++i) {
  const auto now=r.next;
  CHECK(Decide(s,r,now)==Action::Replan);
  r.Attempt(now);
  CHECK(r.next>now && r.next-now<=60);
 }
 CHECK(r.exponent==4);
 r.Reset(); CHECK(r.Due(0) && !r.waiting && r.exponent==0);
 std::cout<<checks<<" recovery policy checks passed; no native integration claimed\n";
}
