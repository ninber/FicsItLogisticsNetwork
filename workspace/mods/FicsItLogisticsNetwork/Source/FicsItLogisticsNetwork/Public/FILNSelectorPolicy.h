#pragma once
#include <cstdint>
namespace filn::selector {
inline bool ReplyMatches(std::uint32_t pending,std::uint32_t received,bool sameStation) {
 return pending!=0 && pending==received && sameStation;
}
inline bool Confirmed(bool mutationAccepted,bool blank,bool hasRecord,bool matchesNative,bool sameResource) {
 return mutationAccepted && (blank?!hasRecord:(hasRecord && matchesNative && sameResource));
}
}
