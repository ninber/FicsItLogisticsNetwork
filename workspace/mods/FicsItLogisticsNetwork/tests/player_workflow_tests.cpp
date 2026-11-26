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
 assert(VacantBay(true,true,false,false));
 assert(!VacantBay(true,true,true,false));
 assert(!VacantBay(true,true,false,true));
 assert(!VacantBay(false,true,false,false));
 assert(!VacantBay(true,false,false,false));
 // Power loss does not consume physical bay capacity.
 auto unpowered=free; unpowered.powered=false;
 assert(!Free(unpowered) && VacantBay(true,unpowered.valid,unpowered.claimed,unpowered.occupied));
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
 assert(Membership(true,true,false,false)); // any route containing a depot
 assert(Membership(true,false,true,false)); // actual arrival
 assert(!Membership(true,false,true,true)); // explicit opt-out, same arrival
 assert(Membership(true,true,true,true)); // explicit new route overrides suppression
 assert(!Membership(true,false,false,false));
 assert(!Membership(false,true,true,false));
 assert(!RemovedDepot(true,false,false));
 assert(RemovedDepot(false,false,true));
 assert(!RemovedDepot(false,true,true)); // arrival-only route not yet written
 assert(RemovedDepot(false,true,false)); // player changed arrival-only route
 assert(MayMove(true,true,false));
 assert(!MayMove(true,false,false));
 assert(!MayMove(true,true,true));
 assert(!MayMove(false,true,false));
}
