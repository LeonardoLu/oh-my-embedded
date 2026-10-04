-- SPDX-License-Identifier: MIT
-- Run from repository root: lua projects/espressif-esp-mosaico/corallium-firmware/tests/test_fluid.lua
local root = "projects/espressif-esp-mosaico/corallium-firmware/overlay/fatfs_image/system/"
package.path = root .. "scripts/builtin/lib/?.lua;" .. package.path
local fluid = require("corallium_fluid")
local abs = math.abs

local function finite(v) return v == v and abs(v) < 1000000 end
local function volume(s)
    local total = 0
    for i = 1, s.n do total = total + s.h[i] end
    return total
end

local liquid = fluid.new_liquid()
local initial_volume = volume(liquid)
local float32 = 1.0 + 1e-8 == 1.0
-- The ESP VM uses float32; allow rounding below 0.0005% of initial water.
-- Bounds, positivity and velocity assertions remain exact in both VMs.
local volume_tolerance = initial_volume * (float32 and 5e-6 or 1e-10)
liquid:splash(17, 1.5)
assert(abs(volume(liquid) - initial_volume) < 1e-9, "splash created water")
assert(liquid.h[28] ~= liquid.depth, "liquid input did not disturb water")
for frame = 1, 1200 do
    if frame % 20 == 0 then liquid:splash((frame * 0.37) % liquid.width, math.sin(frame) * 1.6) end
    liquid:step(frame % 3 == 0 and 0.05 or 0.016)
    assert(abs(volume(liquid) - initial_volume) < volume_tolerance, "water volume drift")
    for i = 1, liquid.n do
        assert(finite(liquid.h[i]) and liquid.h[i] > 0, "invalid water depth")
        assert(finite(liquid.u[i]) and abs(liquid.u[i]) <= 12.00001, "unstable liquid velocity")
    end
end
liquid:reset()
for i = 1, liquid.n do assert(liquid.h[i] == liquid.depth and liquid.u[i] == 0) end

local particles = fluid.new_particles()
local first_y = particles.y[180]
particles:step(0.016)
assert(particles.y[180] > first_y, "particle gravity is missing")
particles:stir(particles.x[100], particles.y[100], 0.8, -0.5)
assert(abs(particles.vx[100]) > 0, "touch did not stir particles")
for frame = 1, 360 do
    if frame % 25 == 0 then particles:stir(22 + math.sin(frame) * 12, 22, 0.6, -0.8) end
    particles:step(frame % 3 == 0 and 0.05 or 0.016)
    assert(#particles.x == 180 and #particles.y == 180, "unbounded particle allocation")
    assert(#particles.head == particles.cols * particles.rows, "spatial hash changed size")
    for i = 1, particles.n do
        assert(finite(particles.x[i]) and finite(particles.y[i]), "invalid particle position")
        assert(particles.x[i] >= particles.radius and particles.x[i] <= particles.width - particles.radius, "particle escaped X wall")
        assert(particles.y[i] >= particles.radius and particles.y[i] <= particles.height - particles.radius, "particle escaped Y wall")
        assert(finite(particles.vx[i]) and finite(particles.vy[i]) and abs(particles.vx[i]) <= 24.00001 and abs(particles.vy[i]) <= 24.00001, "unstable particle velocity")
    end
end
-- An exact overlap must separate without division by zero or loss of either particle.
particles.x[1], particles.y[1] = particles.x[2], particles.y[2]
particles:step(0.033)
assert(finite(particles.x[1]) and finite(particles.x[2]))
assert(abs(particles.x[1] - particles.x[2]) + abs(particles.y[1] - particles.y[2]) > 0)
particles:reset()
for i = 1, particles.n do assert(particles.vx[i] == 0 and particles.vy[i] == 0) end

-- Isolate an interacting pair: pressure must resolve an overlap without
-- translating its center of mass; viscosity conserves momentum and reduces
-- the pair's kinetic energy instead of accelerating it.
local pair = fluid.new_particles()
pair.n, pair.gravity = 2, 0
pair.x[1], pair.x[2], pair.y[1], pair.y[2] = 20, 20, 15, 15
pair:step(0.016)
assert(abs(pair.x[1] - pair.x[2]) > 0.001, "pressure did not separate overlap")
assert(abs(pair.x[1] + pair.x[2] - 40) < 1e-7, "pressure changed total momentum")
pair.x[1], pair.x[2], pair.vx[1], pair.vx[2] = 20, 21, 5, -5
pair:viscosity(0.016)
assert(abs(pair.vx[1] + pair.vx[2]) < 1e-7, "viscosity changed total momentum")
assert(pair.vx[1]^2 + pair.vx[2]^2 < 50, "viscosity did not dissipate energy")

-- Execute both shipped entry scripts through the real runner. The fake display
-- checks its frame/close contract; it does not reproduce the fluid algorithm.
local now, presents, closed, activity, points, timeout, commands, frame_open, cancel_after
local function fresh()
    now, presents, closed, activity, timeout, frame_open, cancel_after = 0, 0, false, 0, 0, false, nil
    points, commands = {}, {}
end
local screen = setmetatable({}, { __close = function() closed = true end })
function screen:info() return { width = 480, height = 480, touch_available = true } end
function screen:touch() return { points = points } end
function screen:begin() assert(not frame_open); frame_open = true; commands = {} end
function screen:present() assert(frame_open); frame_open = false; presents = presents + 1 end
local function record(kind, ...)
    assert(frame_open, "drawing outside a frame")
    local command = { kind, ... }
    commands[#commands + 1] = command
end
function screen:text(...) record("text", ...) end
function screen:fill_rect(...) record("rect", ...) end
function screen:stroke_rect(...) record("border", ...) end
function screen:fill_triangle(...) record("triangle", ...) end
function screen:line(...) record("line", ...) end
function screen:fill_circle(...) record("circle", ...) end
package.preload.display = function()
    return { open = function(options) assert(options.framebuffer_count == 1); return screen end,
        color = function(r, g, b) return string.format("#%02x%02x%02x", r, g, b) end }
end
package.preload.system = function() return { millis = function() return now end } end
package.preload.delay = function()
    return { delay_ms = function()
        now = now + 33
        -- Mirror the C helper's signed int32 clock on a 64-bit host VM too.
        -- An ESP VM performs this wrap directly in integer addition.
        if now > 2147483647 then now = now - 4294967296.0 end
        if cancel_after and presents >= cancel_after then error("cooperative stop") end
    end }
end
package.preload.corallium_works = function()
    return { activity = function() activity = activity + 1 end,
        idle_timeout_ms = function() return timeout end,
        millis = function() return assert(math.tointeger(now), "clock must use integers") end }
end

for _, entry in ipairs({ "fluid_liquid", "fluid_particles" }) do
    fresh()
    args = { max_frames = 4 }
    local model, frames = dofile(root .. "apps/" .. entry .. "/scripts/" .. entry .. ".lua")
    assert(frames == 4 and presents == 4 and closed, "entry did not release the RAW screen")
    assert(model.n > 0 and #commands > model.n, "entry rendered a placeholder")
    fresh()
    points, args = { { x = 224, y = 296 } }, { max_frames = 4 }
    local touched = dofile(root .. "apps/" .. entry .. "/scripts/" .. entry .. ".lua")
    assert(activity == 4 and closed, "RAW touches did not reset the shared idle timestamp")
    if touched.h then
        assert(touched.h[34] ~= touched.depth, "runner did not disturb water")
    else
        local stirred = false
        for i = 1, touched.n do if abs(touched.vx[i]) > 0.01 then stirred = true end end
        assert(stirred, "runner did not stir particles")
    end
    fresh()
    args, cancel_after = {}, 2
    local ok, err = pcall(dofile, root .. "apps/" .. entry .. "/scripts/" .. entry .. ".lua")
    assert(not ok and err:find("cooperative stop", 1, true) and closed,
        "job cancellation did not close the RAW screen")
    fresh()
    args, timeout = {}, 66
    local _, idle_frames = dofile(root .. "apps/" .. entry .. "/scripts/" .. entry .. ".lua")
    assert(idle_frames == 2 and closed, "idle policy did not close the RAW screen")
    for _, start in ipairs({ 2147483632, -17 }) do
        fresh()
        now, args, timeout = start, {}, 66
        local _, wrap_frames = dofile(root .. "apps/" .. entry .. "/scripts/" .. entry .. ".lua")
        assert(wrap_frames == 2 and closed, "millis wrap changed the idle deadline")
    end
    fresh()
    points = { { x = 435, y = 30 } }
    dofile(root .. "apps/" .. entry .. "/scripts/" .. entry .. ".lua")
    assert(closed and presents == 0 and activity == 1, "Quit did not close immediately")
    fresh()
    points, args = { { x = 320, y = 30 } }, { max_frames = 3 }
    local paused = dofile(root .. "apps/" .. entry .. "/scripts/" .. entry .. ".lua")
    assert(closed and presents == 3 and activity == 3)
    if paused.h then
        assert(paused.h[1] == paused.depth, "Pause advanced water")
    else
        assert(paused.vy[1] == 0, "Pause advanced particles")
    end
end
args = nil
print("fluid: conservation, pressure/gravity, finite bounds, touch, Pause, Quit, millis wrap and idle cleanup passed (" ..
    (float32 and "float32" or "float64") .. ", " ..
    (math.maxinteger == 2147483647 and "int32" or "int64") .. ")")
