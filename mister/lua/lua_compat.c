// See lauxlib.h in this directory. The functions are those of Lua 5.2's
// lauxlib.c (Lua.org, PUC-Rio, MIT license), as LuaJIT provides them.

#include <lua.h>
#include <lauxlib.h>

void luaL_setfuncs(lua_State *L, const luaL_Reg *l, int nup)
{
    luaL_checkstack(L, nup, "too many upvalues");
    for (; l->name != NULL; l++) {
        int i;
        for (i = 0; i < nup; i++)
            lua_pushvalue(L, -nup);
        lua_pushcclosure(L, l->func, nup);
        lua_setfield(L, -(nup + 2), l->name);
    }
    lua_pop(L, nup);
}

void *luaL_testudata(lua_State *L, int ud, const char *tname)
{
    void *p = lua_touserdata(L, ud);
    if (p != NULL && lua_getmetatable(L, ud)) {
        luaL_getmetatable(L, tname);
        if (!lua_rawequal(L, -1, -2))
            p = NULL;
        lua_pop(L, 2);
        return p;
    }
    return NULL;
}

#define LEVELS1 12
#define LEVELS2 10

static int countlevels(lua_State *L)
{
    lua_Debug ar;
    int li = 1, le = 1;
    while (lua_getstack(L, le, &ar)) {
        li = le;
        le *= 2;
    }
    while (li < le) {
        int m = (li + le) / 2;
        if (lua_getstack(L, m, &ar))
            li = m + 1;
        else
            le = m;
    }
    return le - 1;
}

void luaL_traceback(lua_State *L, lua_State *L1, const char *msg, int level)
{
    lua_Debug ar;
    int top = lua_gettop(L);
    int numlevels = countlevels(L1);
    int mark = (numlevels > LEVELS1 + LEVELS2) ? LEVELS1 : 0;
    if (msg)
        lua_pushfstring(L, "%s\n", msg);
    lua_pushliteral(L, "stack traceback:");
    while (lua_getstack(L1, level++, &ar)) {
        if (level == mark) {
            lua_pushliteral(L, "\n\t...");
            level = numlevels - LEVELS2;
        } else {
            lua_getinfo(L1, "Sln", &ar);
            lua_pushfstring(L, "\n\t%s:", ar.short_src);
            if (ar.currentline > 0)
                lua_pushfstring(L, "%d:", ar.currentline);
            if (*ar.namewhat != '\0')
                lua_pushfstring(L, " in function '%s'", ar.name);
            else if (*ar.what == 'm')
                lua_pushliteral(L, " in main chunk");
            else if (*ar.what == 'C')
                lua_pushliteral(L, " in function <C>");
            else
                lua_pushfstring(L, " in function <%s:%d>", ar.short_src, ar.linedefined);
            lua_concat(L, lua_gettop(L) - top);
        }
    }
    lua_concat(L, lua_gettop(L) - top);
}
