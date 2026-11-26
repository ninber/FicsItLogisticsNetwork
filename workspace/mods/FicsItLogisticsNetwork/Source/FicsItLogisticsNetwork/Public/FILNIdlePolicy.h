#pragma once
namespace filn::idle {
enum class Action { Release, Hold, NativeReturn, RetryEmptyReturn };
struct Snapshot {
 bool owned=false, idle=false, autopilot=false, driver=false, homeValid=false;
 bool homeNearby=false, stopped=false, docked=false, held=false, empty=false;
 bool fault=false, fuel=false;
};
inline Action Evaluate(const Snapshot& s) {
 if(!s.owned||!s.idle||!s.autopilot||s.driver||!s.homeValid) return Action::Release;
 if(s.homeNearby&&s.stopped&&(s.docked||s.held)) return Action::Hold;
 if(!s.empty) return Action::NativeReturn;
 if(s.fault&&s.fuel&&!s.docked) return Action::RetryEmptyReturn;
 return Action::NativeReturn;
}
}
