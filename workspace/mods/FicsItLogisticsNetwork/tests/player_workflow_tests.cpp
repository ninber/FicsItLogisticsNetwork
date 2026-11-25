// SPDX-License-Identifier: GPL-3.0-only
#include "FILNPlayerWorkflow.h"
#include <cassert>
int main() {
 using namespace filn::player;
 using M=CargoMedium;
 assert(Compatible(M::Solid,M::Solid));
 assert(Compatible(M::Fluid,M::Fluid));
 assert(!Compatible(M::Solid,M::Fluid));
 assert(!Compatible(M::Fluid,M::Solid));
 assert(!Compatible(M::Unknown,M::Unknown));
 assert(!Compatible(M::Unknown,M::Fluid));
 DepotFacts free{true,true,true,false,false,false,true,false};
 assert(Free(free));
 auto d=free; d.claimed=true; assert(!Free(d));
 d=free; d.occupied=true; assert(!Free(d));
 d=free; d.reserved=true; assert(!Free(d));
 d=free; d.uniqueNode=false; assert(!Free(d));
 d=free; d.foreignRoute=true; assert(!Free(d));
 d=free; d.powered=false; assert(!Free(d));
 d=free; d.designated=false; assert(!Free(d));
 d=free; d.valid=false; assert(!Free(d));
 assert(!ForeignRoute(true,true,true,false,false));
 assert(ForeignRoute(false,true,true,false,false));
 assert(ForeignRoute(true,false,true,false,false));
 assert(ForeignRoute(true,true,false,false,false));
 assert(ForeignRoute(true,true,true,true,false));
 assert(ForeignRoute(true,true,true,false,true));
 assert(OptedIn(true,true,false,true,false));
 assert(!OptedIn(true,false,false,true,false));
 assert(!OptedIn(true,true,true,true,false));
 assert(!OptedIn(true,true,false,false,false));
 assert(!OptedIn(true,true,false,true,true));
 assert(!OptedIn(false,true,false,true,false));
}
