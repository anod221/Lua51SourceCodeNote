/*
** $Id: ltable.h,v 2.10 2006/01/10 13:13:06 roberto Exp $
** Lua tables (hash)
** See Copyright Notice in lua.h
*/

#ifndef ltable_h
#define ltable_h

#include "lobject.h"

// [@ltable] Table����
// ��ȡTableʵ��t���±�Ϊi��Ԫ��
// ���أ� Node
#define gnode(t,i)	(&(t)->node[i])

// [@ltable] Node����
// ��ȡNodeʵ��n��key��������
#define gkey(n)		(&(n)->i_key.nk)

// [@ltable] Node����
// ��ȡNodeʵ��n��value��
// ���� TValue*
#define gval(n)		(&(n)->i_val)

// [@ltable] Node����
// ��ȡNodeʵ��n����һ��
// ���� Node*
#define gnext(n)	((n)->i_key.nk.next)

// [@ltable] Node����
// ��ȡNodeʵ��n��key���ֵ
// ���� TValue*
#define key2tval(n)	(&(n)->i_key.tvk)


LUAI_FUNC const TValue *luaH_getnum (Table *t, int key);
LUAI_FUNC TValue *luaH_setnum (lua_State *L, Table *t, int key);
LUAI_FUNC const TValue *luaH_getstr (Table *t, TString *key);
LUAI_FUNC TValue *luaH_setstr (lua_State *L, Table *t, TString *key);

// [@ltable] Table����
// ��t�л�ȡkey��Ӧ��value
// �൱�ڴ��룺
// function luaH_get( t, key ) 
//   assert( type(key)=="anytype" )
//   return reference( t[key] ) or nil
// end
LUAI_FUNC const TValue *luaH_get (Table *t, const TValue *key);

// [@ltable] Table����
// ��t�л�ȡkey��Ӧ��value�����key�����ھʹ���
// �൱�ڴ��룺
// function luaH_set( t, key ) 
//   assert( type(key)~="nil" and key~=NaN )
//   if t[key] == nil then
//     t[key] = new_TValue()
//   end
//   return reference( t[key] )
// end
// luaH_set��luaH_get�Ĳ�ͬ���ڣ�luaH_set��
// ��key����飬��keyΪnil����nanʱ���׳��쳣
// ���ұ�֤���������ڵ�keyʱ�ᴴ���µģ���֤����
// ���ַ���luaO_nilobject�����
LUAI_FUNC TValue *luaH_set (lua_State *L, Table *t, const TValue *key);
LUAI_FUNC Table *luaH_new (lua_State *L, int narray, int lnhash);//����һ������narray������������lnhash��kv������Table*������
LUAI_FUNC void luaH_resizearray (lua_State *L, Table *t, int nasize);
LUAI_FUNC void luaH_free (lua_State *L, Table *t);
LUAI_FUNC int luaH_next (lua_State *L, Table *t, StkId key);
LUAI_FUNC int luaH_getn (Table *t);


#if defined(LUA_DEBUG)
LUAI_FUNC Node *luaH_mainposition (const Table *t, const TValue *key);
LUAI_FUNC int luaH_isdummy (Node *n);
#endif


#endif
