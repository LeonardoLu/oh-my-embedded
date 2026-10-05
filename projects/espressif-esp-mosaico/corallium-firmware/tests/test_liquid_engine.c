/* Validate the actual solver, density assignment and bounded RGB565 renderer. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../overlay/components/mosaic_ui/apps/liquid/liquid_engine.c"

static void inspect_particles(liquid_engine_t *liquid, int expected)
{
    int count;
    float *positions, *velocities;
    flip_get_particles(liquid->fluid, &count, &positions, &velocities);
    assert(count == expected);
    const float radius = flip_get_particle_radius(liquid->fluid);
    for (int i = 0; i < count; ++i) {
        for (int axis = 0; axis < 2; ++axis) {
            const float position = positions[2 * i + axis];
            const float maximum = ((axis == 0 ? liquid->cols : liquid->rows) + 1) * liquid->spacing - radius;
            assert(isfinite(position) && isfinite(velocities[2 * i + axis]));
            assert(position >= liquid->spacing + radius - 0.00001f);
            assert(position <= maximum + 0.00001f);
        }
    }
}

int main(void)
{
    static const uint32_t bright[] = {0x00FF00, 0x00FFFF, 0xFF8800, 0xDDDDDD,
                                      0xFF0000, 0x00CCFF, 0x39FF14, 0xD6AD00};
    for (unsigned palette = 0; palette < LIQUID_PALETTES; ++palette) {
        assert(liquid_engine_color(LIQUID_PIXEL, palette, 1) == bright[palette]);
        assert(liquid_engine_color(LIQUID_PIXEL, palette, 20) == bright[palette]);
        assert(liquid_engine_color(LIQUID_GRADIENT, palette, 0.49f) == s_palettes[palette][0]);
        assert(liquid_engine_color(LIQUID_GRADIENT, palette, 1) == s_palettes[palette][1]);
        assert(liquid_engine_color(LIQUID_GRADIENT, palette, 4) == s_palettes[palette][6]);
        assert(liquid_engine_color(LIQUID_WATER, palette, 10) == s_palettes[palette][3]);
        assert(liquid_engine_color(LIQUID_WATER, palette, 1000) == s_palettes[palette][6]);
        assert(liquid_engine_color(LIQUID_WATER, palette, NAN) == s_palettes[palette][0]);
    }
    assert(liquid_engine_create((liquid_engine_style_t)99) == NULL);
    liquid_engine_destroy(NULL);
    const size_t stride = LIQUID_WIDTH + 7;
    const size_t length = stride * (LIQUID_HEIGHT + 1);
    uint16_t *pixels = malloc(length * sizeof(*pixels));
    assert(pixels != NULL);
    for (int style = LIQUID_PIXEL; style <= LIQUID_WATER; ++style) {
        liquid_engine_t *liquid = liquid_engine_create((liquid_engine_style_t)style);
        assert(liquid != NULL);
        int particles;
        float *positions;
        flip_get_particles(liquid->fluid, &particles, &positions, NULL);
        assert(particles > 0);
        for (int tick = 0; tick < 720; ++tick) {
            const float angle = tick * 0.035f;
            liquid_engine_step(liquid, tick % 79 == 0 ? 3 : 1.0f / 30,
                             sinf(angle) * 20, cosf(angle) * 20);
            inspect_particles(liquid, particles);
        }
        float before = positions[0];
        liquid_engine_step(liquid, NAN, 0, 1);
        liquid_engine_step(liquid, -1, 0, 1);
        liquid_engine_step(liquid, 1, INFINITY, 1);
        assert(positions[0] == before);
        for (size_t i = 0; i < length; ++i) pixels[i] = 0xdead;
        liquid_engine_render(liquid, 5, pixels, stride);
        unsigned colored = 0;
        const uint16_t background = rgb565(s_palettes[5][0]);
        for (int y = 0; y < LIQUID_HEIGHT; ++y) {
            for (int x = 0; x < LIQUID_WIDTH; ++x) {
                assert(pixels[y * stride + x] != 0xdead);
                if (pixels[y * stride + x] != background) {
                    ++colored;
                    if (style == LIQUID_PIXEL) assert(pixels[y * stride + x] == rgb565(bright[5]));
                }
            }
            for (size_t x = LIQUID_WIDTH; x < stride; ++x) assert(pixels[y * stride + x] == 0xdead);
        }
        assert(colored > 0 && colored < LIQUID_WIDTH * LIQUID_HEIGHT);
        for (size_t x = 0; x < stride; ++x) assert(pixels[LIQUID_HEIGHT * stride + x] == 0xdead);
        if (style != LIQUID_WATER) {
            float sum = 0;
            for (int i = 0; i < liquid->cols * liquid->rows; ++i) sum += liquid->grid[i];
            assert(sum == particles); /* One deposit per particle, no bilinear halo. */
            for (int x = 0; x < LIQUID_WIDTH; ++x) assert(pixels[x] == background);
        }
        liquid_engine_destroy(liquid);
    }
    free(pixels);
    puts("PASS: LiquidDuck palettes, rotating/shaking gravity, particle conservation, wall bounds and renderer guards");
    return 0;
}
