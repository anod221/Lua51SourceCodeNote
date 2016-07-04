/*
** $Id: ldebug.h,v 2.3 2005/04/25 19:24:10 roberto Exp $
** Auxiliary functions from Debug Interface module
** See Copyright Notice in lua.h
*/

#ifndef ldebug_h
#define ldebug_h


#include "lstate.h"

// [@ldebug] 只在hook检查的时候使用。找到正在执行的指令在整个函数全部指令中的下标
// 为啥要-1？因为hook在检查的时候，总是假定前面已经进行了 pc++ 这样的行为
// see `if (L->hookmask & LUA_MASKCALL)` [luaD_precall处的代码]
// pc: Instruction*
// p: Proto*
#define pcRel(pc, p)	(cast(int, (pc) - (p)->code) - 1)

// [@ldebug] 得到某个指令对应于源文件中的行号
// f: Proto*
// pc: index of f->code，基本上都是配合pcRel使用
#define getline(f,pc)	(((f)->lineinfo) ? (f)->lineinfo[pc] : 0)

// [@ldebug] 把count类型的hook计数器重置
#define resethookcount(L)	(L->hookcount = L->basehookcount)


LUAI_FUNC void luaG_typeerror (lua_State *L, const TValue *o,
                                             const char *opname);
LUAI_FUNC void luaG_concaterror (lua_State *L, StkId p1, StkId p2);
LUAI_FUNC void luaG_aritherror (lua_State *L, const TValue *p1,
                                              const TValue *p2);
LUAI_FUNC int luaG_ordererror (lua_State *L, const TValue *p1,
                                             const TValue *p2);
LUAI_FUNC void luaG_runerror (lua_State *L, const char *fmt, ...);
LUAI_FUNC void luaG_errormsg (lua_State *L);
LUAI_FUNC int luaG_checkcode (const Proto *pt);
LUAI_FUNC int luaG_checkopenop (Instruction i);

#endif
