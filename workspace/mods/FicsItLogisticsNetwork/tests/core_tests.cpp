#include "FILNCore.h"
#include <cstdlib>
#include <iostream>
#include <string>
#include <chrono>
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } ++checks; } while (0)
int main() {
 int checks=0;
 filn::Ownership owners;
 CHECK(owners.Pair("truck", "bay"));
 CHECK(owners.Pair("truck", "bay"));
 CHECK(!owners.Pair("other", "bay"));
 CHECK(!owners.Pair("truck", "other"));
 CHECK(!owners.Pair("", "unused-bay"));
 CHECK(!owners.Pair("unused-truck", ""));
 filn::Reservations r;
 CHECK(r.Acquire("j1", "t1", "p", "q"));
 CHECK(!r.Acquire("j2", "t2", "p", "q2"));
 CHECK(!r.Acquire("j3", "t1", "p3", "q3"));
 CHECK(!r.Acquire("j4", "t4", "p4", "q"));
 CHECK(!r.Acquire("cross-role", "other-truck", "q", "new-requester"));
 CHECK(r.Acquire("j5", "t2", "p2", "q2")); // failed acquire did not partially book t2/q2
 CHECK(!r.Acquire("j6", "t6", "same", "same"));
 CHECK(!r.Acquire("j1", "unused", "unused-p", "unused-q"));
 CHECK(!r.Acquire("", "t6", "p6", "q6"));
 CHECK(r.Acquire("j8", "t8", "unused-p", "unused-q"));
 r.Release("unknown");
 CHECK(r.Size()==3);
 r.Release("j1");
 CHECK(r.Acquire("j7", "t1", "p", "q"));
 filn::Reservations retained;
 CHECK(retained.AcquireDelivery("cargo","truck","requester"));
 CHECK(retained.StationBusy("requester"));
 CHECK(!retained.Acquire("pickup","other","provider","requester"));
 CHECK(!retained.AcquireDelivery("other","truck","other-requester"));
 retained.Release("cargo");
 CHECK(!retained.StationBusy("requester"));
 CHECK(retained.Acquire("pickup","other","provider","requester"));
 using S=filn::State; using E=filn::Event;
 CHECK(filn::Advance(S::ToProvider,E::RequesterDock)==S::Recovery);
 CHECK(filn::Advance(S::ToProvider,E::ProviderDock)==S::Loading);
 CHECK(filn::Advance(S::Loading,E::ProviderLeave)==S::ToRequester);
 CHECK(filn::Advance(S::ToRequester,E::RequesterDock)==S::Unloading);
 CHECK(filn::Advance(S::Unloading,E::RequesterLeave)==S::Returning);
 CHECK(filn::Advance(S::Returning,E::HomeStopped)==S::Completed);
 CHECK(filn::Advance(S::ToProvider,E::HomeStopped)==S::Recovery);
 CHECK(filn::AfterReload(S::Idle)==S::Idle);
 CHECK(filn::AfterReload(S::Loading)==S::Recovery);
 CHECK(filn::AfterReload(S::Completed)==S::Recovery);
 CHECK(filn::AfterReload(static_cast<S>(255))==S::Recovery);
 for(S s:{S::Reserved,S::ToProvider,S::Loading,S::ToRequester,S::Unloading,S::Returning,S::Recovery}) CHECK(filn::AfterReload(s)==S::Recovery);
 for(int n: {100,500,1000}) {
   filn::Reservations fleet;
   for(int i=0;i<n;++i) { const auto s=std::to_string(i); CHECK(fleet.Acquire("j"+s,"t"+s,"p"+s,"q"+s)); }
   CHECK(fleet.Size()==static_cast<std::size_t>(n));
   for(int i=0;i<n;++i) fleet.Release("j"+std::to_string(i));
   CHECK(fleet.Size()==0);
 }
 // 300 station slots, exclusive endpoints: at most 150 simultaneous jobs.
 for(int n:{100,500,1000}) {
  filn::Reservations fleet;
  const auto start=std::chrono::steady_clock::now();
  for(int i=0;i<n;++i) {
   const auto s=std::to_string(i), slot=std::to_string(i%150);
   CHECK(fleet.Acquire("job"+s,"truck"+s,"p"+slot,"q"+slot)==(i<150));
  }
  const auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
  CHECK(fleet.Size()==static_cast<std::size_t>(n<150?n:150));
  std::cout << "reservation exercise: vehicles=" << n << " station slots=300 attempts_us=" << us << " active=" << fleet.Size() << '\n';
 }
 std::cout << checks << " checks passed (portable primitives only)\n";
}
