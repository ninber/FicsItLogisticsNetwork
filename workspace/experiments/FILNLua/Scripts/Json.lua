-- SPDX-License-Identifier: GPL-3.0-only
local M={}
function M.array(t)return setmetatable(t or {},{__json_array=true})end
local function quote(s)
    return '"'..s:gsub('[%z\1-\31\\"]',function(c)
        local map={['"']='\\"',['\\']='\\\\',['\n']='\\n',['\r']='\\r',['\t']='\\t'}
        return map[c]or string.format('\\u%04x',c:byte())
    end)..'"'
end
function M.encode(value,seen)
    local kind=type(value)
    if value==nil then return 'null' end
    if kind=='string'then return quote(value)end
    if kind=='boolean'then return value and 'true'or 'false'end
    if kind=='number'then assert(value==value and value~=math.huge and value~=-math.huge,'non-finite JSON number');return tostring(value)end
    assert(kind=='table','unsupported JSON type '..kind)
    seen=seen or {};assert(not seen[value],'cyclic JSON value');seen[value]=true
    local out={};local meta=getmetatable(value)
    if (meta and meta.__json_array)or #value>0 then
        for i=1,#value do out[#out+1]=M.encode(value[i],seen)end
        seen[value]=nil;return '['..table.concat(out,',')..']'
    end
    local keys={};for k in pairs(value)do assert(type(k)=='string','JSON object key must be a string');keys[#keys+1]=k end;table.sort(keys)
    for _,k in ipairs(keys)do out[#out+1]=quote(k)..':'..M.encode(value[k],seen)end
    seen[value]=nil;return '{'..table.concat(out,',')..'}'
end
function M.write(path,value)
    local file=assert(io.open(path..'.tmp','wb'),'cannot open telemetry temporary file')
    assert(file:write(M.encode(value)));assert(file:close())
    -- Windows rename cannot overwrite. Readers retain their last valid snapshot across this gap.
    os.remove(path);assert(os.rename(path..'.tmp',path),'cannot publish telemetry snapshot')
end
return M
