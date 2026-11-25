#include "FILNDockFilterPolicy.h"
#include <cassert>
using namespace filn::dockfilter;
int main() {
 assert(Decide(false,false,true,false,true,true)==Action::Native);
 assert(Decide(true,true,true,false,true,true)==Action::Native);
 assert(Decide(true,false,false,true,false,true)==Action::Block);
 assert(Decide(true,false,true,false,true,true)==Action::Block);
 assert(Decide(true,false,true,false,false,true)==Action::Block);
 assert(Decide(true,false,true,true,false,true)==Action::Native);
 assert(Decide(true,false,true,true,true,true)==Action::SelectiveSolid);
 assert(Decide(true,false,true,true,false,false)==Action::Native);
 assert(Decide(true,false,true,true,true,false)==Action::Block);
 assert(Conserved(50,30,100,120,20));
 assert(!Conserved(50,50,100,120,20));
 assert(!Conserved(50,30,100,100,20));
 assert(!Conserved(50,30,100,120,0));
}
