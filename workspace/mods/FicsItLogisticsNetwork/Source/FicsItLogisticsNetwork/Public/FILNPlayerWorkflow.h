#pragma once
namespace filn::player {
// Resource forms are converted from native enums at the adapter boundary.
// Unknown is never a compatible cargo medium.
enum class CargoMedium { Unknown, Solid, Fluid };
inline bool Compatible(CargoMedium first,CargoMedium second) {
 return first!=CargoMedium::Unknown && first==second;
}
struct DepotFacts {
 bool designated=false, valid=false, powered=false, claimed=false, occupied=false, reserved=false, uniqueNode=false, foreignRoute=false;
};
// Physical capacity is not departure readiness. Power, routing and temporary
// reservations do not mean the player needs to build another depot.
inline bool VacantBay(bool compatible,bool valid,bool claimed,bool occupiedByOther) {
 return compatible && valid && !claimed && !occupiedByOther;
}
inline bool Free(const DepotFacts& d) {
 return d.designated && d.valid && d.powered && !d.claimed && !d.occupied && !d.reserved && d.uniqueNode && !d.foreignRoute;
}
// Another new, AP-on single-home claimant may compete for the original depot.
// The first eligible parked candidate claims it on the game thread; others
// subsequently see the ownership and relocate. Other route types never yield.
inline bool ForeignRoute(bool requested,bool homeOnly,bool autopilot,bool driver,bool alreadyOwned) {
 return !requested || !homeOnly || !autopilot || driver || alreadyOwned;
}
inline bool Membership(bool authority,bool routeDepot,bool arrival,bool suppressed) {
 return authority && (routeDepot || (arrival && !suppressed));
}
inline bool RemovedDepot(bool routeDepot,bool awaitingIntent,bool originalRouteUnchanged) {
 return !routeDepot && !(awaitingIntent && originalRouteUnchanged);
}
// Membership does not grant permission to move a manually controlled or AP-off truck.
inline bool MayMove(bool authority,bool autopilot,bool driver) {
 return authority && autopilot && !driver;
}
}
