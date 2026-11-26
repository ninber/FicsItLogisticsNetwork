#include "FILNDepotPolicy.h"
#include <cstdlib>
#include <iostream>
using namespace filn::depot;
int main() {
 int checks=0;
 for(unsigned m=0;m<64;++m) {
  Snapshot s{bool(m&1),bool(m&2),bool(m&4),bool(m&8),bool(m&16),bool(m&32)};
  bool designate=(m&3)==3 && !(m&12) && (!(m&32)||(m&16));
  bool remove=(m&1) && !(m&(4|8|16|32));
  if(CanDesignate(s)!=designate || CanRemove(s)!=remove) return EXIT_FAILURE;
  checks+=2;
 }
 std::cout<<checks<<" depot role policy checks passed\n";
}
