// SPDX-License-Identifier: GPL-3.0-only
#include "FILNLegacy.h"
#include <iostream>
#include <cstdlib>
using namespace filn::legacy;
#define CHECK(x) do { ++checks; if(!(x)) { std::cerr<<__LINE__<<": "<<#x<<'\n'; return EXIT_FAILURE; } } while(false)
int main(){
 int checks=0;
 const std::string h(32,'a'),p(32,'b'),q(32,'c');
 std::vector<Entry> e;
 CHECK(Parse(h+"\t"+p+"\t"+q+"\n",e));
 CHECK(e.size()==1&&e[0].home==h);
 CHECK(Match(e[0],{h,p,q,h})==RouteMatch::Pickup);
 CHECK(Match(e[0],{p,q,h})==RouteMatch::Pickup);
 CHECK(Match(e[0],{h})==RouteMatch::Idle);
 CHECK(Match(e[0],{h,p})==RouteMatch::None);
 CHECK(!Parse(h+"\t"+p+"\t"+q+"\n"+h+"\t-\t"+q,e));
 CHECK(e.size()==1&&e[0].provider==p);
 CHECK(!Parse(h+"\t-\t"+h,e));
 CHECK(!Parse(h+"\t"+p+"\t"+p,e));
 CHECK(!Parse("invalid",e));
 CHECK(!Parse(std::string(65537,'a'),e));
 CHECK(Parse(h+"\t-\t"+q+"\r\n",e));
 CHECK(Match(e[0],{h,q,h})==RouteMatch::Delivery);
 CHECK(Match(e[0],{q,h})==RouteMatch::Delivery);
 CHECK(Match(e[0],{h,p,q,h})==RouteMatch::None);
 CHECK(!Guid(std::string(32,'0')));
 CHECK(!Guid(std::string(32,'A')));
 CHECK(IdleAdoption(true,true,true,true,true,false,true,true,true,true));
 CHECK(!IdleAdoption(true,true,true,true,false,false,true,true,true,true));
 CHECK(!IdleAdoption(true,true,true,true,true,true,true,true,true,true));
 CHECK(!IdleAdoption(false,true,true,true,true,false,true,true,true,true));
 std::cout<<checks<<" legacy policy checks passed\n";
}
