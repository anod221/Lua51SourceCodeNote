/*
** $Id: ldo.h,v 2.7 2005/08/24 16:15:49 roberto Exp $
** Stack and Call structure of Lua
** See Copyright Notice in lua.h
*/

#ifndef ldo_h
#define ldo_h


#include "lobject.h"
#include "lstate.h"
#include "lzio.h"

// [@ldo] 检查vm上的stack数组里面，剩余stack的可用数量
// L lua_State指针
// n stack剩余数量检查值。如果不够n，会触发自动增长
#define luaD_checkstack(L,n)	\
  if ((char *)L->stack_last - (char *)L->top <= (n)*(int)sizeof(TValue)) \
    luaD_growstack(L, n); \
  else condhardstacktests(luaD_reallocstack(L, L->stacksize - EXTRA_STACK - 1));


#define incr_top(L) {luaD_checkstack(L,1); L->top++;}

// [@ldo] 和restorestack搭配使用
// L->stack是整个stack数组的0下标指针
// 所以savestack返回的是栈上的某个lua类型相对于基地址的偏移
// 用法：pdiff_t oldstack = savestack(L, luaValue)
// L lua_State指针
// p 取自栈上的某个lua_TValue的指针
#define savestack(L,p)		((char *)(p) - (char *)L->stack)

// [@ldo] 和savestack搭配使用
// L->stack是整个stack数组的0下标指针
// 所以savestack返回的是栈上的某个lua类型相对于基地址的偏移
// 用法：pdiff_t oldstack = savestack(L, luaValue)
// L lua_State指针
// n 调用savestack返回的数值
#define restorestack(L,n)	((TValue *)((char *)L->stack + (n)))

#define saveci(L,p)		((char *)(p) - (char *)L->base_ci)
#define restoreci(L,n)		((CallInfo *)((char *)L->base_ci + (n)))


/* results from luaD_precall */
#define PCRLUA		0	/* initiated a call to a Lua function */
#define PCRC		1	/* did a call to a C function */
#define PCRYIELD	2	/* C funtion yielded */


/* type of protected functions, to be ran by `runprotected' */
typedef void (*Pfunc) (lua_State *L, void *ud);

LUAI_FUNC int luaD_protectedparser (lua_State *L, ZIO *z, const char *name);
LUAI_FUNC void luaD_callhook (lua_State *L, int event, int line);
// [@ldo] 
// 执行到这里的时候，对于func来说，其stack frame为
// stack-index    value
//  1               func
//  2               param1 for func
//  3               param2 for func
//  ...
//  top-1           paramN for func
LUAI_FUNC int luaD_precall (lua_State *L, StkId func, int nresults);
LUAI_FUNC void luaD_call (lua_State *L, StkId func, int nResults);
LUAI_FUNC int luaD_pcall (lua_State *L, Pfunc func, void *u,
                                        ptrdiff_t oldtop, ptrdiff_t ef);
LUAI_FUNC int luaD_poscall (lua_State *L, StkId firstResult);
LUAI_FUNC void luaD_reallocCI (lua_State *L, int newsize);
LUAI_FUNC void luaD_reallocstack (lua_State *L, int newsize);
LUAI_FUNC void luaD_growstack (lua_State *L, int n);

LUAI_FUNC void luaD_throw (lua_State *L, int errcode);
LUAI_FUNC int luaD_rawrunprotected (lua_State *L, Pfunc f, void *ud);

LUAI_FUNC void luaD_seterrorobj (lua_State *L, int errcode, StkId oldtop);

#endif

