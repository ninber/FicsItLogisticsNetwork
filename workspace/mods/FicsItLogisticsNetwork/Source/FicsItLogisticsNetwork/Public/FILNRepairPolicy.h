#pragma once
namespace filn::repair {
template<class Descriptor>
bool SlotMatches(const Descriptor& Original,const Descriptor& Actual) {
 return Original==Actual;
}
inline bool CanRestoreJob(bool ValidState,bool OwnLease,bool RouteMatches,bool EndpointsReady) {
 return ValidState && OwnLease && RouteMatches && EndpointsReady;
}
}
