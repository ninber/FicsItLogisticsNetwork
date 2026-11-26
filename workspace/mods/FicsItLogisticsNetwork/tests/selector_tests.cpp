#include "FILNSelectorPolicy.h"
#include <cstdlib>
#include <iostream>
#define CHECK(v) do { if(!(v)) { std::cerr << __LINE__ << ": " #v "\n"; return EXIT_FAILURE; } } while(false)
int main() {
 using namespace filn::selector;
 CHECK(!ReplyMatches(0,0,true));
 CHECK(!ReplyMatches(1,2,true));
 CHECK(!ReplyMatches(1,1,false));
 CHECK(ReplyMatches(1,1,true));
 CHECK(!ShouldQuery(true,false,0,20,0));
 CHECK(ShouldQuery(false,false,0,20,0));
 CHECK(!ShouldQuery(false,false,3,4.99,5));
 CHECK(ShouldQuery(false,false,3,5,5));
 CHECK(ShouldQuery(false,false,3,20,5));
 CHECK(!ShouldQuery(true,true,0,20,0));
 CHECK(!ShouldQuery(false,true,0,20,0));
 CHECK(!ShouldQuery(false,true,3,20,5));
 CHECK(!Confirmed(false,false,true,true,true));
 CHECK(!Confirmed(true,false,false,true,true));
 CHECK(!Confirmed(true,false,true,false,true));
 CHECK(!Confirmed(true,false,true,true,false));
 CHECK(Confirmed(true,false,true,true,true));
 CHECK(!Confirmed(false,true,false,false,false));
 CHECK(!Confirmed(true,true,true,true,true));
 CHECK(Confirmed(true,true,false,false,false));
 std::cout << "20 selector protocol checks passed (portable; no Unreal UI/RPC exercised)\n";
}
