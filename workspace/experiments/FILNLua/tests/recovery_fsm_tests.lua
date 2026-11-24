local root=assert(arg[1]);package.path=root..'/?.lua;'..package.path
local FSM=require('RecoveryFSM');local count=0
local function check(v)assert(v);count=count+1 end
local r=FSM.new(0)
local s={now=0,autopilot=true,driver=false,route_changed=false,home=false,empty=true,fuel=false,error=4}
check(FSM.step(r,s)=='FuelWait');check(r.attempts==0)
s.fuel=true;s.error=1;check(FSM.step(r,s)=='Replan');check(r.attempts==1 and r.next_retry==5)
s.now=1;check(FSM.step(r,s)=='RetryWait');check(r.attempts==1)
s.now=5;check(FSM.step(r,s)=='Replan');check(r.next_retry==15)
s.driver=true;check(FSM.step(r,s)=='DriverWait');s.driver=false
s.autopilot=false;check(FSM.step(r,s)=='Detached');s.autopilot=true
s.route_changed=true;check(FSM.step(r,s)=='Detached');s.route_changed=false
s.home=true;check(FSM.step(r,s)=='HomeReady');s.empty=false;check(FSM.step(r,s)~='HomeReady');s.home=false
s.error=0;s.resumable=true;check(FSM.step(r,s)=='Resume')
s.returning=true;check(FSM.step(r,s)=='Returning')
s.stalled=true;s.now=100;check(FSM.step(r,s)=='Replan')
for i=1,20 do s.now=r.next_retry;check(FSM.step(r,s)=='Replan');check(r.next_retry-s.now<=60)end
check(r.attempts==23) -- No permanent terminal lock after many retries.
print(count..' recovery FSM checks passed; no native integration claimed')
