#include "FILNIdentityPolicy.h"
#include <cassert>
int main(){
 using namespace filn::identity;
 assert(ShouldName(true,false,true));
 assert(!ShouldName(false,false,true));
 assert(!ShouldName(true,true,true));
 assert(!ShouldName(true,false,false));
 assert(!ProviderMember(false,false,false));
 assert(ProviderMember(true,false,false));
 assert(ProviderMember(false,true,false));
 assert(ProviderMember(false,false,true));
}
