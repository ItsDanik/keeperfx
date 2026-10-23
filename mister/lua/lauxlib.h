// MiSTer hybrid core: the game is written for LuaJIT, which the MiSTer
// toolchain cannot build (it needs a 32 bit compiler on the build machine).
// It is built with Lua 5.1 instead, the language LuaJIT implements; this
// header stands before Lua's own on the include path and adds the functions
// of later Lua versions that LuaJIT has and the game uses (lua_compat.c).
#ifndef MISTER_LUA_COMPAT_H
#define MISTER_LUA_COMPAT_H

#include <lua.h>
#include_next <lauxlib.h>

#ifdef __cplusplus
extern "C" {
#endif

void luaL_setfuncs(lua_State *L, const luaL_Reg *l, int nup);
void luaL_traceback(lua_State *L, lua_State *L1, const char *msg, int level);
void *luaL_testudata(lua_State *L, int ud, const char *tname);
#define luaL_newlib(L, l) (lua_createtable(L, 0, sizeof(l) / sizeof((l)[0]) - 1), luaL_setfuncs(L, l, 0))

#ifdef __cplusplus
}
#endif

#endif
