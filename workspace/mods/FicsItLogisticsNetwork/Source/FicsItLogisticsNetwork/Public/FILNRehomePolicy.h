#pragma once
namespace filn::rehome {
struct Snapshot {
 bool authority=false, knownTruck=false, idleOrRecovery=false, homeEligible=false;
 bool exclusive=false, autopilot=false, driver=false, docked=false, stopped=false;
 bool leaseOwned=false, cargoReadable=false;
};
inline bool Allowed(const Snapshot& s) {
 return s.authority&&s.knownTruck&&s.idleOrRecovery&&s.homeEligible&&s.exclusive&&
        !s.autopilot&&!s.driver&&s.docked&&s.stopped&&s.leaseOwned&&s.cargoReadable;
}
}
