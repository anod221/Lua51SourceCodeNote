/*
** $Id: lstring.h,v 1.43 2005/04/25 19:24:10 roberto Exp $
** String table (keep all strings handled by Lua)
** See Copyright Notice in lua.h
*/

#ifndef lstring_h
#define lstring_h


#include "lgc.h"
#include "lobject.h"
#include "lstate.h"

// [@lstring] TString方法
// 获得某个TString*对象占用的内存大小
#define sizestring(s)	(sizeof(union TString)+((s)->len+1)*sizeof(char))

// [@lstring] Udata方法
// 获得某个Udata*对象占用的内存大小(因为Udata)
#define sizeudata(u)	(sizeof(union Udata)+(u)->len)

// [@lstring] 创建一个TString*对象
// L lua_State*
// s char*
#define luaS_new(L, s)	(luaS_newlstr(L, s, strlen(s)))
// [@lstring] 创建一个TString对象
// L lua_state*
// s 字符串常量
#define luaS_newliteral(L, s)	(luaS_newlstr(L, "" s, \
                                 (sizeof(s)/sizeof(char))-1))
// [@lstring] TString方法
// 设置某个TString为“不要被GC”的状态
#define luaS_fix(s)	l_setbit((s)->tsv.marked, FIXEDBIT)

// [@lstring] 调整全局表中的stringtable的大小为newsize
LUAI_FUNC void luaS_resize (lua_State *L, int newsize);
// [@lstring] 从vm创建一个userdata对象，内存大小是s，并返回
// e userdata使用的env表
LUAI_FUNC Udata *luaS_newudata (lua_State *L, size_t s, Table *e);
// [@lstring] 根据c字符串str和长度l，在vm中创建一个TString对象并返回
// 如果vm中已经有了相同的字符串，那就直接返回
// 即使在stringtable中有相同值，其实还是有O(l)的时间复杂度！！！
// 注意，上面那个O(l)可不是O(1)！！！！！！！
// 不过，在lua代码中，创建一个新字符串的机会并不是特别多，需要特别注意尽量少进行创建操作
LUAI_FUNC TString *luaS_newlstr (lua_State *L, const char *str, size_t l);


#endif
