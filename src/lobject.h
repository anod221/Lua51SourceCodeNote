/*
** $Id: lobject.h,v 2.20 2006/01/18 11:37:34 roberto Exp $
** Type definitions for Lua objects
** ������Lua���ö�������
** See Copyright Notice in lua.h
*/


#ifndef lobject_h
#define lobject_h


#include <stdarg.h>


#include "llimits.h"
#include "lua.h"


/* tags for values visible from Lua */
#define LAST_TAG	LUA_TTHREAD			// [@lobject]: 8��ָ�����һ����������ö��ֵ

#define NUM_TAGS	(LAST_TAG+1)		// [@lobject]: 9 


/*
** Extra tags for non-values
*/
// [@lua]: 9
// LUA_TNIL                                       0
// LUA_TBOOLEAN                           1
// LUA_TLIGHTUSERDATA              2
// LUA_TNUMBER                            3
// LUA_TSTRING                              4
// LUA_TTABLE                                 5
// LUA_TFUNCTION                         6
// LUA_TUSERDATA                        7
// LUA_TTHREAD                             8
// LUA_TPROTO           ------>        9
// LUA_TUPVAL                                10
// LUA_TDEADKEY                           11
#define LUA_TPROTO	(LAST_TAG+1)
// [@lua]: 10
// LUA_TNIL                                       0
// LUA_TBOOLEAN                           1
// LUA_TLIGHTUSERDATA              2
// LUA_TNUMBER                            3
// LUA_TSTRING                              4
// LUA_TTABLE                                 5
// LUA_TFUNCTION                         6
// LUA_TUSERDATA                        7
// LUA_TTHREAD                             8
// LUA_TPROTO                               9
// LUA_TUPVAL            ------>        10
// LUA_TDEADKEY                           11
#define LUA_TUPVAL	(LAST_TAG+2)
// [@lua]: 11
// LUA_TNIL                                       0
// LUA_TBOOLEAN                           1
// LUA_TLIGHTUSERDATA              2
// LUA_TNUMBER                            3
// LUA_TSTRING                              4
// LUA_TTABLE                                 5
// LUA_TFUNCTION                         6
// LUA_TUSERDATA                        7
// LUA_TTHREAD                             8
// LUA_TPROTO                               9
// LUA_TUPVAL                                10
// LUA_TDEADKEY        ------>       11
#define LUA_TDEADKEY	(LAST_TAG+3)


/*
** Union of all collectable objects
*/
// [@lobject]: �ɻ��ն���union from:
// GCheader gch
// TString ts
// UData u
// Closure cl
// Table h
// Proto p
// UpVal uv
// lua_State th
typedef union GCObject GCObject;


/*
** Common Header for all collectable objects (in macro form, to be
** included in other objects)
*/
// [@lobject]: �ɻ��յġ�ʵ���������Ļ������ͷ��
// next: TODO
// tt: TODO
// marked: TODO
#define CommonHeader	GCObject *next; lu_byte tt; lu_byte marked


/*
** Common header in struct form
*/

typedef struct GCheader {
  CommonHeader;
} 
// [@lobject]: �������ֻ�ǰ�CommonHeader��Ϊunion
// �е�һ������Ŀ�ŵ�GCObject��Ϊ�ֽڶ�������õġ�����
// ����ʵ����û�õ���
GCheader;




/*
** Union of all Lua values
*/
typedef union {
  GCObject *gc;		//lua���ö��������
  void *p;			//cָ�룬�Զ���Ŀ������
  lua_Number n;		//numberValue
  int b;			//boolValue
} 
// [@lobject]: һ��wrapper����װ����lua�����ö���ʵ��
// ������ c �����������ֵ��
// b: ����ֵ
// n: ����ֵ
// p: cָ�룬�Զ���Ŀ������
// gc: lua���ö��������
Value;


/*
** Tagged Values
*/
#define TValuefields	Value value; int tt

typedef struct lua_TValue {
  TValuefields;
} 
// [@lobject]: lua���ö�����
// value: ��Ӧc��ֵ
// tt: ���ö�������
TValue;


/* Macros to test type */
// [@lobject]: lua_TValue/TValue����
// �൱��lua���룺
// function(o) 
//     return type(o) == "nil" 
// end
#define ttisnil(o)	(ttype(o) == LUA_TNIL)
// [@lobject]: lua_TValue/TValue����
// �൱��lua���룺
// function(o) 
//     return type(o) == "number" 
// end
#define ttisnumber(o)	(ttype(o) == LUA_TNUMBER)
// [@lobject]: lua_TValue/TValue����
// �൱��lua���룺
// function(o) 
//     return type(o) == "string" 
// end
#define ttisstring(o)	(ttype(o) == LUA_TSTRING)
// [@lobject]: lua_TValue/TValue����
// �൱��lua���룺
// function(o) 
//     return type(o) == "table" 
// end
#define ttistable(o)	(ttype(o) == LUA_TTABLE)
// [@lobject]: lua_TValue/TValue����
// �൱��lua���룺
// function(o) 
//     return type(o) == "function" 
// end
#define ttisfunction(o)	(ttype(o) == LUA_TFUNCTION)
// [@lobject]: lua_TValue/TValue����
// �൱��lua���룺
// function(o) 
//     return type(o) == "boolean" 
// end
#define ttisboolean(o)	(ttype(o) == LUA_TBOOLEAN)
// [@lobject]: lua_TValue/TValue����
// �൱��lua���룺
// function(o) 
//     return type(o) == "userdata" 
// end
#define ttisuserdata(o)	(ttype(o) == LUA_TUSERDATA)
// [@lobject]: lua_TValue/TValue����
// ���ĳ��lua�����Ƿ���һ��thread���͵Ķ���
#define ttisthread(o)	(ttype(o) == LUA_TTHREAD)
// [@lobject]: lua_TValue/TValue����
// ���ĳ��lua�����Ƿ���һ��lightuserdata���͵Ķ���
#define ttislightuserdata(o)	(ttype(o) == LUA_TLIGHTUSERDATA)

/* Macros to access values */
// [@lobject]: lua_TValue/TValue����
// ��ȡĳ��lua_TValueʵ��o�������ֶ�
#define ttype(o)	((o)->tt)
// [@lobject]: lua_TValue/TValue����
// ��ȡĳ��lua_TValueʵ��o������װ��GCObject�ֶ�
#define gcvalue(o)	check_exp(iscollectable(o), (o)->value.gc)
// [@lobject]: lua_TValue/TValue����
// ��ȡĳ��lua_TValueʵ��o����װ��c�����Զ���ָ���ֶ�
#define pvalue(o)	check_exp(ttislightuserdata(o), (o)->value.p)
// [@lobject]: lua_TValue/TValue����
// ��ȡĳ��lua_TValueʵ��o����װ��c���͡����֡��ֶ�
#define nvalue(o)	check_exp(ttisnumber(o), (o)->value.n)
// [@lobject]: lua_TValue/TValue����
// ��ȡĳ��lua_TValueʵ��o�����õġ�lua�ַ���������
#define rawtsvalue(o)	check_exp(ttisstring(o), &(o)->value.gc->ts)
// [@lobject]: lua_TValue/TValue����
// ��ȡĳ��lua_TValueʵ��o�����õġ�lua�ַ������󡱶�Ӧ���ַ������ݽṹ
#define tsvalue(o)	(&rawtsvalue(o)->tsv)
// [@lobject]: lua_TValue/TValue����
// ��ȡĳ��lua_TValueʵ��o�����õ�UpVal����
#define rawuvalue(o)	check_exp(ttisuserdata(o), &(o)->value.gc->u)
// [@lobject]: lua_TValue/TValue����
// TODO
#define uvalue(o)	(&rawuvalue(o)->uv)
// [@lobject]: lua_TValue/TValue����
// ��ȡĳ��lua_TValueʵ��o�����õ�Closure����
#define clvalue(o)	check_exp(ttisfunction(o), &(o)->value.gc->cl)
// [@lobject]: lua_TValue/TValue����
// ��ȡĳ��lua_TValueʵ��o�����õ�Table����
#define hvalue(o)	check_exp(ttistable(o), &(o)->value.gc->h)
// [@lobject]: lua_TValue/TValue����
// ��ȡĳ��lua_TValueʵ��o����װ��c���͵Ĳ����ֶ�
#define bvalue(o)	check_exp(ttisboolean(o), (o)->value.b)
// [@lobject]: lua_TValue/TValue����
// ��ȡĳ��lua_TValueʵ��o�����õ�lua_State����Э�̣�
#define thvalue(o)	check_exp(ttisthread(o), &(o)->value.gc->th)

// [@lobject]: lua_TValue/TValue����
// ��ĳ��lua_TValue���в������ԡ�ֻ��nil/false����true�����򷵻�false
#define l_isfalse(o)	(ttisnil(o) || (ttisboolean(o) && bvalue(o) == 0))

/*
** for internal debug only
*/
// [@lobject]: lua_TValue/TValue������debug assert�ã���������
#define checkconsistency(obj) \
  lua_assert(!iscollectable(obj) || (ttype(obj) == (obj)->value.gc->gch.tt))

// [@lobject]: lua_TValue/TValue������debug assert�ã���������
#define checkliveness(g,obj) \
  lua_assert(!iscollectable(obj) || \
  ((ttype(obj) == (obj)->value.gc->gch.tt) && !isdead(g, (obj)->value.gc)))


/* Macros to set values */
// [@lobject]: lua_TValue/TValue����
// ����ĳ��lua_TValue������nil
#define setnilvalue(obj) ((obj)->tt=LUA_TNIL)

// [@lobject]: lua_TValue/TValue����
// ����ĳ��lua_TValueΪ��������
// ������
// obj ָ��lua_TValue��ָ��
// x һ��������
#define setnvalue(obj,x) \
  { TValue *i_o=(obj); i_o->value.n=(x); i_o->tt=LUA_TNUMBER; }

// [@lobject]: lua_TValue/TValue����
// ����ĳ��lua_TValueΪc�Զ���ָ������
// ������
// obj ָ��lua_TValue��ָ��
// x һ��c��ָ�����
#define setpvalue(obj,x) \
  { TValue *i_o=(obj); i_o->value.p=(x); i_o->tt=LUA_TLIGHTUSERDATA; }

// [@lobject]: lua_TValue/TValue����
// ����ĳ��lua_TValueΪ��������
// ������
// obj ָ��lua_TValue��ָ��
// x 0/1��ʾ�߼���/�߼���
#define setbvalue(obj,x) \
  { TValue *i_o=(obj); i_o->value.b=(x); i_o->tt=LUA_TBOOLEAN; }

// [@lobject]: lua_TValue/TValue����
// ����ĳ��lua_TValueΪstring����
// ������
// L lua_State����
// obj ָ��lua_TValue��ָ��
// x ָ��TString��ָ��
#define setsvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TSTRING; \
    checkliveness(G(L),i_o); }

// [@lobject]: lua_TValue/TValue����
// ����ĳ��lua_TValueΪUpVal����
// ������
// L lua_State����
// obj ָ��lua_TValue��ָ��
// x ָ��UpVal��ָ��
#define setuvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TUSERDATA; \
    checkliveness(G(L),i_o); }

// [@lobject]: lua_TValue/TValue����
// ����ĳ��lua_TValue����Ϊthread����
// ����:
// L lua_State����
// obj ָ��lua_TValue��ָ��
// x ָ��Э��lua_State�����ָ��
#define setthvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TTHREAD; \
    checkliveness(G(L),i_o); }

// [@lobject]: lua_TValue/TValue����
// ����lua_TValue����ΪClosure����
// ������
// L lua_State����
// obj ָ��lua_TValue��ָ��
// x ָ��Closure��ָ��
#define setclvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TFUNCTION; \
    checkliveness(G(L),i_o); }

// [@lobject]: lua_TValue/TValue����
// ����ĳ��lua_TValue����ΪTable����
// ������
// L lua_State����
// obj ָ��lua_TValue��ָ��
// x ָ��Table��ָ��
#define sethvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TTABLE; \
    checkliveness(G(L),i_o); }

// [@lobject]: lua_TValue/TValue����
// TODO
// ������
// L lua_State����
// obj ָ��lua_TValue��ָ��
// x ָ��Proto��ָ��
#define setptvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TPROTO; \
    checkliveness(G(L),i_o); }



// [@lobject]: lua_TValue/TValue����
// ��ĳ��lua_TValue����Ϊ��һ��lua_TValueʵ��������
// ������
// L lua_State����
// obj1 ��ֵ��lua_TValue���󣬽������ó�Ϊ����
// obj2 ��ֵ��lua_TValue������Ϊ�������õ�obj1��ȥ
#define setobj(L,obj1,obj2) \
  { const TValue *o2=(obj2); TValue *o1=(obj1); \
    o1->value = o2->value; o1->tt=o2->tt; \
    checkliveness(G(L),o1); }


/*
** different types of sets, according to destination
*/

/* from stack to (same) stack */

// [@lobject]: lua_TValue/TValue����
// ��ĳ��lua_TValue����Ϊ��һ��lua_TValueʵ��������
// ������
// L lua_State����
// obj1 ��ֵ��lua_TValue���󣬽������ó�Ϊ����
// obj2 ��ֵ��lua_TValue������Ϊ�������õ�obj1��ȥ
#define setobjs2s	setobj
/* to stack (not from same stack) */

// [@lobject]: lua_TValue/TValue����
// ��ĳ��lua_TValue����Ϊ��һ��lua_TValueʵ��������
// ������
// L lua_State����
// obj1 ��ֵ��lua_TValue���󣬽������ó�Ϊ����
// obj2 ��ֵ��lua_TValue������Ϊ�������õ�obj1��ȥ
#define setobj2s	setobj

// [@lobject]: lua_TValue/TValue����
// ����ĳ��lua_TValueΪstring����
// ������
// L lua_State����
// obj ָ��lua_TValue��ָ��
// x ָ��TString��ָ��
#define setsvalue2s	setsvalue

// [@lobject]: lua_TValue/TValue����
// ����ĳ��lua_TValue����ΪTable����
// ������
// L lua_State����
// obj ָ��lua_TValue��ָ��
// x ָ��Table��ָ��
#define sethvalue2s	sethvalue

// [@lobject]: lua_TValue/TValue����
// TODO
// ������
// L lua_State����
// obj ָ��lua_TValue��ָ��
// x ָ��Proto��ָ��
#define setptvalue2s	setptvalue
/* from table to same table */

// [@lobject]: lua_TValue/TValue����
// ��ĳ��lua_TValue����Ϊ��һ��lua_TValueʵ��������
// ������
// L lua_State����
// obj1 ��ֵ��lua_TValue���󣬽������ó�Ϊ����
// obj2 ��ֵ��lua_TValue������Ϊ�������õ�obj1��ȥ
#define setobjt2t	setobj
/* to table */

// [@lobject]: lua_TValue/TValue����
// ��ĳ��lua_TValue����Ϊ��һ��lua_TValueʵ��������
// ������
// L lua_State����
// obj1 ��ֵ��lua_TValue���󣬽������ó�Ϊ����
// obj2 ��ֵ��lua_TValue������Ϊ�������õ�obj1��ȥ
#define setobj2t	setobj
/* to new object */

// [@lobject]: lua_TValue/TValue����
// ��ĳ��lua_TValue����Ϊ��һ��lua_TValueʵ��������
// ������
// L lua_State����
// obj1 ��ֵ��lua_TValue���󣬽������ó�Ϊ����
// obj2 ��ֵ��lua_TValue������Ϊ�������õ�obj1��ȥ
#define setobj2n	setobj

// [@lobject]: lua_TValue/TValue����
// ����ĳ��lua_TValueΪstring����
// ������
// L lua_State����
// obj ָ��lua_TValue��ָ��
// x ָ��TString��ָ��
#define setsvalue2n	setsvalue

// [@lobject]: lua_TValue/TValue����
// ����ĳ��lua_TValueʵ��o�������ֶ�
// ������
// obj ָ��lua_TValue��ָ��
// tt Ҫ������ȥ��lua����
#define setttype(obj, tt) (ttype(obj) = (tt))

// [@lobject]: lua_TValue/TValue����
// ���ĳ��lua_TValueʵ��o�Ƿ���Ա������������������
#define iscollectable(o)	(ttype(o) >= LUA_TSTRING)


// [@lobject]: ����һ��lua����ָ��ΪStkId
// StkId ��ʾ Stack Index
// ������ʹ����д����õ�����Ҫ�Ǵ�lua/C����ջ��ȡ��
// �����ݣ�����ת����ΪStkId��Ҳ����lua_TValue*
typedef TValue *StkId;  



// [@lobject]: lua string����
// ��Ҫ�ֶ�tsv��tsv�����У�
// hash �ַ���hashֵ
// len �ַ�������
// reserved ��������࣬��¼��������ַ�����ȫ�ֱ���luaX_tokens@llex������
// ooooooooooooooooooooooooooooooooooooooo
// ����tsv���⣬TString����һ�����ص������ֶ�
// <c_str>������������tsv���棬// ������raw c
// string(const char*)
typedef union TString {
  L_Umaxalign dummy;  /* ensures maximum alignment for strings */
  struct {
    CommonHeader;
    lu_byte reserved;
    unsigned int hash;
    size_t len;
  } tsv;
} 
// [@lobject]: lua string����
// ��Ҫ�ֶ�tsv��tsv�����У�
// hash �ַ���hashֵ
// len �ַ�������
// reserved ??<TODO: �в��˽���;>
// ooooooooooooooooooooooooooooooooooooooo
// ����tsv���⣬TString����һ�����ص������ֶ�
// <c_str>������������tsv���棬// ������raw c
// string(const char*)
TString;

// [@lobject]: TString����
// ��TString�л�ȡ��c�ַ�����ַ
// PS: ts+1����Ϊvm�У��ַ����ı��浥Ԫ����ǰ��
// һ��TString�ṹ�������һ��const char*��
// ��ϸ������Ҫ�鿴lstring.c
#define getstr(ts)	cast(const char *, (ts) + 1)
// [@lobject]: lua_TValue/TValue����
// ��lua_TValue��ʵ��o�л��c�ַ����ĵ�ַ
#define svalue(o)       getstr(tsvalue(o))


// [@lobject]: Udata���ͣ���Ӧ��lua��userdata
// �����²ۣ�����������ͣ���һ����Ա������û�У�
// ��Ҫ�ֶ���uv���ṩ��
// metatable ��Ӧ��userdata��Ԫ��
// env ��Ӧ��userdata���ڵĻ�����TODO: ����֪�������ã�
// len TODO: ����֪��������
typedef union Udata {
  L_Umaxalign dummy;  /* ensures maximum alignment for `local' udata */
  struct {
    CommonHeader;
    struct Table *metatable;
    struct Table *env;
    size_t len;
  } uv;
} 
// [@lobject]: Udata���ͣ���Ӧ��lua��userdata
// �����²ۣ�����������ͣ���һ����Ա������û�У�
// ��Ҫ�ֶ���uv���ṩ��
// metatable ��Ӧ��userdata��Ԫ��
// env ��Ӧ��userdata���ڵĻ�����TODO: ����֪�������ã�
// len alloc�������ڴ��С
Udata;




/*
** Function Prototypes
*/
typedef struct Proto {
  CommonHeader;
  TValue *k;  /* constants used by the function */
  Instruction *code;
  struct Proto **p;  /* functions defined inside the function */
  int *lineinfo;  /* map from opcodes to source lines */
  struct LocVar *locvars;  /* information about local variables */
  TString **upvalues;  /* upvalue names */
  TString  *source;
  int sizeupvalues;
  int sizek;  /* size of `k' */
  int sizecode;
  int sizelineinfo;
  int sizep;  /* size of `p' */
  int sizelocvars;
  int linedefined;
  int lastlinedefined;
  GCObject *gclist;
  lu_byte nups;  /* number of upvalues */
  lu_byte numparams;	//��¼function(a,b,c,...)�У������ֵı���������
  lu_byte is_vararg;
  lu_byte maxstacksize;
} 
// [@lobject]: ��Ӧһ��lua function�����ݽṹ
// ��ӵ�GCObject,��Ч�ֶΰ���
// k+sizek �����õ��ĳ��������һ������
// code+sizecode �ֽ���ָ�����У����һ������
// p+sizep һ�����飬��¼�ڵ�ǰ�����������涨���Ƕ�׺���������
// lineinfo+sizelineinfo һ�����飬�±��Ӧcode���±꣬��¼��Ӧ��ÿһ��instruction��ԭ�ļ��е��к�
// locvars+sizelocvars �õ��ľֲ�����
// upvalues+sizeupvalues �õ���up value�б�
// source ������Դ����
// linedefined
// lastlinedefined
// gclist
// nups
// numparams
// is_vararg
// maxstacksize 
Proto;


/* masks for new-style vararg */
#define VARARG_HASARG		1	// [@lobject]: ����ΪProto::is_vararg��ֵ
#define VARARG_ISVARARG		2	// [@lobject]: ����ΪProto::is_vararg��ֵ
#define VARARG_NEEDSARG		4	// [@lobject]: ����ΪProto::is_vararg��ֵ


typedef struct LocVar {
  TString *varname;
  int startpc;  /* first point where variable is active */
  int endpc;    /* first point where variable is dead */
} 
// [@lobject]: �ֲ������ṹ
// varname lua�ַ��������������
// startpc �����״�ʹ�õ�ָ��λ��
// endpc �������һ��ʹ�õ�ָ��λ��
LocVar;



/*
** Upvalues
*/

typedef struct UpVal {
  CommonHeader;
  TValue *v;  /* points to stack or to its own value */
  union {
    TValue value;  /* the value (when closed) */
    struct {  /* double linked list (when open) */
      struct UpVal *prev;
      struct UpVal *next;
    } l;
  } u;
} 
// [@lobject]: up-value
// v ����һ��lua_TValue��ָ�룬ָ�����ջ���ϵĲ�����ָ��TValue*
// u ����һ��union������
// u.value <TValue>��ָ��������UpVal����
// u.l <DLinkList<UpValPtr>> UpVal��˫������
UpVal;


/*
** Closures
*/

// [@lobject]: ���ñհ�����
// isC �Ƿ��װ��C����
// nupvalues ���õ�up-value������
// gclist TODO
// env �հ�ָ���õ���env��
#define ClosureHeader \
	CommonHeader; lu_byte isC; lu_byte nupvalues; GCObject *gclist; \
	struct Table *env

typedef struct CClosure {
  ClosureHeader;
  lua_CFunction f;
  TValue upvalue[1];
} 
// [@lobject]: C���ͱհ�
// isC �Ƿ��װ��C����
// nupvalues ���õ�up-value������
// gclist TODO
// env �հ�ָ���õ���env��
// f �����õ���C����int (*f)(lua_State *L)
// upvalue ����һ��truck����֮��upvalue��һ��TValue[nupvalues]����ʵ��TValue����
CClosure;


typedef struct LClosure {
  ClosureHeader;
  struct Proto *p;
  UpVal *upvals[1]; //��Ҳ��һ��trick����֮��������Ϊupvals��һ��UpValPtr[nupvalues]��Ҳ����ָ������
} 
// [@lobject]: lua���ͱհ�
// isC �Ƿ��װ��C����
// nupvalues ���õ�up-value������
// gclist TODO
// env �հ�ָ���õ���env��
// p �����õ���lua��������Ҳ����Proto����
// upvals ��Ҳ��һ��trick����֮��������Ϊupvals��һ��UpValPtr[nupvalues]��Ҳ����ָ������
LClosure;


typedef union Closure {
  CClosure c;
  LClosure l;
} Closure;

// [@lobject]: Closure����
// ���Closureʵ��o�Ƿ��Ƿ�װc�ĺ���
#define iscfunction(o)	(ttype(o) == LUA_TFUNCTION && clvalue(o)->c.isC)
// [@lobject]: Closure����
// ���Closureʵ��o�Ƿ���һ����lua����
#define isLfunction(o)	(ttype(o) == LUA_TFUNCTION && !clvalue(o)->c.isC)


/*
** Tables
*/

typedef union TKey {
  struct {
    TValuefields;
    struct Node *next;  /* for chaining */
  } nk;
  TValue tvk;
} 
// [@lobject]: ����Table�����key
// tvk ������ΪTable��key��lua_TValue����
// nk ��Ȼ��һ��union��������ʵ�ڴ沼���� tvk -> next��ʵ����ֻ��һ��Node*����
// ���ӵ���һ��Node��
TKey;


typedef struct Node {
  TValue i_val;
  TKey i_key;
} 
// [@lobject]: ����Table�����һ��key-value��
// i_key һ��TKey���󣬱���key
// i_val һ��lua_TValue���󣬱���value
Node;


// [@lobject]: ����һ��lua�������õ���Table����
// flag TODO
// lsizenode table����ֵȡ2Ϊ�׵Ķ���ֵ
// metatable ��Ӧ��Ԫ��
// array+sizearray һ��TValue���飬����������Ĳ���
// node TODO
// lastfree TODO
// gclist TODO
typedef struct Table {
  CommonHeader;
  lu_byte flags;  /* 1<<p means tagmethod(p) is not present */ 
  lu_byte lsizenode;  /* log2 of size of `node' array */
  struct Table *metatable;
  TValue *array;  /* array part */
  Node *node;
  Node *lastfree;  /* any free position is before this position */
  GCObject *gclist;
  int sizearray;  /* size of `array' array */
} 
// [@lobject]: ����һ��lua�������õ���Table����
// flag һ����λ��ʶ��Ӧ��Ԫ���ĳ��tag method�Ƿ���ڵ�u32��1��ʾ�����ڣ�0��ʾ��һ��
// lsizenode table����ֵȡ2Ϊ�׵Ķ���ֵ
// metatable ��Ӧ��Ԫ��
// array+sizearray һ��TValue���飬����������Ĳ���
// node ����һ�����飬����Ԫ����Node��table��k-v���� <Node[]>
// lastfree node���棬���һ����λλ�� <Node*>
// gclist TODO
Table;


/*
** `module' operation for hashing (size is always a power of 2)
*/

// [@lobject]: ��������
// ���ܣ���ȡ s ��size�µ���ֵ���൱�� s % size
// Լ��������size������2����
#define lmod(s,size) \
	(check_exp((size&(size-1))==0, (cast(int, (s) & ((size)-1)))))

// [@lobject]: ��������
// ���ܣ�ȡ 2 ��x����
#define twoto(x)	(1<<(x))
// [@lobject]: Table����
// ����: �õ�Table��node����
#define sizenode(t)	(twoto((t)->lsizenode))

// [@lobject]: ȫ��nil����
#define luaO_nilobject		(&luaO_nilobject_)

LUAI_DATA const TValue luaO_nilobject_;

#define ceillog2(x)	(luaO_log2((x)-1) + 1)

// [@lobject]: ��������
// ���ܣ���� math.floor( log2(x) )
LUAI_FUNC int luaO_log2 (unsigned int x);

// [@lobject]: ��intת����Ϊfb��ʽ
// fb��ʽ��8b := XXXXXYYY
// ���㹫ʽ��(YYY|0x08) * 2^(XXXXX-1)
// ���ͣ������ʽת��ʵ����������ġ�ת��
// ��Ľ��k���Դ���x��k��һ��ż��������
// k-x���������ƱȽ�ƽ��
LUAI_FUNC int luaO_int2fb (unsigned int x);

// [@lobject]: fbת����int
// fb��ʽ��8b := XXXXXYYY
// ���㹫ʽ��(YYY|0x08) * 2^(XXXXX-1)
LUAI_FUNC int luaO_fb2int (int x);

// [@lobject]: lua_TValue/TValue����
// �Ƚ�����TValue�Ƿ�����ѧ���������
LUAI_FUNC int luaO_rawequalObj (const TValue *t1, const TValue *t2);

// [@lobject]: ��������
// ��һ������c�淶�������ַ������뵽һ��������������
// s ����c�淶�������ַ������������������߸���������16��������
// result ����s�õ�������Ҫд���Ŀ�������ַ
// ���أ�1 �ɹ� 0ʧ��
LUAI_FUNC int luaO_str2d (const char *s, lua_Number *result);


// [@lobject]: ��valist�ķ�ʽ����lua�����ջ��ѹ���ʽ�������ַ���
LUAI_FUNC const char *luaO_pushvfstring (lua_State *L, const char *fmt,
                                                       va_list argp);

// [@lobject]: �ÿɱ�����ķ�ʽ����lua�����ջ��ѹ���ʽ�������ַ���
LUAI_FUNC const char *luaO_pushfstring (lua_State *L, const char *fmt, ...);

// [@lobject]: ��������
// ����source����out������compileĳ��chunk��ʱ��
// �������chunk����Դ���ļ�������̨�����ַ�����
LUAI_FUNC void luaO_chunkid (char *out, const char *source, size_t len);


#endif

