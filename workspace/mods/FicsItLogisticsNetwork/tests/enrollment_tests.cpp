// SPDX-License-Identifier: GPL-3.0-only
#include "FILNEnrollment.h"
#include <cstdlib>
#include <iostream>
using namespace filn::enrollment;
#define CHECK(x) do { ++checks; if(!(x)) { std::cerr<<__LINE__<<": "<<#x<<'\n'; return EXIT_FAILURE; } } while(false)
int main() {
 int checks=0;
 Snapshot good{true,true,true,true,false,true,true,true,false,Route::Empty};
 CHECK(Evaluate(good)==Decision::AssignHome);
 for(auto route:{Route::Empty,Route::HomeOnly,Route::Other}) for(bool ap:{false,true}) {
  auto s=good; s.route=route; s.autopilot=ap;
  auto expected=route==Route::Empty&&!ap?Decision::AssignHome:route==Route::HomeOnly&&ap?Decision::AdoptHome:Decision::Reject;
  CHECK(Evaluate(s)==expected);
  for(auto field:{&Snapshot::authority,&Snapshot::homeEligible,&Snapshot::uniqueHome,&Snapshot::uniqueTruck,&Snapshot::stopped,&Snapshot::emptyCargo,&Snapshot::nearby}) {
   auto bad=s; bad.*field=false; CHECK(Evaluate(bad)==Decision::Reject);
  }
  s.driver=true; CHECK(Evaluate(s)==Decision::Reject);
 }
 for(bool cargo:{false,true}) for(bool fuel:{false,true}) for(bool port:{false,true})
  CHECK(ConnectedPortAllowed(cargo,fuel,port)==(!cargo&&(fuel||port)));
 std::cout<<checks<<" enrollment policy checks passed; no native integration claimed\n";
}
