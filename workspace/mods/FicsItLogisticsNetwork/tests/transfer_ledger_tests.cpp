// SPDX-License-Identifier: GPL-3.0-only
#include "FILNTransferLedger.h"
#include <iostream>
#include <cstdlib>
using namespace filn::transfer;
#define CHECK(x) do { ++n; if(!(x)) { std::cerr<<__LINE__<<": "<<#x<<'\n'; return EXIT_FAILURE; } } while(false)
int main() {
 int n=0;
 Ledger l{0,0,0};
 CHECK(Record(l,60,true)); CHECK(Record(l,40,true));
 CHECK(l.cargo==100&&l.loaded==100&&l.delivered==0);
 CHECK(Reconciled(l,100,false)); CHECK(!Reconciled(l,99,false)); CHECK(!Reconciled(l,100,true));
 CHECK(Record(l,75,false)); CHECK(l.cargo==25&&l.delivered==75);
 CHECK(!Record(l,26,false)); CHECK(l.cargo==25&&l.delivered==75);
 CHECK(Record(l,25,false)); CHECK(l.cargo==0&&l.delivered==100);
 CHECK(!Record(l,0,true)); CHECK(!Record(l,-1,false));
 Ledger retained{20,0,0}; CHECK(Record(retained,5,false)); CHECK(retained.loaded==0&&retained.delivered==5&&retained.cargo==15);
 Ledger max{std::numeric_limits<int>::max(),0,0};
 CHECK(!Record(max,1,true)); CHECK(max.cargo==std::numeric_limits<int>::max());
 Ledger overflow{1,0,std::numeric_limits<int>::max()}; CHECK(!Record(overflow,1,false)); CHECK(overflow.cargo==1);
 std::cout<<n<<" transfer ledger checks passed\n";
}
