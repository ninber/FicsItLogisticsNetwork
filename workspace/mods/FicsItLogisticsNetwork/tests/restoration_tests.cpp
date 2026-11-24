// SPDX-License-Identifier: GPL-3.0-only
#include "FILNRestore.h"
#include "FILNCore.h"
#include <iostream>
#include <cstdlib>
#include <optional>
using namespace filn::restore;
#define CHECK(x) do { ++n; if(!(x)) { std::cerr<<__LINE__<<": "<<#x<<'\n'; return EXIT_FAILURE; } } while(false)
int main(){
 int n=0; const std::vector<int> before{1,1,1},after{0,2,0};
 auto actual=before; auto read=[&](std::size_t i){return actual[i];};
 auto write=[&](std::size_t i,int v){actual[i]=v;};
 CHECK(Transaction(before,after,read,write)==Result::Success); CHECK(actual==after);
 CHECK(Transaction(before,after,read,write)==Result::Rejected);
 actual=before;
 CHECK(Transaction(before,after,read,[&](std::size_t i,int v){if(i!=1)write(i,v);})==Result::RolledBack); CHECK(actual==before);
 actual=before;
 CHECK(Transaction(before,after,read,[&](std::size_t i,int v){if(i==1)actual[i]=9;else write(i,v);})==Result::Conflict);
 CHECK(actual[0]==1&&actual[1]==9&&actual[2]==1);
 // Native null filter is a value, not an unreadable inventory.
 using Value=std::optional<int>;
 const std::vector<Value> allowed{1,1}, unrestricted{0,0};
 auto slots=allowed;
 auto readSlot=[&](std::size_t i){return slots[i];};
 auto writeSlot=[&](std::size_t i,Value v){slots[i]=v;};
 CHECK(Transaction(allowed,unrestricted,readSlot,writeSlot)==Result::Success);
 CHECK(slots[0].has_value() && slots[0].value()==0);
 slots=allowed; slots[1]=std::nullopt;
 CHECK(Transaction(allowed,unrestricted,readSlot,writeSlot)==Result::Rejected);
 CHECK(slots[0]==allowed[0] && !slots[1]);
 slots=allowed;
 CHECK(Transaction(allowed,unrestricted,readSlot,[&](std::size_t i,Value v){if(i==0){slots[0]=v;slots[1]=std::nullopt;}else writeSlot(i,v);})==Result::Conflict);
 CHECK(slots[0]==allowed[0] && !slots[1]);
 filn::Ownership o; CHECK(o.Pair("v","h")); CHECK(!o.Unpair("v","wrong")); CHECK(o.Unpair("v","h")); CHECK(o.Pair("other","h"));
 filn::Reservations r; CHECK(r.Acquire("j","v","p","q")); CHECK(r.OwnsJob("j","v")); CHECK(!r.OwnsJob("j","other")); r.Release("j"); CHECK(!r.StationBusy("p")&&!r.StationBusy("q"));
 std::cout<<n<<" restoration checks passed\n";
}
