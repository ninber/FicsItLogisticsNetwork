#include "FILNRouting.h"
#include <cstdlib>
#include <iostream>
#define CHECK(c) do { if(!(c)) {std::cerr<<__LINE__<<": " #c "\n"; std::exit(1);} ++checks; } while(false)
int main() {
 int checks=0;
 filn::roads::ReverseSearch graph(6);
 CHECK(graph.AddEdge(0,1,100)); CHECK(graph.AddEdge(0,2,1));
 CHECK(graph.AddEdge(2,1,2)); CHECK(graph.AddEdge(1,3,4));
 CHECK(graph.AddEdge(3,0,0)); CHECK(graph.AddEdge(4,3,0));
 CHECK(!graph.AddEdge(9,1,3)); CHECK(!graph.AddEdge(0,1,-1));
 CHECK(!graph.AddEdge(0,1,std::numeric_limits<double>::infinity()));
 CHECK(!graph.Start(6)); CHECK(graph.Start(3)); CHECK(!graph.Start(3));
 CHECK(graph.Distance(0)==-1); CHECK(graph.Step(0)==0); CHECK(!graph.Complete());
 int rounds=0;
 while(!graph.Complete()) { CHECK(graph.Step(1)<=1); CHECK(++rounds<100); }
 CHECK(graph.Distance(0)==7); CHECK(graph.Distance(1)==4); CHECK(graph.Distance(2)==6);
 CHECK(graph.Distance(3)==0); CHECK(graph.Distance(4)==0); CHECK(graph.Distance(5)==-1);
 CHECK(!graph.AddEdge(5,3,1));
 filn::roads::ReverseSearch directed(3);
 CHECK(directed.AddEdge(0,1,2)); CHECK(directed.AddEdge(1,2,2)); CHECK(directed.Start(0));
 while(!directed.Complete()) directed.Step(2);
 CHECK(directed.Distance(2)==-1);
 // High fan-in is bounded even when one vertex has thousands of connections.
 filn::roads::ReverseSearch fan(1001);
 for(int i=1;i<=1000;++i) CHECK(fan.AddEdge(i,0,i));
 CHECK(fan.Start(0));
 for(int i=0;i<10;++i) CHECK(fan.Step(7)<=7);
 CHECK(!fan.Complete());
 while(!fan.Complete()) CHECK(fan.Step(7)<=7);
 CHECK(fan.Distance(999)==999);
 std::cout<<checks<<" directed routing and bounded-work checks passed (portable only)\n";
}
