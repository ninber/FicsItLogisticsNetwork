// SPDX-License-Identifier: GPL-3.0-only
#pragma once
namespace filn::player {
struct DepotFacts {
 bool designated=false, valid=false, powered=false, claimed=false, occupied=false, reserved=false, uniqueNode=false, foreignRoute=false;
};
inline bool Free(const DepotFacts& d) {
 return d.designated && d.valid && d.powered && !d.claimed && !d.occupied && !d.reserved && d.uniqueNode && !d.foreignRoute;
}
// Another new, AP-on single-home claimant may compete for the original depot.
// The first eligible parked candidate claims it on the game thread; others
// subsequently see the ownership and relocate. Other route types never yield.
inline bool ForeignRoute(bool requested,bool homeOnly,bool autopilot,bool driver,bool alreadyOwned) {
 return !requested || !homeOnly || !autopilot || driver || alreadyOwned;
}
inline bool OptedIn(bool authority,bool autopilot,bool driver,bool homeOnly,bool knownVehicle) {
 return authority && autopilot && !driver && homeOnly && !knownVehicle;
}
}
