-- Recovery is observed forever; retries are bounded in frequency, never discarded.
local M={}
function M.new(now)
    return {state='Assess',attempts=0,next_retry=now,since=now}
end
function M.step(r,s)
    if not s.autopilot then return 'Detached','player-autopilot-off' end
    if s.driver then return 'DriverWait','driver-present' end
    if s.route_changed then return 'Detached','player-route-changed' end
    if s.home and s.empty then return 'HomeReady','empty-truck-at-home' end
    if not s.fuel then return 'FuelWait','native-fuel-unavailable' end
    if s.returning and s.error==0 and not s.stalled then return 'Returning','native-return-in-progress' end
    if not s.returning and s.resumable and s.error==0 and not s.stalled then return 'Resume','native-fault-cleared' end
    if s.now<r.next_retry then return 'RetryWait','retry-backoff' end
    r.attempts=r.attempts+1
    r.next_retry=s.now+math.min(60,5*2^math.min(r.attempts-1,4))
    return 'Replan',s.error==2 and 'not-on-road' or 'native-path-retry'
end
return M
