// SPDX-License-Identifier: MIT
/* Actual output facade + worker + bundled Lua; only board/RTOS are doubles. */
#define WORKS_AUDIO_LUA_TEST
#include "test_audio.c"
#include "lauxlib.h"
#include "lualib.h"
#include "works_audio.h"

static lua_CFunction audio_loader;
esp_err_t cap_lua_register_module(const char *name, lua_CFunction loader)
{
    assert(strcmp(name,"audio")==0 && loader && !audio_loader);
    audio_loader=loader;
    return ESP_OK;
}

static int lua_pump(lua_State *L) { (void)L; pump(); return 0; }
static int lua_opened(lua_State *L) { lua_pushinteger(L,opened); return 1; }
static int lua_volume(lua_State *L) { lua_pushinteger(L,mosaico_audio_get_volume()); return 1; }
static int lua_busy(lua_State *L) { try_lock_busy=lua_toboolean(L,1); return 0; }

static lua_State *new_state(void)
{
    lua_State *L=luaL_newstate();
    assert(L);
    luaL_openlibs(L);
    luaL_requiref(L,"audio",audio_loader,0);
    lua_pop(L,1);
    lua_pushcfunction(L,lua_pump); lua_setglobal(L,"_pump");
    lua_pushcfunction(L,lua_opened); lua_setglobal(L,"_opened");
    lua_pushcfunction(L,lua_volume); lua_setglobal(L,"_volume");
    lua_pushcfunction(L,lua_busy); lua_setglobal(L,"_busy");
    return L;
}

int main(int argc, char **argv)
{
    assert(argc==3 && sizeof(lua_Number)==4 && sizeof(lua_Integer)==4);
    run_audio_tests();
    assert(works_audio_register()==ESP_OK && audio_loader);
    max_pcm_amplitude=32768;
    lua_State *L=new_state();
    lua_pushstring(L,argv[2]); lua_setglobal(L,"_factory_flappy");
    if (luaL_dofile(L,argv[1])!=LUA_OK) {
        fprintf(stderr,"Works audio Lua regression: %s\n",lua_tostring(L,-1));
        return 1;
    }
    lua_close(L);
    pump();

    /* Lua job error/cancel cleanup uses lua_close(), just like cap_lua. Even a
     * reachable output userdata must cancel its queued sound before first open. */
    const int before=opened;
    L=new_state();
    assert(luaL_dostring(L,"held=assert(require('audio').open_output()); assert(held:write(string.rep('\\0\\8',1600))); error('cancel job')")!=LUA_OK);
    lua_close(L);
    pump();
    assert(opened==before);
    L=new_state();
    assert(luaL_dostring(L,"local o=assert(require('audio').open_output()); assert(o:close())")==LUA_OK);
    lua_close(L);
    pump();
    puts("Works audio: bundled float32/int32 Lua, original Flappy SFX, busy retry, userdata GC/error cancellation and stream reuse passed");
    return 0;
}
