#pragma once
#include <cstdint>

namespace filn::telemetry {
// The dashboard lease controls whether to START another scan, not whether
// an accepted scan may finish. A changed request survives a pause or busy scan.
struct Demand {
 std::int64_t LastStamp=0;
 bool Observed=false, Pending=false, Fresh=false;
 void Observe(std::int64_t Stamp,bool IsFresh) {
  if(Stamp>0 && ((!Observed && IsFresh) || (Observed && Stamp!=LastStamp))) Pending=true;
  LastStamp=Stamp;
  Observed=true;
  Fresh=Stamp>0 && IsFresh;
 }
 bool ShouldStart(bool Published) const { return !Published || Pending || Fresh; }
 void Started() { Pending=false; }
 void Interrupted(bool WasCollecting) { Pending=Pending || WasCollecting; }
 void Failed() { Pending=true; }
};
}
