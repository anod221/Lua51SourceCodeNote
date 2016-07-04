/*
** $Id: lfunc.h,v 2.4 2005/04/25 19:24:10 roberto Exp $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in lua.h
*/

#ifndef lfunc_h
#define lfunc_h


#include "lobject.h"

// [@lfunc] ��ȡһ��CClosureռ�õ��ڴ��С
// �������Ҫ����malloc��ʱ������ڴ������ٿռ�
// n��ʾ���Ǳհ�������upvalue������
// ����n-1����ΪCClosure�Ķ����У�upvalue��TValue upvalue[1]������Ҫ��ȥ1
// ������ʵ��ΪCClosure������ڴ�ռ䣬��ʹ�õ�ʱ����TValue upvalue[n]
#define sizeCclosure(n)	(cast(int, sizeof(CClosure)) + \
                         cast(int, sizeof(TValue)*((n)-1)))

// [@lfunc] ��ȡһ��LClosureռ�õ��ڴ��С
// �������Ҫ����malloc��ʱ������ڴ������ٿռ�
// n��ʾ���Ǳհ�������upvalue������
// ����n-1����ΪLClosure�Ķ����У�upvals��TValue *upvals[1]������Ҫ��ȥ1
// ������ʵ��ΪLClosure������ڴ�ռ䣬��ʹ�õ�ʱ����TValue *upvals[n]
#define sizeLclosure(n)	(cast(int, sizeof(LClosure)) + \
                         cast(int, sizeof(TValue *)*((n)-1)))


LUAI_FUNC Proto *luaF_newproto (lua_State *L);
LUAI_FUNC Closure *luaF_newCclosure (lua_State *L, int nelems, Table *e);
LUAI_FUNC Closure *luaF_newLclosure (lua_State *L, int nelems, Table *e);
LUAI_FUNC UpVal *luaF_newupval (lua_State *L);
LUAI_FUNC UpVal *luaF_findupval (lua_State *L, StkId level);
LUAI_FUNC void luaF_close (lua_State *L, StkId level);
LUAI_FUNC void luaF_freeproto (lua_State *L, Proto *f);
LUAI_FUNC void luaF_freeclosure (lua_State *L, Closure *c);
LUAI_FUNC void luaF_freeupval (lua_State *L, UpVal *uv);
LUAI_FUNC const char *luaF_getlocalname (const Proto *func, int local_number,
                                         int pc);


#endif