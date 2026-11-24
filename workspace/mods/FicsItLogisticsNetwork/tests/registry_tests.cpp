// SPDX-License-Identifier: GPL-3.0-only
#include "FILNRegistryPolicy.h"
#include <iostream>
#include <cstdlib>
int main() {
 int checks=0;
 for(unsigned m=0;m<8;++m) {
  if(filn::registry::LeaseReference(m&1,m&2,m&4)!=((m&1)&&(!(m&2)||(m&4)))) return EXIT_FAILURE;
  ++checks;
 }
 for(unsigned m=0;m<8;++m) {
  if(filn::registry::MayReclaimDestroyed(m&1,m&2,m&4)!=(m==1)) return EXIT_FAILURE;
  ++checks;
 }
 for(unsigned state=0;state<256;++state) for(bool job:{false,true}) {
  if(filn::registry::StateAndJob(static_cast<std::uint8_t>(state),job)!=(state<=8&&(state==0?!job:job))) return EXIT_FAILURE;
  ++checks;
 }
 std::cout<<checks<<" registry preflight policy checks passed\n";
}
