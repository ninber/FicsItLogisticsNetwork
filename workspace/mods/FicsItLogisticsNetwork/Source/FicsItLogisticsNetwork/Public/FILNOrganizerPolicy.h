// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <string_view>
namespace filn::organizer {
enum class Side { None, Provider, Requester };
// Only a saved selection identifies a requester; a provider may also be
// identified from an observed homogeneous stock. Depots are never cargo rows.
inline Side StationSide(bool depot,bool provider,std::string_view saved,std::string_view observed) {
 if(depot) return Side::None;
 if(provider) return !saved.empty() || !observed.empty()?Side::Provider:Side::None;
 return !saved.empty()?Side::Requester:Side::None;
}
inline bool CurrentTarget(int index,int target,int count) {
 return index>=0 && target>=0 && index==target && index<count;
}
inline bool HasCapacity(double capacity) { return capacity>0; }
inline bool RequestScan(bool webDemand,double now,double openUntil) {
 return webDemand || now<openUntil;
}
}
