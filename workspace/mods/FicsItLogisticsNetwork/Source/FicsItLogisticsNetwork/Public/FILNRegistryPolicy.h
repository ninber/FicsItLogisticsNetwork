// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "FILNCore.h"
namespace filn::registry {
// Absence of a live endpoint is not permission to invent a new lease identity.
// The saved identity must exist, and a surviving endpoint must match it exactly.
inline bool LeaseReference(bool known,bool live,bool matches) {
 return known && (!live || matches);
}
inline bool MayReclaimDestroyed(bool explicitEvent,bool boundOwnerLive,bool lastOwnerLive) {
 return explicitEvent && !boundOwnerLive && !lastOwnerLive;
}
inline bool StateAndJob(std::uint8_t state,bool job) {
 return state<=static_cast<std::uint8_t>(State::Recovery) &&
  (state==static_cast<std::uint8_t>(State::Idle)?!job:job);
}
}
