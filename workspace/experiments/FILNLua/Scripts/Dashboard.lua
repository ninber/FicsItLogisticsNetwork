-- Native OpenWebURL is disabled following the manual-test freeze/crash.
-- A pcall cannot recover a hung native engine call. Keep this path free of engine calls.
local M={}
function M.open(api,heartbeat)
    local f=io.open(heartbeat,'rb');local stamp=f and tonumber(f:read(64));if f then f:close()end
    local age=stamp and (api.now()-stamp)
    assert(age and age>=-5 and age<20,'Dashboard server is stopped. Run: bash scripts/start-filn-dashboard.sh')
    api.log('DASHBOARD_MANUAL_OPEN native_browser_disabled=true URL=http://filn.localhost:8765/ ; open manually in Steam overlay browser')
    return false
end
return M
