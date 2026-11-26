#include "FILNDispatchPolicy.h"
#include <iostream>
#include <cstdlib>
#include <limits>
using namespace filn::dispatch;
#define CHECK(x) do { ++checks; if(!(x)) { std::cerr<<__LINE__<<": "<<#x<<'\n'; return EXIT_FAILURE; } } while(false)
int main() {
 int checks=0;
 CHECK(ProviderResourceMatches("iron","iron",1,""));
 CHECK(ProviderResourceMatches("iron","iron",1,"iron"));
 CHECK(!ProviderResourceMatches("iron","iron",1,"copper"));
 CHECK(!ProviderResourceMatches("iron","copper",1,""));
 CHECK(!ProviderResourceMatches("iron","iron",2,""));
 CHECK(!ProviderResourceMatches("iron","",0,""));
 CHECK(!ProviderResourceMatches("","",1,""));
 Offer o;
 o.vehicle.id="truck"; o.vehicle.home="home"; o.vehicle.enrolled=true; o.vehicle.homeReady=true;
 o.vehicle.idle=true; o.vehicle.autopilot=true; o.vehicle.fuelReady=true; o.vehicle.cargoCapacity=100;
 o.requester.id="request"; o.requester.resource="iron"; o.requester.explicitFilter=true; o.requester.powered=true; o.requester.capacity=100;
 Station p; p.id="provider"; p.resource="iron"; p.loadMode=true; p.powered=true; p.kinds=1; p.quantity=100; p.capacity=100;
 o.provider=p; o.roads={1,2,-1,3};
 CHECK(Evaluate(o).has_value());
 o.provider->depot=true; CHECK(!Evaluate(o)); o.provider->depot=false;
 o.requester.explicitFilter=false; CHECK(!Evaluate(o)); o.requester.explicitFilter=true;
 o.provider->quantity=99; CHECK(!Evaluate(o)); o.provider->quantity=100;
 o.provider->medium=Medium::Fluid; CHECK(!Evaluate(o));
 o.provider->medium=Medium::Solid; o.provider->label="[s]"; o.requester.label="[S]"; CHECK(!Evaluate(o));
 CHECK(WithinNearestGroup(-1,0)); CHECK(WithinNearestGroup(10,10)); CHECK(!WithinNearestGroup(10,10.01));
 CHECK(!WithinNearestGroup(-1,-1)); CHECK(!WithinNearestGroup(10,std::numeric_limits<double>::infinity()));
 CHECK(HomePositionReady(800.0*800.0,250,1,false));
 CHECK(!HomePositionReady(800.0*800.0+1,0,0,false));
 CHECK(!HomePositionReady(100,251,0,false)); CHECK(!HomePositionReady(100,0,-1.1,false));
 CHECK(!HomePositionReady(100,0,0,true));
 CHECK(!HomePositionReady(std::numeric_limits<double>::quiet_NaN(),0,0,false));
 CHECK(!HomePositionReady(-1,0,0,false));
 o.provider->label="Factory"; o.requester.label="Factory";
 Offer equal=o; equal.provider->id="closer-empty"; equal.roads.homeToProvider=0;
 CHECK(Select({o,equal})->provider=="closer-empty");
 std::cout<<checks<<" provider discovery policy checks passed\n";
}
