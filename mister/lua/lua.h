// MiSTer hybrid core: stands before Lua's lua.h, see lauxlib.h here
#ifndef MISTER_LUA_H
#define MISTER_LUA_H

#include_next <lua.h>

#ifndef LUA_OK
#define LUA_OK 0
#endif
#ifndef lua_rawlen
#define lua_rawlen lua_objlen
#endif

#endif
