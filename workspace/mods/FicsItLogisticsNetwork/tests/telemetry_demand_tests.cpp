#include "FILNTelemetryDemand.h"
#include <cassert>
using filn::telemetry::Demand;
int main() {
 Demand d;
 d.Observe(100,false);
 assert(d.ShouldStart(false));
 d.Started();
 assert(!d.ShouldStart(true));
 // A single touch, then lease expiry. Pending survives until accepted.
 d.Observe(101,true);
 d.Observe(101,false);
 assert(d.ShouldStart(true));
 d.Started();
 assert(!d.ShouldStart(true)); // active scan is completed by caller regardless
 // An unseen touch during pause still triggers after its lease expires.
 d.Observe(102,false);
 assert(d.ShouldStart(true));
 d.Started();
 d.Observe(102,false);
 assert(!d.ShouldStart(true));
 // A touch while another scan is active queues one more.
 d.Observe(103,true);
 d.Observe(104,false);
 assert(d.ShouldStart(true));
 d.Started();
 d.Interrupted(true);
 assert(d.ShouldStart(true));
 d.Started();
 d.Failed();
 assert(d.ShouldStart(true));
 d.Started();
 d.Observe(105,true);
 assert(d.ShouldStart(true));
 d.Started();
 assert(d.ShouldStart(true)); // continuous browser demand
 d.Observe(105,false);
 assert(!d.ShouldStart(true));
 d.Observe(0,false);
 assert(!d.ShouldStart(true));
}
