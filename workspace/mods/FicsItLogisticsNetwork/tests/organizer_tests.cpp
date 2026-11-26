#include "FILNOrganizerPolicy.h"
#include <cassert>
int main() {
 using namespace filn::organizer;
 assert(StationSide(true,true,"iron","iron")==Side::None);
 assert(StationSide(true,false,"iron","")==Side::None);
 assert(StationSide(false,true,"iron","")==Side::Provider);
 assert(StationSide(false,true,"","iron")==Side::Provider);
 assert(StationSide(false,true,"","")==Side::None);
 assert(StationSide(false,false,"iron","copper")==Side::Requester);
 assert(StationSide(false,false,"","copper")==Side::None);
 assert(CurrentTarget(2,2,4));
 assert(!CurrentTarget(0,-1,4));
 assert(!CurrentTarget(4,4,4));
 assert(!CurrentTarget(-1,-1,4));
 assert(!HasCapacity(0));
 assert(HasCapacity(4800));
 assert(RequestScan(true,100,0));
 assert(RequestScan(false,100,102));
 assert(!RequestScan(false,102,102));
 assert(!RequestScan(false,100,0));
}
