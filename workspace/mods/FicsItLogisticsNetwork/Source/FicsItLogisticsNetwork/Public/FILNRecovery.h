// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>
#include <cmath>
namespace filn::recovery {
// Port of RecoveryFSM.lua's bounded-frequency retry clock. Native adapter
// deliberately only retries the currently reserved, unchanged journey.
struct RetryClock {
 std::uint8_t exponent=0;
 double next=0;
 bool waiting=false;
 bool Due(double now) const { return std::isfinite(now) && now>=next; }
 void Attempt(double now) {
  const unsigned delay=exponent>=4?60u:5u*(1u<<exponent);
  next=now+delay;
  if(exponent<4) ++exponent;
  waiting=true;
 }
 void Reset() { *this=RetryClock{}; }
};
enum class Action { Stop, DriverWait, FuelWait, RetryWait, Replan, Resume };
struct Snapshot {
 bool autopilot=false, driver=false, routeMatches=false, travelling=false;
 bool docking=false, cargoMatches=false, fuel=false, nativeFault=false;
};
inline Action Decide(const Snapshot& s,const RetryClock& r,double now) {
 if(!s.autopilot || !s.routeMatches || !s.travelling || !s.cargoMatches) return Action::Stop;
 if(s.driver) return Action::DriverWait;
 if(s.docking) return Action::Stop;
 if(!s.fuel) return Action::FuelWait;
 if(!s.nativeFault) return Action::Resume;
 return r.Due(now)?Action::Replan:Action::RetryWait;
}
}
