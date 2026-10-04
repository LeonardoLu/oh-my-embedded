-- SPDX-License-Identifier: MIT
-- Bounded, platform-independent fluid models used by the two local Works.
local M = {}
local min, max, sqrt, ceil, floor = math.min, math.max, math.sqrt, math.ceil, math.floor

local function clamp(v, lo, hi)
    return max(lo, min(hi, v))
end

local function zeros(n)
    local values = {}
    for i = 1, n do values[i] = 0 end
    return values
end

-- A conservative shallow-water column model. Heights exchange water through
-- staggered face velocities; walls have zero flux and no water is created.
function M.new_liquid()
    local s = { n = 72, width = 44, height = 32, gravity = 36, depth = 11.5 }
    s.dx = s.width / (s.n - 1)
    s.h, s.u, s.flux = zeros(s.n), zeros(s.n), zeros(s.n)
    function s:reset()
        for i = 1, self.n do
            self.h[i] = self.depth
            self.u[i], self.flux[i] = 0, 0
        end
    end
    function s:splash(x, strength)
        local i = clamp(floor(x / self.dx) + 1, 3, self.n - 2)
        local amount = clamp(strength, -1.6, 1.6)
        if amount > 0 then
            amount = min(amount, self.h[i - 1] * 0.2, self.h[i + 1] * 0.2)
        else
            amount = max(amount, -self.h[i] * 0.2)
        end
        self.h[i] = self.h[i] + amount
        self.h[i - 1] = self.h[i - 1] - amount * 0.5
        self.h[i + 1] = self.h[i + 1] - amount * 0.5
        self.u[i - 1] = clamp(self.u[i - 1] - strength, -12, 12)
        self.u[i] = clamp(self.u[i] + strength, -12, 12)
    end
    function s:step(dt)
        dt = clamp(dt, 0, 0.05)
        if dt == 0 then return end
        -- CFL-bound the step using the current deepest column, including
        -- disturbances; the donor limiter keeps all columns positive.
        local deepest = self.depth
        for i = 1, self.n do deepest = max(deepest, self.h[i]) end
        local substeps = ceil(dt * (sqrt(self.gravity * deepest) + 12) / (0.4 * self.dx))
        local step = dt / max(1, substeps)
        for _ = 1, max(1, substeps) do
            for i = 1, self.n - 1 do
                self.u[i] = clamp((self.u[i] - self.gravity *
                    (self.h[i + 1] - self.h[i]) * step / self.dx) *
                    max(0, 1 - step * 0.7), -12, 12)
                local flux = self.u[i] * (self.h[i] + self.h[i + 1]) * 0.5 * step / self.dx
                self.flux[i] = clamp(flux, -self.h[i + 1] * 0.24, self.h[i] * 0.24)
            end
            self.flux[self.n] = 0
            local incoming = 0
            for i = 1, self.n do
                self.h[i] = self.h[i] + incoming - self.flux[i]
                incoming = self.flux[i]
            end
        end
    end
    s:reset()
    return s
end

-- Position-based double-density relaxation with short-range pressure,
-- cohesion, viscosity, gravity and reflecting walls. The spatial hash has a
-- fixed size, and particles are never allocated or removed while stepping.
function M.new_particles()
    local s = { n = 180, width = 32, height = 24, radius = 0.38,
        support = 2.2, gravity = 26, rest_density = 1.8 }
    s.cols, s.rows = ceil(s.width / s.support), ceil(s.height / s.support)
    s.x, s.y, s.vx, s.vy = zeros(s.n), zeros(s.n), zeros(s.n), zeros(s.n)
    s.ox, s.oy, s.pressure, s.near = zeros(s.n), zeros(s.n), zeros(s.n), zeros(s.n)
    s.next, s.cell = zeros(s.n), zeros(s.n)
    s.head = zeros(s.cols * s.rows)
    function s:reset()
        for i = 1, self.n do
            self.x[i] = 8 + ((i - 1) % 15) * 1.1
            self.y[i] = self.height - 1.3 - floor((i - 1) / 15) * 1.05
            self.vx[i], self.vy[i] = 0, 0
        end
    end
    function s:stir(x, y, dx, dy)
        for i = 1, self.n do
            local rx, ry = self.x[i] - x, self.y[i] - y
            local distance = sqrt(rx * rx + ry * ry)
            if distance < 6 then
                local weight = 1 - distance / 6
                local radial = 3 / max(distance, 0.4)
                self.vx[i] = clamp(self.vx[i] + weight * (dx * 8 + rx * radial), -24, 24)
                self.vy[i] = clamp(self.vy[i] + weight * (dy * 8 + ry * radial), -24, 24)
            end
        end
    end
    function s:hash()
        for cell = 1, self.cols * self.rows do self.head[cell] = 0 end
        for i = 1, self.n do
            local col = clamp(floor(self.x[i] / self.support), 0, self.cols - 1)
            local row = clamp(floor(self.y[i] / self.support), 0, self.rows - 1)
            local cell = row * self.cols + col + 1
            self.next[i], self.cell[i], self.head[cell] = self.head[cell], cell, i
        end
    end
    function s:relax(dt)
        self:hash()
        local support2 = self.support * self.support
        for i = 1, self.n do
            local density, near = 0, 0
            local col, row = (self.cell[i] - 1) % self.cols, floor((self.cell[i] - 1) / self.cols)
            for ry = max(0, row - 1), min(self.rows - 1, row + 1) do
                for rx = max(0, col - 1), min(self.cols - 1, col + 1) do
                    local j = self.head[ry * self.cols + rx + 1]
                    while j ~= 0 do
                        if j ~= i then
                            local dx, dy = self.x[j] - self.x[i], self.y[j] - self.y[i]
                            local r2 = dx * dx + dy * dy
                            if r2 < support2 then
                                local q = 1 - sqrt(r2) / self.support
                                density, near = density + q * q, near + q * q * q
                            end
                        end
                        j = self.next[j]
                    end
                end
            end
            self.pressure[i] = clamp(220 * (density - self.rest_density), -90, 500)
            self.near[i] = min(650, 420 * near)
        end
        for i = 1, self.n do
            local col, row = (self.cell[i] - 1) % self.cols, floor((self.cell[i] - 1) / self.cols)
            for ry = max(0, row - 1), min(self.rows - 1, row + 1) do
                for rx = max(0, col - 1), min(self.cols - 1, col + 1) do
                    local j = self.head[ry * self.cols + rx + 1]
                    while j ~= 0 do
                        if j > i then
                            local dx, dy = self.x[j] - self.x[i], self.y[j] - self.y[i]
                            local r2 = dx * dx + dy * dy
                            if r2 < support2 then
                                if r2 < 0.000001 then dx, dy, r2 = 0.001, 0, 0.000001 end
                                local r = sqrt(r2)
                                local q = 1 - r / self.support
                                local d = clamp(0.25 * dt * dt * ((self.pressure[i] +
                                    self.pressure[j]) * q + (self.near[i] + self.near[j]) * q * q), -0.04, 0.1)
                                local px, py = dx * d / r, dy * d / r
                                self.x[i], self.y[i] = self.x[i] - px, self.y[i] - py
                                self.x[j], self.y[j] = self.x[j] + px, self.y[j] + py
                            end
                        end
                        j = self.next[j]
                    end
                end
            end
        end
    end
    function s:viscosity(dt)
        self:hash()
        for i = 1, self.n do
            local col, row = (self.cell[i] - 1) % self.cols, floor((self.cell[i] - 1) / self.cols)
            for ry = max(0, row - 1), min(self.rows - 1, row + 1) do
                for rx = max(0, col - 1), min(self.cols - 1, col + 1) do
                    local j = self.head[ry * self.cols + rx + 1]
                    while j ~= 0 do
                        if j > i then
                            local dx, dy = self.x[j] - self.x[i], self.y[j] - self.y[i]
                            local r = sqrt(dx * dx + dy * dy)
                            if r < self.support then
                                local weight = min(0.1, dt * 2 * (1 - r / self.support))
                                local vx = (self.vx[j] - self.vx[i]) * weight
                                local vy = (self.vy[j] - self.vy[i]) * weight
                                self.vx[i], self.vy[i] = self.vx[i] + vx, self.vy[i] + vy
                                self.vx[j], self.vy[j] = self.vx[j] - vx, self.vy[j] - vy
                            end
                        end
                        j = self.next[j]
                    end
                end
            end
        end
    end
    function s:substep(dt)
        self:viscosity(dt)
        for i = 1, self.n do
            self.ox[i], self.oy[i] = self.x[i], self.y[i]
            self.vy[i] = min(24, self.vy[i] + self.gravity * dt)
            self.x[i] = clamp(self.x[i] + self.vx[i] * dt, self.radius, self.width - self.radius)
            self.y[i] = clamp(self.y[i] + self.vy[i] * dt, self.radius, self.height - self.radius)
        end
        self:relax(dt)
        self:relax(dt)
        for i = 1, self.n do
            self.x[i] = clamp(self.x[i], self.radius, self.width - self.radius)
            self.y[i] = clamp(self.y[i], self.radius, self.height - self.radius)
            self.vx[i] = clamp((self.x[i] - self.ox[i]) / dt * (1 - dt * 0.65), -24, 24)
            self.vy[i] = clamp((self.y[i] - self.oy[i]) / dt * (1 - dt * 0.65), -24, 24)
            if (self.x[i] == self.radius and self.vx[i] < 0) or
               (self.x[i] == self.width - self.radius and self.vx[i] > 0) then
                self.vx[i] = -self.vx[i] * 0.25
            end
            if (self.y[i] == self.radius and self.vy[i] < 0) or
               (self.y[i] == self.height - self.radius and self.vy[i] > 0) then
                self.vy[i] = -self.vy[i] * 0.25
            end
        end
    end
    function s:step(dt)
        dt = clamp(dt, 0, 0.05)
        if dt == 0 then return end
        local steps = ceil(dt / 0.012)
        for _ = 1, steps do self:substep(dt / steps) end
    end
    s:reset()
    return s
end

return M
