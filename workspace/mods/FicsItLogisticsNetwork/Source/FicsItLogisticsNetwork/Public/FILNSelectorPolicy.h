#pragma once
#include <cstdint>
namespace filn::selector {
inline bool ReplyMatches(std::uint32_t pending,std::uint32_t received,bool sameStation) {
 return pending!=0 && pending==received && sameStation;
}
// Only read-only status queries may be retried; mutations are never replayed.
inline bool ShouldQuery(bool editable,std::uint32_t pending,double now,double deadline) {
 return !editable && (!pending || now>=deadline);
}
inline bool Confirmed(bool mutationAccepted,bool blank,bool hasRecord,bool matchesNative,bool sameResource) {
 return mutationAccepted && (blank?!hasRecord:(hasRecord && matchesNative && sameResource));
}
}
