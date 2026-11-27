#pragma once
#include <cstdint>
namespace filn::selector {
inline bool ReplyMatches(std::uint32_t pending,std::uint32_t received,bool sameStation) {
 return pending!=0 && pending==received && sameStation;
}
// An open dropdown is never changed by a read or retry. An expired request
// may be replaced with the latest absolute desired value (idempotent RPC).
inline bool CanRefresh(bool popupOpen,std::uint32_t pending,double now,double deadline) {
 return !popupOpen && (!pending || now>=deadline);
}
inline bool SelectionAcknowledged(bool accepted,bool sameDesiredResource) {
 return accepted && sameDesiredResource;
}
// Only application of a stored selection waits for these operational facts.
inline bool CanReconcile(bool docked,bool reserved,bool compatibleContents) {
 return !docked && !reserved && compatibleContents;
}
// Existing leases use the active filter. Only new assignments are blocked.
inline bool NewDispatchAllowed(bool desiredMatchesActive) {
 return desiredMatchesActive;
}
}
