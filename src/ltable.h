/*
** $Id: ltable.h,v 2.10 2006/01/10 13:13:06 roberto Exp $
** Lua tables (hash)
** See Copyright Notice in lua.h
*/

#ifndef ltable_h
#define ltable_h

#include "lobject.h"

// [@ltable] Table方法
// 获取Table实例t中下标为i的元素
// 返回： Node
#define gnode(t,i)	(&(t)->node[i])

// [@ltable] Node方法
// 获取Node实例n的key的链表项
#define gkey(n)		(&(n)->i_key.nk)

// [@ltable] Node方法
// 获取Node实例n的value项
// 返回 TValue*
#define gval(n)		(&(n)->i_val)

// [@ltable] Node方法
// 获取Node实例n的下一项
// 返回 Node*
#define gnext(n)	((n)->i_key.nk.next)

// [@ltable] Node方法
// 获取Node实例n的key项的值
// 返回 TValue*
#define key2tval(n)	(&(n)->i_key.tvk)


LUAI_FUNC const TValue *luaH_getnum (Table *t, int key);
LUAI_FUNC TValue *luaH_setnum (lua_State *L, Table *t, int key);
LUAI_FUNC const TValue *luaH_getstr (Table *t, TString *key);
LUAI_FUNC TValue *luaH_setstr (lua_State *L, Table *t, TString *key);

// [@ltable] Table方法
// 从t中获取key对应的value
// 相当于代码：
// function luaH_get( t, key ) 
//   assert( type(key)=="anytype" )
//   return reference( t[key] ) or nil
// end
LUAI_FUNC const TValue *luaH_get (Table *t, const TValue *key);

// [@ltable] Table方法
// 从t中获取key对应的value，如果key不存在就创建
// 相当于代码：
// function luaH_set( t, key ) 
//   assert( type(key)~="nil" and key~=NaN )
//   if t[key] == nil then
//     t[key] = new_TValue()
//   end
//   return reference( t[key] )
// end
// luaH_set和luaH_get的不同在于，luaH_set会
// 对key做检查，在key为nil或者nan时候抛出异常
// 并且保证索引不存在的key时会创建新的，保证不会
// 出现返回luaO_nilobject的情况
LUAI_FUNC TValue *luaH_set (lua_State *L, Table *t, const TValue *key);
LUAI_FUNC Table *luaH_new (lua_State *L, int narray, int lnhash);//创建一个含有narray个数组容量和lnhash个kv容量的Table*并返回
LUAI_FUNC void luaH_resizearray (lua_State *L, Table *t, int nasize);
LUAI_FUNC void luaH_free (lua_State *L, Table *t);
LUAI_FUNC int luaH_next (lua_State *L, Table *t, StkId key);
LUAI_FUNC int luaH_getn (Table *t);


#if defined(LUA_DEBUG)
LUAI_FUNC Node *luaH_mainposition (const Table *t, const TValue *key);
LUAI_FUNC int luaH_isdummy (Node *n);
#endif


#endif
