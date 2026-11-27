#define near // Win32 headers define this legacy keyword macro.
#include "FILNIdlePolicy.h"
#undef near
#include <cstdlib>
#include <iostream>
using namespace filn::idle;
#define CHECK(x) do { ++n; if(!(x)) { std::cerr<<__LINE__<<": "<<#x<<'\n'; return EXIT_FAILURE; } } while(false)
int main(){
 int n=0; Snapshot s{true,true,true,false,true,true,true,true,false,true,false,true};
 CHECK(Evaluate(s)==Action::Hold);
 s.docked=false; CHECK(Evaluate(s)==Action::NativeReturn);
 s.held=true; CHECK(Evaluate(s)==Action::Hold);
 s.homeNearby=false; CHECK(Evaluate(s)==Action::NativeReturn);
 s.fault=true; CHECK(Evaluate(s)==Action::RetryEmptyReturn);
 s.empty=false; CHECK(Evaluate(s)==Action::NativeReturn);
 s.empty=true; s.fuel=false; CHECK(Evaluate(s)==Action::NativeReturn);
 s.fuel=true;
 for(auto field:{&Snapshot::owned,&Snapshot::idle,&Snapshot::autopilot,&Snapshot::homeValid}) {
  auto bad=s; bad.*field=false; CHECK(Evaluate(bad)==Action::Release);
 }
 s.driver=true; CHECK(Evaluate(s)==Action::Release);
 CHECK(MayRequestHomeDock(true,true,true,false,true,true,false,false,false));
 CHECK(!MayRequestHomeDock(false,true,true,false,true,true,false,false,false));
 CHECK(!MayRequestHomeDock(true,false,true,false,true,true,false,false,false));
 CHECK(!MayRequestHomeDock(true,true,false,false,true,true,false,false,false));
 CHECK(!MayRequestHomeDock(true,true,true,true,true,true,false,false,false));
 CHECK(!MayRequestHomeDock(true,true,true,false,false,true,false,false,false));
 CHECK(!MayRequestHomeDock(true,true,true,false,true,false,false,false,false));
 CHECK(!MayRequestHomeDock(true,true,true,false,true,true,true,false,false));
 CHECK(!MayRequestHomeDock(true,true,true,false,true,true,false,true,false));
 CHECK(!MayRequestHomeDock(true,true,true,false,true,true,false,false,true));
 std::cout<<n<<" idle recovery checks passed\n";
}
