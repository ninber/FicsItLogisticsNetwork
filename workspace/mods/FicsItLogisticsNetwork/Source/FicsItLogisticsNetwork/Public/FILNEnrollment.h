#pragma once
namespace filn::enrollment {
enum class Route { Empty, HomeOnly, Other };
enum class Decision { Reject, AssignHome, AdoptHome };
struct Snapshot {
 bool authority=false, homeEligible=false, uniqueHome=false, uniqueTruck=false;
 bool driver=false, stopped=false, emptyCargo=false, nearby=false, autopilot=false;
 Route route=Route::Other;
};
inline Decision Evaluate(const Snapshot& s) {
 if(!s.authority || !s.homeEligible || !s.uniqueHome || !s.uniqueTruck ||
    s.driver || !s.stopped || !s.emptyCargo || !s.nearby) return Decision::Reject;
 // Only the explicit registration action may enable a newly assigned, unrouted
 // truck. A previously disabled route is not consent to automatic enrollment.
 if(s.route==Route::Empty && !s.autopilot) return Decision::AssignHome;
 if(s.route==Route::HomeOnly && s.autopilot) return Decision::AdoptHome;
 return Decision::Reject;
}
inline bool ConnectedPortAllowed(bool cargoInventory,bool fuelInventory,bool nativeFuelPort) {
 return !cargoInventory && (fuelInventory || nativeFuelPort);
}
}
