/*
** $Id: lstate.h,v 2.24 2006/02/06 18:27:59 roberto Exp $
** Global State
** ��������ݽṹ����
** See Copyright Notice in lua.h
*/

#ifndef lstate_h
#define lstate_h

#include "lua.h"

#include "lobject.h"
#include "ltm.h"
#include "lzio.h"



struct lua_longjmp;  /* defined in ldo.c */


/* table of globals */
// [@lstate] lua_State����
// ����ĳ��VM�����ȫ�ֱ�_G
#define gt(L)	(&L->l_gt)

/* registry */
// [@lstate] lua_State����
// ����lua_State�����ע��� TValue*
#define registry(L)	(&G(L)->l_registry)


/* extra stack space to handle TM calls and some other extras */
#define EXTRA_STACK   5


#define BASIC_CI_SIZE           8

#define BASIC_STACK_SIZE        (2*LUA_MINSTACK)			/* [@lstate] 40 */



typedef struct stringtable {
  GCObject **hash;
  lu_int32 nuse;  /* number of elements */
  int size;
}
// [@lstate] ȫ���ַ������ṹ
// �����ֶ�:
// hash һ����ά���飬�����ÿ��Ԫ����һ��GCObject
// nuse ����Ѿ�ʹ�õ�λ������
// size ���������
stringtable;


/*
** informations about a call
*/
typedef struct CallInfo {
  StkId base;  // [@lstate] ����ʱ��ջ��ָ�� <lua_TValue*>
  StkId func;  // [@lstate] CallInfo��Ӧ�ĺ�����Closure���� <lua_TValue*>
  StkId	top;  // [@lstate] ����ʱ��ջ��ָ��(��L->top��ͬ��L->topָ����ʹ�ò���β����CI->topָ�����ۿ�ʹ�õ����) <lua_TValue*>
  const Instruction *savedpc;
  int nresults;  /* expected number of results from this function */
  int tailcalls;  /* number of tail calls lost under this entry */
} 
// [@lstate] lua_State���������浱ǰ�����Ľṹ
// �����ֶ�:
// func CallInfo��Ӧ�ĺ�����Closure���� <lua_TValue*>
// base ����ʱ��ջ��ָ�� <lua_TValue*>
// top ����ʱ��ջ��ָ��(��L->top��ͬ��L->topָ����ʹ�ò���β����CI->topָ�����ۿ�ʹ�õ����) <lua_TValue*>
// savedpc ����ʱVM��ǰָ��λ�� <Instruction*>
// nresult ��������ֵ���� <int>
// tailcalls TODO <int>
CallInfo;


// [@lstate] lua_State����
// ��ȡ��ǰ����ִ�еĺ���(lua_TValue)����
#define curr_func(L)	(clvalue(L->ci->func))
// [@lstate] CallInfo����
// ��ȡ��Ա����func��getter
#define ci_func(ci)	(clvalue((ci)->func))
// [@lstate] CallInfo����
// ��ǰ�����Ƿ���lua����
#define f_isLua(ci)	(!ci_func(ci)->c.isC)
// [@lstate] CallInfo����
// ��ǰ�����Ƿ���lua����
#define isLua(ci)	(ttisfunction((ci)->func) && f_isLua(ci))


/*
** `global state', shared by all threads of this state
*/
typedef struct global_State {
  stringtable strt;  /* hash table for strings */
  lua_Alloc frealloc;  /* function to reallocate memory */
  void *ud;         /* auxiliary data to `frealloc' */
  lu_byte currentwhite;
  lu_byte gcstate;  /* state of garbage collector */
  int sweepstrgc;  /* position of sweep in `strt' */
  GCObject *rootgc;  /* list of all collectable objects */
  GCObject **sweepgc;  /* position of sweep in `rootgc' */
  GCObject *gray;  /* list of gray objects */
  GCObject *grayagain;  /* list of objects to be traversed atomically */
  GCObject *weak;  /* list of weak tables (to be cleared) */
  GCObject *tmudata;  /* last element of list of userdata to be GC */
  Mbuffer buff;  /* temporary buffer for string concatentation */
  lu_mem GCthreshold;
  lu_mem totalbytes;  /* number of bytes currently allocated */
  lu_mem estimate;  /* an estimate of number of bytes actually in use */
  lu_mem gcdept;  /* how much GC is `behind schedule' */
  int gcpause;  /* size of pause between successive GCs */
  int gcstepmul;  /* GC `granularity' */
  lua_CFunction panic;  /* to be called in unprotected errors */
  TValue l_registry;
  struct lua_State *mainthread;
  UpVal uvhead;  /* head of double-linked list of all open upvalues */
  struct Table *mt[NUM_TAGS];  /* metatables for basic types */
  TString *tmname[TM_N];  /* array with tag-method names */
} 
// [@lstate] vm����ʱȫ�����ݱ�
// strt �ַ�����<stringtable>
// frealloc �ط��亯�� <c����>
// ud frealloc��ĳ�������õ��Զ������� <void*>
// currentwhite TODO<Ӧ����GC�õĶ���> <int>
// gcstate ��ǰGC���е�״̬ <int>
// sweepstrgc TODO<Ӧ����GC�õĶ���> <int>
// rootgc TODO<Ӧ����GC�õĶ���> <GCObject*>
// sweepgc TODO<Ӧ����GC�õĶ���> <GCObject*>
// gray TODO<Ӧ����GC�õĶ���> <GCObject*>
// grayagain TODO<Ӧ����GC�õĶ���> <GCObject*>
// weak ���Ϊdirty��ȫ������ <GCObject*>
// tmudata TODO<Ӧ����GC�õĶ���> <GCObject*>
// buff �����ַ���concat�õ�buffer���ڴ�����cache<MBuffer>
// GCthreshold ����GC���ڴ���ֵ<int>
// totalbytes �ܹ������롱���ڴ��С<int>
// estmimate ����ʹ�á����ڴ��С���Ǹ�����ֵ<int>
// gcdept TODO<Ӧ����GC�õĶ���> <int>
// gcpause TODO<Ӧ����GC�õĶ���> <int>
// gcstepmul TODO<Ӧ����GC�õĶ���> <int>
// panic ����uncatching exception��c���� <c����>
// l_registry ȫ��ע���<TValue>
// mainthread VM��������Э��<lua_State*>
// uvhead TODO <UpVal>
// mt Ϊ�����������õ�metatable <Table*[]>
// tmname metatable�еı���key�б� <TString*[]>
global_State;


/*
** `per thread' state
*/
// [@lstate] һ��������̵߳��ȼ��ϣ����Ա�GC
// 1 status ��ǰ�߳�״̬<int>
// 1 l_G ȫ�ֱ�ָ�� <global_State*>
// 1 savedpc ����ʱVM��ǰָ��λ�� <Instruction*>
// 1 nCcalls ��¼����c����luaD_call�������˼��Σ��ﵽһ������������� <int>
// 1 l_gt _G�� <TValue>
// 2 ====== ����ʱ����ջ ======
// 2 ci ��ǰ����������Ϣ <CallInfo*>
// 2 base_ci, end_ci, size_ci: CallInfo����
// 2 ���һ�����飬base_ci���б���Ԫ�أ�end_ci���б�βԪ�أ�size_ci���б���
// 2 ͨ��ci�õ���ǰջ��������ǰ��ci��caller��caller��caller��
// 3 ====== ����ʱ����ջ ======
// 3 base ��ǰ��������ʱ����ջ��ָ�� <lua_TValue*>
// 3 top ��ǰ��������ʱ����ջβָ�� <lua_TValue*>
// 3 ����lua���룺 f(1, '2', _3) local a, b, c end
// 3 stack-index    rev-stack-index(CClosure only)     value
// 3  1 <- base      -3                                  1
// 3  2              -2                                  '2'
// 3  3              -1 <- top                           _3
// 3  4              index(a) = 4                        nil
// 3  5              index(b) = 5                        nil
// 3  6              index(c) = 6                        nil
// 4 ====== ����ʱ����ջ���� ======
// 4 stack stack_last stacksize: TValue����
// 4 stack��������Ԫ�أ�stack_last������βԪ�أ�stacisize�����鳤��
// 4 ����ջ�����к������õ������������ǿ���Խ����ʱ�ĺ����Ĳ����ġ�����PCһ�£�
// 5 ====== ����ʱ������Ϣ =====
// 5 hookmask ��ǰ���õ�hook״̬ <int>
// 5 allowhook hook���õĿ��ء���������Ǳ�Ҫ�ģ���Ȼ��hook�ĺ�������ô����hook? <int>
// 5 basehookcount hookcount ʵ�֡�ÿִ��count��ָ��ִ��һ��hook��,ǰ���ǳ����������Ǽ�¼ֵ <int>
// 5 hook cд��hook���� <c����>
// env TODO <TValue>
// openupval upvalue������UpVal::v�������У�����ʱ����˳�� <GCObject*>
// gclist TODO <GCObject*>
// errorJmp TODO <lua_longjmp*>
// errfunc ���������� <ptrdiff_t>
struct lua_State {
  CommonHeader;
  lu_byte status;
  StkId top;  /* first free slot in the stack */
  StkId base;  /* base of current function */
  global_State *l_G;
  CallInfo *ci;  /* call info for current function */
  const Instruction *savedpc;  /* `savedpc' of current function */
  StkId stack_last;  /* last free slot in the stack */
  StkId stack;  /* stack base */
  CallInfo *end_ci;  /* points after end of ci array*/
  CallInfo *base_ci;  /* array of CallInfo's */
  int stacksize;
  int size_ci;  /* size of array `base_ci' */
  unsigned short nCcalls;  // [@lstate] ��¼����c����luaD_call�������˼��Σ��ﵽһ�������������
  lu_byte hookmask;
  lu_byte allowhook;
  int basehookcount;
  int hookcount;
  lua_Hook hook;
  TValue l_gt;  /* table of globals */
  TValue env;  /* temporary place for environments */
  GCObject *openupval;  /* list of open upvalues in this stack */
  GCObject *gclist;
  struct lua_longjmp *errorJmp;  /* current error recover point */
  ptrdiff_t errfunc;  /* current error handling function (stack index) */
};

// [@lstate] lua_State����
// ��ȡȫ�����ݱ�
#define G(L)	(L->l_G)


/*
** Union of all collectable objects
*/
// [@lstate]: �ɻ��ն���union from:
// GCheader gch
// TString ts
// UData u
// Closure cl
// Table h
// Proto p
// UpVal uv
// lua_State th
union GCObject {
  GCheader gch;
  union TString ts;
  union Udata u;
  union Closure cl;
  struct Table h;
  struct Proto p;
  struct UpVal uv;
  struct lua_State th;  /* thread */
};


/* macros to convert a GCObject into a specific value */
// [@lstate] GCObject����
// �൱�� TString* GCObject.toTString()
#define rawgco2ts(o)	check_exp((o)->gch.tt == LUA_TSTRING, &((o)->ts))
// [@lstate] GCObject����
// ��� GCObject��TString��Ӧ�ĺ������ݽṹ
#define gco2ts(o)	(&rawgco2ts(o)->tsv)
// [@lstate] GCObject����
// �൱�� Udata* GCObject.toUdata()
#define rawgco2u(o)	check_exp((o)->gch.tt == LUA_TUSERDATA, &((o)->u))
// [@lstate] GCObejct����
// ��� GCObject��Udata�ĺ������ݽṹ
#define gco2u(o)	(&rawgco2u(o)->uv)
// [@lstate] GCObject����
// �൱�� Closure* GCObject.toClosure()
// �²ۣ����û�к������ݽṹ��
#define gco2cl(o)	check_exp((o)->gch.tt == LUA_TFUNCTION, &((o)->cl))
// [@lstate] GCObject����
// �൱�� Table* GCObject.toTable()
#define gco2h(o)	check_exp((o)->gch.tt == LUA_TTABLE, &((o)->h))
// [@lstate] GCObject����
// �൱�� Proto* GCObject.toProto()
#define gco2p(o)	check_exp((o)->gch.tt == LUA_TPROTO, &((o)->p))
// [@lstate] GCObject����
// �൱�� UpVal* GCObject.toUpVal()
#define gco2uv(o)	check_exp((o)->gch.tt == LUA_TUPVAL, &((o)->uv))
// [@lstate] GCObject����
// �൱�� UpVal* GCObject.toUpVal()
#define ngcotouv(o) \
	check_exp((o) == NULL || (o)->gch.tt == LUA_TUPVAL, &((o)->uv))
// [@lstate] GCObject����
// �൱�� lua_State* GCObject.toLuaState()
#define gco2th(o)	check_exp((o)->gch.tt == LUA_TTHREAD, &((o)->th))

/* macro to convert any Lua object into a GCObject */
#define obj2gco(v)	(cast(GCObject *, (v)))


LUAI_FUNC lua_State *luaE_newthread (lua_State *L);
LUAI_FUNC void luaE_freethread (lua_State *L, lua_State *L1);

#endif

