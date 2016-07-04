/*
** $Id: ltm.h,v 2.6 2005/06/06 13:30:25 roberto Exp $
** Tag methods
** See Copyright Notice in lua.h
*/

#ifndef ltm_h
#define ltm_h


#include "lobject.h"


/*
* WARNING: if you change the order of this enumeration,
* grep "ORDER TM"
*/
typedef enum {
  TM_INDEX,					//[@ltm] function( table, key ) -> TValue
  TM_NEWINDEX,				//[@ltm] function( table, key, value ) -> nil
  TM_GC,					//[@ltm] function( udata ) -> nil
  TM_MODE,					//[@ltm] enum: "kv", "k", "v"
  TM_EQ,					//[@ltm] function( o1, o2 ) -> boolean
  TM_ADD,					//[@ltm] function( o1, o2 ) -> TValue
  TM_SUB,					//[@ltm] function( o1, o2 ) -> TValue
  TM_MUL,					//[@ltm] function( o1, o2 ) -> TValue
  TM_DIV,					//[@ltm] function( o1, o2 ) -> TValue
  TM_MOD,					//[@ltm] function( o1, o2 ) -> TValue
  TM_POW,					//[@ltm] function( o1, o2 ) -> TValue
  TM_UNM,					//[@ltm] function( o1, o2=o1 ) -> TValue
  TM_LEN,					//[@ltm] function( o1, o2=nil ) -> number
  TM_LT,					//[@ltm] function( o1, o2 ) -> boolean
  TM_LE,					//[@ltm] function( o1, o2 ) -> boolean
  TM_CONCAT,				//[@ltm] function( o1, o2 ) -> string of `o1 .. o2`
  TM_CALL,					//[@ltm] function( ... ) -> ?
  TM_N		/* number of elements in the enum */
} TMS;


// [@ltm] 获取某个Table对象et的元方法e的lua_TValue实例
#define gfasttm(g,et,e) ((et) == NULL ? NULL : \
  ((et)->flags & (1u<<(e))) ? NULL : luaT_gettm(et, e, (g)->tmname[e]))

// [@ltm] 获取某个Table对象et的元方法e的lua_TValue实例
#define fasttm(l,et,e)	gfasttm(G(l), et, e)

LUAI_DATA const char *const luaT_typenames[];

// [@ltm] 获取Table对象events对应event的元方法
// 返回 lua_TValue*
// 需要保证event和ename是对应的
// 吐槽：这里参数有冗余！逼死处女座
LUAI_FUNC const TValue *luaT_gettm (Table *events, TMS event, TString *ename);

// [@ltm] 获取某个lua_TValue对象o对应event的元方法
// 返回 lua_TValue*
// 吐槽：我觉得处女座还可以抢救一下！
LUAI_FUNC const TValue *luaT_gettmbyobj (lua_State *L, const TValue *o,
                                                       TMS event);

// [@ltm] 往L的tmname写入全部元方法字段名
LUAI_FUNC void luaT_init (lua_State *L);

#endif
