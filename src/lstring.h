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

// [@lstring] TString����
// ���ĳ��TString*����ռ�õ��ڴ��С
#define sizestring(s)	(sizeof(union TString)+((s)->len+1)*sizeof(char))

// [@lstring] Udata����
// ���ĳ��Udata*����ռ�õ��ڴ��С(��ΪUdata)
#define sizeudata(u)	(sizeof(union Udata)+(u)->len)

// [@lstring] ����һ��TString*����
// L lua_State*
// s char*
#define luaS_new(L, s)	(luaS_newlstr(L, s, strlen(s)))
// [@lstring] ����һ��TString����
// L lua_state*
// s �ַ�������
#define luaS_newliteral(L, s)	(luaS_newlstr(L, "" s, \
                                 (sizeof(s)/sizeof(char))-1))
// [@lstring] TString����
// ����ĳ��TStringΪ����Ҫ��GC����״̬
#define luaS_fix(s)	l_setbit((s)->tsv.marked, FIXEDBIT)

// [@lstring] ����ȫ�ֱ��е�stringtable�Ĵ�СΪnewsize
LUAI_FUNC void luaS_resize (lua_State *L, int newsize);
// [@lstring] ��vm����һ��userdata�����ڴ��С��s��������
// e userdataʹ�õ�env��
LUAI_FUNC Udata *luaS_newudata (lua_State *L, size_t s, Table *e);
// [@lstring] ����c�ַ���str�ͳ���l����vm�д���һ��TString���󲢷���
// ���vm���Ѿ�������ͬ���ַ������Ǿ�ֱ�ӷ���
// ��ʹ��stringtable������ֵͬ����ʵ������O(l)��ʱ�临�Ӷȣ�����
// ע�⣬�����Ǹ�O(l)�ɲ���O(1)��������������
// ��������lua�����У�����һ�����ַ����Ļ��Ტ�����ر�࣬��Ҫ�ر�ע�⾡���ٽ��д�������
LUAI_FUNC TString *luaS_newlstr (lua_State *L, const char *str, size_t l);


#endif
