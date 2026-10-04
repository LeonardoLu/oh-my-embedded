// SPDX-License-Identifier: MIT
#include "works_audio.h"

#include "cap_lua.h"
#include "lauxlib.h"
#include "mosaico_audio.h"

#define WORKS_AUDIO_OUTPUT_META "corallium.works.audio.output"

typedef struct {
    uint32_t stream;
} works_audio_output_t;

static int output_error(lua_State *L, esp_err_t err)
{
    const char *message = "audio output: unavailable";
    if (err == ESP_ERR_NOT_FINISHED) message = "audio output: busy";
    if (err == ESP_ERR_INVALID_STATE) message = "audio output: closed or unavailable";
    if (err == ESP_ERR_NO_MEM) message = "audio output: out of memory";
    if (err == ESP_ERR_INVALID_ARG) message = "audio output: expected even s16 PCM within 16384 bytes";
    lua_pushnil(L);
    lua_pushstring(L, message);
    return 2;
}

static int output_gc(lua_State *L)
{
    works_audio_output_t *output = luaL_checkudata(L, 1, WORKS_AUDIO_OUTPUT_META);
    mosaico_audio_stream_close(output->stream);
    output->stream = 0;
    return 0;
}

static int output_close(lua_State *L)
{
    output_gc(L);
    lua_pushboolean(L, 1);
    return 1;
}

static int output_info(lua_State *L)
{
    works_audio_output_t *output = luaL_checkudata(L, 1, WORKS_AUDIO_OUTPUT_META);
    lua_createtable(L, 0, 5);
    lua_pushinteger(L, MOSAICO_AUDIO_SAMPLE_RATE);
    lua_setfield(L, -2, "sample_rate");
    lua_pushinteger(L, 1);
    lua_setfield(L, -2, "channels");
    lua_pushinteger(L, 16);
    lua_setfield(L, -2, "bits");
    lua_pushboolean(L, output->stream != 0);
    lua_setfield(L, -2, "opened");
    lua_pushliteral(L, "output");
    lua_setfield(L, -2, "kind");
    return 1;
}

static int output_write(lua_State *L)
{
    works_audio_output_t *output = luaL_checkudata(L, 1, WORKS_AUDIO_OUTPUT_META);
    size_t bytes = 0;
    const char *pcm = luaL_checklstring(L, 2, &bytes);
    if (output->stream == 0) return output_error(L, ESP_ERR_INVALID_STATE);
    esp_err_t err = mosaico_audio_stream_write(output->stream, pcm, bytes);
    if (err != ESP_OK) return output_error(L, err);
    lua_pushinteger(L, (lua_Integer)bytes);
    return 1;
}

static int open_output(lua_State *L)
{
    int gain = 100;
    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "volume");
        if (!lua_isnil(L, -1)) {
            if (!lua_isinteger(L, -1) || lua_tointeger(L, -1) < 0 || lua_tointeger(L, -1) > 100) {
                lua_pushnil(L);
                lua_pushliteral(L, "audio output: volume must be an integer in 0..100");
                return 2;
            }
            gain = (int)lua_tointeger(L, -1);
        }
        lua_pop(L, 1);
    } else if (!lua_isnoneornil(L, 1)) {
        lua_pushnil(L);
        lua_pushliteral(L, "audio.open_output: expected opts table or no argument");
        return 2;
    }
    /* As in the factory mixer, info() reports the actual negotiated PCM format.
     * This output-only service always uses the board's 16 kHz mono s16 path. */
    works_audio_output_t *output = lua_newuserdata(L, sizeof(*output));
    output->stream = 0;
    luaL_setmetatable(L, WORKS_AUDIO_OUTPUT_META);
    esp_err_t err = mosaico_audio_stream_open(gain, &output->stream);
    if (err != ESP_OK) return output_error(L, err);
    return 1;
}

static int luaopen_works_audio(lua_State *L)
{
    static const luaL_Reg methods[] = {
        {"info", output_info}, {"write", output_write}, {"close", output_close}, {NULL, NULL},
    };
    if (luaL_newmetatable(L, WORKS_AUDIO_OUTPUT_META)) {
        lua_pushcfunction(L, output_gc);
        lua_setfield(L, -2, "__gc");
        lua_newtable(L);
        luaL_setfuncs(L, methods, 0);
        lua_setfield(L, -2, "__index");
    }
    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushcfunction(L, open_output);
    lua_setfield(L, -2, "open_output");
    return 1;
}

esp_err_t works_audio_register(void)
{
    return cap_lua_register_module("audio", luaopen_works_audio);
}
