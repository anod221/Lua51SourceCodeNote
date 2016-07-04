/*
** $Id: lstate.h,v 2.24 2006/02/06 18:27:59 roberto Exp $
** Global State
** 虚拟机数据结构定义
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
// [@lstate] lua_State方法
// 返回某个VM对象的全局表_G
#define gt(L)	(&L->l_gt)

/* registry */
// [@lstate] lua_State方法
// 返回lua_State对象的注册表 TValue*
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
// [@lstate] 全局字符串表格结构
// 包含字段:
// hash 一个二维数组，数组的每个元素是一个GCObject
// nuse 表格已经使用的位置数量
// size 表格总容量
stringtable;


/*
** informations about a call
*/
typedef struct CallInfo {
  StkId base;  // [@lstate] 运行时，栈底指针 <lua_TValue*>
  StkId func;  // [@lstate] CallInfo对应的函数的Closure对象 <lua_TValue*>
  StkId	top;  // [@lstate] 运行时，栈顶指针(和L->top不同，L->top指向已使用参数尾部，CI->top指向理论可使用的最高) <lua_TValue*>
  const Instruction *savedpc;
  int nresults;  /* expected number of results from this function */
  int tailcalls;  /* number of tail calls lost under this entry */
} 
// [@lstate] lua_State中用来保存当前函数的结构
// 包含字段:
// func CallInfo对应的函数的Closure对象 <lua_TValue*>
// base 运行时，栈底指针 <lua_TValue*>
// top 运行时，栈顶指针(和L->top不同，L->top指向已使用参数尾部，CI->top指向理论可使用的最高) <lua_TValue*>
// savedpc 运行时VM当前指令位置 <Instruction*>
// nresult 函数返回值个数 <int>
// tailcalls TODO <int>
CallInfo;


// [@lstate] lua_State方法
// 获取当前正在执行的函数(lua_TValue)对象
#define curr_func(L)	(clvalue(L->ci->func))
// [@lstate] CallInfo方法
// 获取成员变量func的getter
#define ci_func(ci)	(clvalue((ci)->func))
// [@lstate] CallInfo方法
// 当前函数是否是lua函数
#define f_isLua(ci)	(!ci_func(ci)->c.isC)
// [@lstate] CallInfo方法
// 当前函数是否是lua函数
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
// [@lstate] vm运行时全局数据表
// strt 字符串表<stringtable>
// frealloc 重分配函数 <c函数>
// ud frealloc的某个参数用的自定义数据 <void*>
// currentwhite TODO<应该是GC用的东西> <int>
// gcstate 当前GC进行的状态 <int>
// sweepstrgc TODO<应该是GC用的东西> <int>
// rootgc TODO<应该是GC用的东西> <GCObject*>
// sweepgc TODO<应该是GC用的东西> <GCObject*>
// gray TODO<应该是GC用的东西> <GCObject*>
// grayagain TODO<应该是GC用的东西> <GCObject*>
// weak 标记为dirty的全部弱表 <GCObject*>
// tmudata TODO<应该是GC用的东西> <GCObject*>
// buff 用于字符串concat用的buffer，在此用作cache<MBuffer>
// GCthreshold 触发GC的内存阈值<int>
// totalbytes 总共“申请”的内存大小<int>
// estmimate “已使用”的内存大小，是个估计值<int>
// gcdept TODO<应该是GC用的东西> <int>
// gcpause TODO<应该是GC用的东西> <int>
// gcstepmul TODO<应该是GC用的东西> <int>
// panic 处理uncatching exception的c函数 <c函数>
// l_registry 全局注册表<TValue>
// mainthread VM启动的主协程<lua_State*>
// uvhead TODO <UpVal>
// mt 为内置类型配置的metatable <Table*[]>
// tmname metatable中的保留key列表 <TString*[]>
global_State;


/*
** `per thread' state
*/
// [@lstate] 一个虚拟机线程调度集合，可以被GC
// 1 status 当前线程状态<int>
// 1 l_G 全局表指针 <global_State*>
// 1 savedpc 运行时VM当前指令位置 <Instruction*>
// 1 nCcalls 记录公共c函数luaD_call被调用了几次，达到一定次数报告溢出 <int>
// 1 l_gt _G表 <TValue>
// 2 ====== 运行时调用栈 ======
// 2 ci 当前函数调用信息 <CallInfo*>
// 2 base_ci, end_ci, size_ci: CallInfo数组
// 2 组成一个数组，base_ci是列表首元素，end_ci是列表尾元素，size_ci是列表长度
// 2 通过ci得到当前栈，可以往前找ci的caller，caller的caller等
// 3 ====== 运行时参数栈 ======
// 3 base 当前函数运行时参数栈首指针 <lua_TValue*>
// 3 top 当前函数运行时参数栈尾指针 <lua_TValue*>
// 3 例如lua代码： f(1, '2', _3) local a, b, c end
// 3 stack-index    rev-stack-index(CClosure only)     value
// 3  1 <- base      -3                                  1
// 3  2              -2                                  '2'
// 3  3              -1 <- top                           _3
// 3  4              index(a) = 4                        nil
// 3  5              index(b) = 5                        nil
// 3  6              index(c) = 6                        nil
// 4 ====== 运行时参数栈容器 ======
// 4 stack stack_last stacksize: TValue数组
// 4 stack是数组首元素，stack_last是数组尾元素，stacisize是数组长度
// 4 参数栈是所有函数公用的容器，所以是可以越界访问别的函数的参数的。（和PC一致）
// 5 ====== 运行时钩子信息 =====
// 5 hookmask 当前设置的hook状态 <int>
// 5 allowhook hook启用的开关。这个变量是必要的，不然在hook的函数中怎么禁用hook? <int>
// 5 basehookcount hookcount 实现“每执行count条指令执行一次hook”,前者是常数，后者是记录值 <int>
// 5 hook c写的hook函数 <c函数>
// env TODO <TValue>
// openupval upvalue链表，以UpVal::v降序排列，插入时调整顺序 <GCObject*>
// gclist TODO <GCObject*>
// errorJmp TODO <lua_longjmp*>
// errfunc 错误函数处理 <ptrdiff_t>
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
  unsigned short nCcalls;  // [@lstate] 记录公共c函数luaD_call被调用了几次，达到一定次数报告溢出
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

// [@lstate] lua_State方法
// 获取全局数据表
#define G(L)	(L->l_G)


/*
** Union of all collectable objects
*/
// [@lstate]: 可回收对象，union from:
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
// [@lstate] GCObject方法
// 相当于 TString* GCObject.toTString()
#define rawgco2ts(o)	check_exp((o)->gch.tt == LUA_TSTRING, &((o)->ts))
// [@lstate] GCObject方法
// 获得 GCObject中TString对应的核心数据结构
#define gco2ts(o)	(&rawgco2ts(o)->tsv)
// [@lstate] GCObject方法
// 相当于 Udata* GCObject.toUdata()
#define rawgco2u(o)	check_exp((o)->gch.tt == LUA_TUSERDATA, &((o)->u))
// [@lstate] GCObejct方法
// 获得 GCObject中Udata的核心数据结构
#define gco2u(o)	(&rawgco2u(o)->uv)
// [@lstate] GCObject方法
// 相当于 Closure* GCObject.toClosure()
// 吐槽：这回没有核心数据结构了
#define gco2cl(o)	check_exp((o)->gch.tt == LUA_TFUNCTION, &((o)->cl))
// [@lstate] GCObject方法
// 相当于 Table* GCObject.toTable()
#define gco2h(o)	check_exp((o)->gch.tt == LUA_TTABLE, &((o)->h))
// [@lstate] GCObject方法
// 相当于 Proto* GCObject.toProto()
#define gco2p(o)	check_exp((o)->gch.tt == LUA_TPROTO, &((o)->p))
// [@lstate] GCObject方法
// 相当于 UpVal* GCObject.toUpVal()
#define gco2uv(o)	check_exp((o)->gch.tt == LUA_TUPVAL, &((o)->uv))
// [@lstate] GCObject方法
// 相当于 UpVal* GCObject.toUpVal()
#define ngcotouv(o) \
	check_exp((o) == NULL || (o)->gch.tt == LUA_TUPVAL, &((o)->uv))
// [@lstate] GCObject方法
// 相当于 lua_State* GCObject.toLuaState()
#define gco2th(o)	check_exp((o)->gch.tt == LUA_TTHREAD, &((o)->th))

/* macro to convert any Lua object into a GCObject */
#define obj2gco(v)	(cast(GCObject *, (v)))


LUAI_FUNC lua_State *luaE_newthread (lua_State *L);
LUAI_FUNC void luaE_freethread (lua_State *L, lua_State *L1);

#endif

