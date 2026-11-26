#pragma once
namespace filn::fuel {
enum class Action { Reject, Ready, Wait, RequestDock, Observe };
struct Snapshot {
 bool valid=false, sameFuel=false, supplied=false, pending=false, cooldown=false;
 int quantity=0, capacity=0;
};
inline Action Decide(const Snapshot& s) {
 if(!s.valid || s.quantity<0 || s.capacity<=0 || s.quantity>s.capacity) return Action::Reject;
 if(s.pending) return Action::Observe;
 if(s.quantity==s.capacity) return Action::Ready;
 if(!s.sameFuel) return Action::Reject;
 return s.supplied&&!s.cooldown?Action::RequestDock:Action::Wait;
}
enum class Receipt { Pending, Received, TimedOut, Cancelled };
inline Receipt Observe(bool valid,bool owned,int before,int after,double elapsed) {
 if(!valid || !owned || before<0 || after<0) return Receipt::Cancelled;
 if(after>before) return Receipt::Received;
 return elapsed>=45.0?Receipt::TimedOut:Receipt::Pending;
}
}
