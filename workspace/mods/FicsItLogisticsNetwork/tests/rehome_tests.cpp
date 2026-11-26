#include "FILNRehomePolicy.h"
#include "FILNCore.h"
#include <cstdlib>
#include <iostream>
int main() {
 int checks=0;
 for(unsigned mask=0;mask<2048;++mask) {
  filn::rehome::Snapshot s{bool(mask&1),bool(mask&2),bool(mask&4),bool(mask&8),bool(mask&16),bool(mask&32),bool(mask&64),bool(mask&128),bool(mask&256),bool(mask&512),bool(mask&1024)};
  if(filn::rehome::Allowed(s)!=(mask==1951)) return EXIT_FAILURE;
  ++checks;
 }
 filn::Ownership o;
 if(!o.Pair("truck","old") || !o.Pair("other","busy") || o.Move("truck","old","busy") || !o.CanMove("truck","old","new")) return EXIT_FAILURE;
 if(!o.Move("truck","old","new") || o.CanMove("truck","old","third") || !o.Pair("third","old") || !o.Move("truck","new","new")) return EXIT_FAILURE;
 checks+=8;
 std::cout<<checks<<" rehome checks passed\n";
}
