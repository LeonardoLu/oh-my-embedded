-- SPDX-License-Identifier: MIT
local display = require("display")
local delay = require("delay")
local policy = require("corallium_works")
local physics = require("corallium_fluid")
local M = {}
local min, max, floor, ceil = math.min, math.max, math.floor, math.ceil

local function clamp(v, lo, hi) return max(lo, min(hi, v)) end
local function elapsed(now, before) return (now - before) % 4294967296.0 end

function M.run(kind)
    assert(kind == "liquid" or kind == "particles", "unknown fluid model")
    local screen <close> = display.open({ framebuffer_count = 1 })
    local info = screen:info()
    local sim = kind == "liquid" and physics.new_liquid() or physics.new_particles()
    if kind == "liquid" then sim:splash(sim.width * 0.35, 1.5) end
    local x0, y0 = 16, 90
    local width, height = info.width - 32, info.height - 136
    local sx, sy = width / sim.width, height / sim.height
    local white, muted = display.color(240, 247, 252), display.color(123, 145, 162)
    local tank, frame = display.color(5, 15, 24), display.color(31, 62, 78)
    local liquid, skin = display.color(16, 116, 162), display.color(103, 232, 251)
    local low, high = display.color(47, 170, 210), display.color(178, 245, 255)
    local title_style = { font_size = 24, color = white }
    local label_style = { font_size = 16, color = muted }
    local button_style = { font_size = 18, color = white }
    local title = kind == "liquid" and "Fluid" or "Dot Fluid"
    local hint = kind == "liquid" and "Drag to make waves" or "Drag to stir particles"
    local paused, held, last_x, last_y = false, false, 0, 0
    local before, last_touch, last_policy = policy.millis(), policy.millis(), policy.millis()
    local idle_timeout = policy.idle_timeout_ms()
    local frames = 0

    local function draw_liquid()
        local bottom = y0 + height
        local py = y0 + height - clamp(sim.h[1] / sim.height * height, 0, height)
        for i = 1, sim.n - 1 do
            local left = x0 + floor((i - 1) * width / (sim.n - 1))
            local right = x0 + ceil(i * width / (sim.n - 1))
            local next_y = y0 + height - clamp(sim.h[i + 1] / sim.height * height, 0, height)
            local lower = ceil(max(py, next_y))
            screen:fill_rect(left, lower, right - left, bottom - lower, liquid)
            screen:fill_triangle(left, floor(py), right, floor(next_y), right, lower, liquid)
            screen:fill_triangle(left, floor(py), right, lower, left, lower, liquid)
            -- The surface is linearly interpolated; it is never a tiled/dotted
            -- animation. Only the numerical water heights define its contour.
            screen:line(left, floor(py), right, floor(next_y), skin)
            py = next_y
        end
        screen:fill_rect(x0, bottom - 8, width, 8, display.color(12, 74, 114))
    end

    local function draw_particles()
        -- Snap only presentation, not physical positions, onto a six-pixel
        -- matrix so pressure/viscosity continue to evolve below the dot grid.
        for i = 1, sim.n do
            local x = x0 + floor(sim.x[i] * sx / 6 + 0.5) * 6
            local y = y0 + floor(sim.y[i] * sy / 6 + 0.5) * 6
            local color = math.abs(sim.vx[i]) + math.abs(sim.vy[i]) > 7 and high or low
            screen:fill_circle(clamp(x, x0 + 3, x0 + width - 3),
                clamp(y, y0 + 3, y0 + height - 3), 2, color)
        end
    end

    while true do
        local now = policy.millis()
        local dt = clamp(elapsed(now, before) / 1000, 0.001, 0.05)
        before = now
        local point = info.touch_available and screen:touch().points[1] or nil
        if point then
            last_touch = now
            policy.activity()
            if not held and point.y < 64 then
                if point.x >= info.width - 82 then break
                elseif point.x >= info.width - 190 then paused = not paused
                elseif point.x >= info.width - 292 then sim:reset() end
            elseif point.y >= y0 and point.y < y0 + height then
                local px = clamp((point.x - x0) / sx, 0, sim.width)
                local py = clamp((point.y - y0) / sy, 0, sim.height)
                if not paused then
                    if kind == "liquid" then
                        sim:splash(px, held and clamp((px - last_x) * 0.15, -0.35, 0.35) + 0.08 or 1.4)
                    else
                        sim:stir(px, py, held and clamp(px - last_x, -1, 1) or 0,
                            held and clamp(py - last_y, -1, 1) or 0)
                    end
                end
                last_x, last_y = px, py
            end
        end
        held = point ~= nil
        if elapsed(now, last_policy) >= 1000 then
            idle_timeout = policy.idle_timeout_ms()
            last_policy = now
        end
        -- Release RAW before the shared idle controller sleeps/shuts down.
        -- The panel handoff then applies the accumulated inactivity safely.
        if idle_timeout > 0 and elapsed(now, last_touch) >= idle_timeout then break end
        if not paused then sim:step(dt) end
        screen:begin({ clear = "#050B11" })
        screen:text(16, 22, title, title_style)
        screen:text(info.width - 284, 27, "Reset", button_style)
        screen:text(info.width - 180, 27, paused and "Play" or "Pause", button_style)
        screen:text(info.width - 72, 27, "Quit", button_style)
        screen:text(16, 64, hint, label_style)
        screen:fill_rect(x0, y0, width, height, tank)
        screen:stroke_rect(x0 - 1, y0 - 1, width + 2, height + 2, frame)
        if kind == "liquid" then draw_liquid() else draw_particles() end
        screen:text(16, info.height - 30, paused and "Paused" or "Touch to interact", label_style)
        screen:present()
        frames = frames + 1
        -- A finite frame count is useful for factory/host validation; normal
        -- launches run until Quit, physical Back, stop request or idle policy.
        if type(args) == "table" and type(args.max_frames) == "number" and frames >= args.max_frames then break end
        delay.delay_ms(10)
    end
    return sim, frames
end

return M
