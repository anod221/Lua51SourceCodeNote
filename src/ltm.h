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


// [@ltm] ��ȡĳ��Table����et��Ԫ����e��lua_TValueʵ��
#define gfasttm(g,et,e) ((et) == NULL ? NULL : \
  ((et)->flags & (1u<<(e))) ? NULL : luaT_gettm(et, e, (g)->tmname[e]))

// [@ltm] ��ȡĳ��Table����et��Ԫ����e��lua_TValueʵ��
#define fasttm(l,et,e)	gfasttm(G(l), et, e)

LUAI_DATA const char *const luaT_typenames[];

// [@ltm] ��ȡTable����events��Ӧevent��Ԫ����
// ���� lua_TValue*
// ��Ҫ��֤event��ename�Ƕ�Ӧ��
// �²ۣ�������������࣡������Ů��
LUAI_FUNC const TValue *luaT_gettm (Table *events, TMS event, TString *ename);

// [@ltm] ��ȡĳ��lua_TValue����o��Ӧevent��Ԫ����
// ���� lua_TValue*
// �²ۣ��Ҿ��ô�Ů������������һ�£�
LUAI_FUNC const TValue *luaT_gettmbyobj (lua_State *L, const TValue *o,
                                                       TMS event);

// [@ltm] ��L��tmnameд��ȫ��Ԫ�����ֶ���
LUAI_FUNC void luaT_init (lua_State *L);

#endif
