// SPDX-License-Identifier: GPL-3.0-only
#include "FILNRecovery.h"
#include <cstdlib>
#include <iostream>
using namespace filn::recovery;
#define CHECK(x) do { ++n; if(!(x)) { std::cerr<<__LINE__<<": "<<#x<<'\n'; return EXIT_FAILURE; } } while(false)
int main(){ int n=0; Progress p;
 CHECK(!p.Sample(10,false,false)); CHECK(!p.Sample(69,false,false)); CHECK(p.Sample(70,false,false));
 CHECK(!p.Sample(71,true,false)); CHECK(!p.Sample(130,false,false)); CHECK(p.Sample(131,false,false));
 CHECK(!p.Sample(200,false,true)); CHECK(!p.Sample(259,false,false)); CHECK(p.Sample(260,false,false));
 CHECK(!p.Sample(1,false,false)); p.Reset(); CHECK(!p.Sample(1000,false,false));
 for(unsigned mask=0;mask<128;++mask) {
  bool a=mask&1,b=mask&2,c=mask&4,d=mask&8,e=mask&16,f=mask&32,g=mask&64;
  CHECK(SafeHomeCompletion(a,b,c,d,e,f,g)==(mask==119));
 }
 for(unsigned mask=0;mask<64;++mask) {
  CHECK(CanAbortToHome(mask&1,mask&2,mask&4,mask&8,mask&16,mask&32)==(mask==51));
 }
 CHECK(IdleRouteFaultAllowed(true,false)); CHECK(IdleRouteFaultAllowed(false,true)); CHECK(!IdleRouteFaultAllowed(false,false));
 CHECK(ActivationGrace(true,100,105)); CHECK(!ActivationGrace(true,105,105)); CHECK(!ActivationGrace(false,100,105)); CHECK(!ActivationGrace(true,0,0));
 CHECK(CanBridgePickupDeparture(true,false,100,100));
 CHECK(!CanBridgePickupDeparture(true,true,100,100)); CHECK(!CanBridgePickupDeparture(true,false,0,100)); CHECK(!CanBridgePickupDeparture(false,false,100,100));
 CHECK(CanFinalizeHome(true,false,true,false,true)); CHECK(CanFinalizeHome(true,false,false,true,true)); CHECK(CanFinalizeHome(false,true,true,false,true));
 CHECK(!CanFinalizeHome(false,true,false,false,true)); CHECK(!CanFinalizeHome(true,false,true,false,false)); CHECK(!CanFinalizeHome(false,false,true,false,true));
 std::cout<<n<<" lifecycle checks passed\n";
}
