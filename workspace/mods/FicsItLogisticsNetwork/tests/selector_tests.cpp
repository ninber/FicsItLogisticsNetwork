// SPDX-License-Identifier: GPL-3.0-only
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
 CHECK(!Confirmed(false,false,true,true,true));
 CHECK(!Confirmed(true,false,false,true,true));
 CHECK(!Confirmed(true,false,true,false,true));
 CHECK(!Confirmed(true,false,true,true,false));
 CHECK(Confirmed(true,false,true,true,true));
 CHECK(!Confirmed(false,true,false,false,false));
 CHECK(!Confirmed(true,true,true,true,true));
 CHECK(Confirmed(true,true,false,false,false));
 std::cout << "12 selector protocol checks passed (portable; no Unreal UI/RPC exercised)\n";
}
