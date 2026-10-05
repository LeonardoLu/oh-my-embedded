-- SPDX-License-Identifier: MIT
local audio = require('audio')
assert(audio.open_input == nil and audio.recorder == nil and audio.player == nil)
assert(audio.open_output(false) == nil)
assert(audio.open_output({volume=-1}) == nil)
assert(audio.open_output({volume=101}) == nil)
assert(audio.open_output({volume=2.5}) == nil)
local before = _opened()
local o = assert(audio.open_output({sample_rate=16000, volume=50}))
local info = o:info()
assert(info.sample_rate == 16000 and info.channels == 1 and info.bits == 16 and info.opened)
assert(audio.open_output() == nil)
local pcm = string.rep(string.pack('<i2', 2048), 1600)
assert(o:write('x') == nil)
assert(o:write(string.rep('\0', 16386)) == nil)
_busy(true)
local ok, err = o:write(pcm)
assert(not ok and err:find('busy', 1, true))
_busy(false)
assert(o:write(pcm) == #pcm)
ok, err = o:write(pcm)
assert(not ok and err:find('busy', 1, true))
assert(_opened() == before) -- write only queues; no caller codec open/write.
_pump()
assert(_opened() == before + 1 and _volume() == 50)
assert(o:close() and o:close() and not o:info().opened)
assert(o:write(pcm) == nil)
_pump()

before = _opened()
do
    local abandoned = assert(audio.open_output())
    assert(abandoned:write(pcm) == #pcm)
end
collectgarbage('collect')
_pump()
assert(_opened() == before)

-- A stale finalizer cannot release the stream of the next foreground app.
local old = assert(audio.open_output())
assert(old:close())
local fresh = assert(audio.open_output())
old = nil
collectgarbage('collect')
assert(fresh:write(pcm) == #pcm)
_pump()
assert(fresh:close())
fresh = nil
collectgarbage('collect')
_pump()

-- Execute the unmodified shipped Flappy script. Rendering/touch are doubles;
-- its Lua sound synthesis, calls, retry and real native queue are exercised.
before = _opened()
local frame = 0
local screen = setmetatable({}, {__index=function(_, name)
    if name == 'info' then return function() return {width=480,height=480,touch_available=true} end end
    if name == 'measure_text' then return function(_, text) return #text*8, 16 end end
    if name == 'touch' then return function()
        return {points=frame==1 and {{x=200,y=200}} or {}}
    end end
    return function() return true end
end})
package.preload.display = function() return {
    open=function() return screen end,
    color=function(r,g,b) return (r<<16) | (g<<8) | b end,
} end
package.preload.delay = function() return {delay_ms=function(ms)
    assert(ms == 33)
    _busy(false)
    _pump()
    frame=frame+1
    if frame==2 then _busy(true) end -- First flap write retries on the next frame.
end} end
args = {run_time_ms=1800}
dofile(_factory_flappy)
assert(frame==54 and _opened() >= before+3) -- Ready, flap, crash.
assert(_volume()==50)
assert(audio.open_output():close()) -- Official cleanup released the stream.
_pump()
