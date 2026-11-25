// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <limits>
#include <cstdint>
namespace filn::transfer {
struct Ledger { int cargo=0,loaded=0,delivered=0; };
inline bool Record(Ledger& ledger,int amount,bool added) {
 if(amount<=0 || ledger.cargo<0 || ledger.loaded<0 || ledger.delivered<0) return false;
 const std::int64_t cargo=static_cast<std::int64_t>(ledger.cargo)+(added?static_cast<std::int64_t>(amount):-static_cast<std::int64_t>(amount));
 const std::int64_t loaded=static_cast<std::int64_t>(ledger.loaded)+(added?amount:0);
 const std::int64_t delivered=static_cast<std::int64_t>(ledger.delivered)+(added?0:amount);
 const auto max=std::numeric_limits<int>::max();
 if(cargo<0 || cargo>max || loaded>max || delivered>max) return false;
 ledger={static_cast<int>(cargo),static_cast<int>(loaded),static_cast<int>(delivered)};
 return true;
}
// The station call's actual return value is independent evidence of unloading.
// A cargo delta without a successful matching operation is not delivery proof.
inline bool ConfirmedUnload(const Ledger& ledger,int observed,int result,bool mixed) {
 return !mixed && ledger.cargo>=0 && observed>=0 && result>0 && result<=ledger.cargo && ledger.cargo-result==observed;
}
inline bool Reconciled(const Ledger& ledger,int observed,bool mixed) {
 return !mixed && observed>=0 && observed==ledger.cargo;
}
}
