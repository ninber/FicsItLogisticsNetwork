return {
    -- Enable only after filn_lua.scan / inspect / plan work in your runtime.
    AllowRouteWrites = false,
    AllowAutoDispatch = false, -- Enable after snapshot full-load/return verification.
    AutoTestAllowRouteBridge = false,
    AutoTestAllowDepotJourney = false,
    PollMilliseconds = 1000,
    MaxObservedJobsPerPass = 16,
    JourneyTimeoutSeconds = 1800,
}
