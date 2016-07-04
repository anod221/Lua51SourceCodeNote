/*
** $Id: lobject.h,v 2.20 2006/01/18 11:37:34 roberto Exp $
** Type definitions for Lua objects
** 基本的Lua内置对象类型
** See Copyright Notice in lua.h
*/


#ifndef lobject_h
#define lobject_h


#include <stdarg.h>


#include "llimits.h"
#include "lua.h"


/* tags for values visible from Lua */
#define LAST_TAG	LUA_TTHREAD			// [@lobject]: 8，指向最后一种内置类型枚举值

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
// [@lobject]: 可回收对象，union from:
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
// [@lobject]: 可回收的“实例”包含的回收相关头部
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
// [@lobject]: 这个类型只是把CommonHeader作为union
// 中的一个子项目放到GCObject作为字节对齐参照用的。代码
// 中其实根本没用到它
GCheader;




/*
** Union of all Lua values
*/
typedef union {
  GCObject *gc;		//lua内置对象的引用
  void *p;			//c指针，自定义目标类型
  lua_Number n;		//numberValue
  int b;			//boolValue
} 
// [@lobject]: 一个wrapper，封装的是lua中内置对象实例
// 保存在 c 里面的真正的值。
// b: 布尔值
// n: 数字值
// p: c指针，自定义目标类型
// gc: lua内置对象的引用
Value;


/*
** Tagged Values
*/
#define TValuefields	Value value; int tt

typedef struct lua_TValue {
  TValuefields;
} 
// [@lobject]: lua内置对象类
// value: 对应c的值
// tt: 内置对象类型
TValue;


/* Macros to test type */
// [@lobject]: lua_TValue/TValue方法
// 相当于lua代码：
// function(o) 
//     return type(o) == "nil" 
// end
#define ttisnil(o)	(ttype(o) == LUA_TNIL)
// [@lobject]: lua_TValue/TValue方法
// 相当于lua代码：
// function(o) 
//     return type(o) == "number" 
// end
#define ttisnumber(o)	(ttype(o) == LUA_TNUMBER)
// [@lobject]: lua_TValue/TValue方法
// 相当于lua代码：
// function(o) 
//     return type(o) == "string" 
// end
#define ttisstring(o)	(ttype(o) == LUA_TSTRING)
// [@lobject]: lua_TValue/TValue方法
// 相当于lua代码：
// function(o) 
//     return type(o) == "table" 
// end
#define ttistable(o)	(ttype(o) == LUA_TTABLE)
// [@lobject]: lua_TValue/TValue方法
// 相当于lua代码：
// function(o) 
//     return type(o) == "function" 
// end
#define ttisfunction(o)	(ttype(o) == LUA_TFUNCTION)
// [@lobject]: lua_TValue/TValue方法
// 相当于lua代码：
// function(o) 
//     return type(o) == "boolean" 
// end
#define ttisboolean(o)	(ttype(o) == LUA_TBOOLEAN)
// [@lobject]: lua_TValue/TValue方法
// 相当于lua代码：
// function(o) 
//     return type(o) == "userdata" 
// end
#define ttisuserdata(o)	(ttype(o) == LUA_TUSERDATA)
// [@lobject]: lua_TValue/TValue方法
// 检查某个lua对象是否是一个thread类型的对象
#define ttisthread(o)	(ttype(o) == LUA_TTHREAD)
// [@lobject]: lua_TValue/TValue方法
// 检查某个lua对象是否是一个lightuserdata类型的对象
#define ttislightuserdata(o)	(ttype(o) == LUA_TLIGHTUSERDATA)

/* Macros to access values */
// [@lobject]: lua_TValue/TValue方法
// 获取某个lua_TValue实例o的类型字段
#define ttype(o)	((o)->tt)
// [@lobject]: lua_TValue/TValue方法
// 获取某个lua_TValue实例o的所封装的GCObject字段
#define gcvalue(o)	check_exp(iscollectable(o), (o)->value.gc)
// [@lobject]: lua_TValue/TValue方法
// 获取某个lua_TValue实例o所封装的c类型自定义指针字段
#define pvalue(o)	check_exp(ttislightuserdata(o), (o)->value.p)
// [@lobject]: lua_TValue/TValue方法
// 获取某个lua_TValue实例o所封装的c类型“数字”字段
#define nvalue(o)	check_exp(ttisnumber(o), (o)->value.n)
// [@lobject]: lua_TValue/TValue方法
// 获取某个lua_TValue实例o所引用的“lua字符串”对象
#define rawtsvalue(o)	check_exp(ttisstring(o), &(o)->value.gc->ts)
// [@lobject]: lua_TValue/TValue方法
// 获取某个lua_TValue实例o所引用的“lua字符串对象”对应的字符串数据结构
#define tsvalue(o)	(&rawtsvalue(o)->tsv)
// [@lobject]: lua_TValue/TValue方法
// 获取某个lua_TValue实例o所引用的UpVal对象
#define rawuvalue(o)	check_exp(ttisuserdata(o), &(o)->value.gc->u)
// [@lobject]: lua_TValue/TValue方法
// TODO
#define uvalue(o)	(&rawuvalue(o)->uv)
// [@lobject]: lua_TValue/TValue方法
// 获取某个lua_TValue实例o所引用的Closure对象
#define clvalue(o)	check_exp(ttisfunction(o), &(o)->value.gc->cl)
// [@lobject]: lua_TValue/TValue方法
// 获取某个lua_TValue实例o所引用的Table对象
#define hvalue(o)	check_exp(ttistable(o), &(o)->value.gc->h)
// [@lobject]: lua_TValue/TValue方法
// 获取某个lua_TValue实例o所封装的c类型的布尔字段
#define bvalue(o)	check_exp(ttisboolean(o), (o)->value.b)
// [@lobject]: lua_TValue/TValue方法
// 获取某个lua_TValue实例o所引用的lua_State对象（协程）
#define thvalue(o)	check_exp(ttisthread(o), &(o)->value.gc->th)

// [@lobject]: lua_TValue/TValue方法
// 对某个lua_TValue进行布尔测试。只有nil/false返回true，否则返回false
#define l_isfalse(o)	(ttisnil(o) || (ttisboolean(o) && bvalue(o) == 0))

/*
** for internal debug only
*/
// [@lobject]: lua_TValue/TValue方法，debug assert用，可以无视
#define checkconsistency(obj) \
  lua_assert(!iscollectable(obj) || (ttype(obj) == (obj)->value.gc->gch.tt))

// [@lobject]: lua_TValue/TValue方法，debug assert用，可以无视
#define checkliveness(g,obj) \
  lua_assert(!iscollectable(obj) || \
  ((ttype(obj) == (obj)->value.gc->gch.tt) && !isdead(g, (obj)->value.gc)))


/* Macros to set values */
// [@lobject]: lua_TValue/TValue方法
// 设置某个lua_TValue对象编程nil
#define setnilvalue(obj) ((obj)->tt=LUA_TNIL)

// [@lobject]: lua_TValue/TValue方法
// 设置某个lua_TValue为数字类型
// 参数：
// obj 指向lua_TValue的指针
// x 一个立即数
#define setnvalue(obj,x) \
  { TValue *i_o=(obj); i_o->value.n=(x); i_o->tt=LUA_TNUMBER; }

// [@lobject]: lua_TValue/TValue方法
// 设置某个lua_TValue为c自定义指针类型
// 参数：
// obj 指向lua_TValue的指针
// x 一个c的指针对象
#define setpvalue(obj,x) \
  { TValue *i_o=(obj); i_o->value.p=(x); i_o->tt=LUA_TLIGHTUSERDATA; }

// [@lobject]: lua_TValue/TValue方法
// 设置某个lua_TValue为布尔类型
// 参数：
// obj 指向lua_TValue的指针
// x 0/1表示逻辑假/逻辑真
#define setbvalue(obj,x) \
  { TValue *i_o=(obj); i_o->value.b=(x); i_o->tt=LUA_TBOOLEAN; }

// [@lobject]: lua_TValue/TValue方法
// 设置某个lua_TValue为string类型
// 参数：
// L lua_State对象
// obj 指向lua_TValue的指针
// x 指向TString的指针
#define setsvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TSTRING; \
    checkliveness(G(L),i_o); }

// [@lobject]: lua_TValue/TValue方法
// 设置某个lua_TValue为UpVal类型
// 参数：
// L lua_State对象
// obj 指向lua_TValue的指针
// x 指向UpVal的指针
#define setuvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TUSERDATA; \
    checkliveness(G(L),i_o); }

// [@lobject]: lua_TValue/TValue方法
// 设置某个lua_TValue对象为thread类型
// 参数:
// L lua_State对象
// obj 指向lua_TValue的指针
// x 指向协程lua_State对象的指针
#define setthvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TTHREAD; \
    checkliveness(G(L),i_o); }

// [@lobject]: lua_TValue/TValue方法
// 设置lua_TValue对象为Closure类型
// 参数：
// L lua_State对象
// obj 指向lua_TValue的指针
// x 指向Closure的指针
#define setclvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TFUNCTION; \
    checkliveness(G(L),i_o); }

// [@lobject]: lua_TValue/TValue方法
// 设置某个lua_TValue对象为Table类型
// 参数：
// L lua_State对象
// obj 指向lua_TValue的指针
// x 指向Table的指针
#define sethvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TTABLE; \
    checkliveness(G(L),i_o); }

// [@lobject]: lua_TValue/TValue方法
// TODO
// 参数：
// L lua_State对象
// obj 指向lua_TValue的指针
// x 指向Proto的指针
#define setptvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TPROTO; \
    checkliveness(G(L),i_o); }



// [@lobject]: lua_TValue/TValue方法
// 将某个lua_TValue设置为另一个lua_TValue实例的引用
// 参数：
// L lua_State对象
// obj1 左值的lua_TValue对象，将会设置成为引用
// obj2 右值的lua_TValue对象，作为范本设置到obj1上去
#define setobj(L,obj1,obj2) \
  { const TValue *o2=(obj2); TValue *o1=(obj1); \
    o1->value = o2->value; o1->tt=o2->tt; \
    checkliveness(G(L),o1); }


/*
** different types of sets, according to destination
*/

/* from stack to (same) stack */

// [@lobject]: lua_TValue/TValue方法
// 将某个lua_TValue设置为另一个lua_TValue实例的引用
// 参数：
// L lua_State对象
// obj1 左值的lua_TValue对象，将会设置成为引用
// obj2 右值的lua_TValue对象，作为范本设置到obj1上去
#define setobjs2s	setobj
/* to stack (not from same stack) */

// [@lobject]: lua_TValue/TValue方法
// 将某个lua_TValue设置为另一个lua_TValue实例的引用
// 参数：
// L lua_State对象
// obj1 左值的lua_TValue对象，将会设置成为引用
// obj2 右值的lua_TValue对象，作为范本设置到obj1上去
#define setobj2s	setobj

// [@lobject]: lua_TValue/TValue方法
// 设置某个lua_TValue为string类型
// 参数：
// L lua_State对象
// obj 指向lua_TValue的指针
// x 指向TString的指针
#define setsvalue2s	setsvalue

// [@lobject]: lua_TValue/TValue方法
// 设置某个lua_TValue对象为Table类型
// 参数：
// L lua_State对象
// obj 指向lua_TValue的指针
// x 指向Table的指针
#define sethvalue2s	sethvalue

// [@lobject]: lua_TValue/TValue方法
// TODO
// 参数：
// L lua_State对象
// obj 指向lua_TValue的指针
// x 指向Proto的指针
#define setptvalue2s	setptvalue
/* from table to same table */

// [@lobject]: lua_TValue/TValue方法
// 将某个lua_TValue设置为另一个lua_TValue实例的引用
// 参数：
// L lua_State对象
// obj1 左值的lua_TValue对象，将会设置成为引用
// obj2 右值的lua_TValue对象，作为范本设置到obj1上去
#define setobjt2t	setobj
/* to table */

// [@lobject]: lua_TValue/TValue方法
// 将某个lua_TValue设置为另一个lua_TValue实例的引用
// 参数：
// L lua_State对象
// obj1 左值的lua_TValue对象，将会设置成为引用
// obj2 右值的lua_TValue对象，作为范本设置到obj1上去
#define setobj2t	setobj
/* to new object */

// [@lobject]: lua_TValue/TValue方法
// 将某个lua_TValue设置为另一个lua_TValue实例的引用
// 参数：
// L lua_State对象
// obj1 左值的lua_TValue对象，将会设置成为引用
// obj2 右值的lua_TValue对象，作为范本设置到obj1上去
#define setobj2n	setobj

// [@lobject]: lua_TValue/TValue方法
// 设置某个lua_TValue为string类型
// 参数：
// L lua_State对象
// obj 指向lua_TValue的指针
// x 指向TString的指针
#define setsvalue2n	setsvalue

// [@lobject]: lua_TValue/TValue方法
// 设置某个lua_TValue实例o的类型字段
// 参数：
// obj 指向lua_TValue的指针
// tt 要设置上去的lua类型
#define setttype(obj, tt) (ttype(obj) = (tt))

// [@lobject]: lua_TValue/TValue方法
// 检查某个lua_TValue实例o是否可以被虚拟机进行垃圾回收
#define iscollectable(o)	(ttype(o) >= LUA_TSTRING)


// [@lobject]: 声明一个lua对象指针为StkId
// StkId 表示 Stack Index
// 这个类型代码中大量用到，主要是从lua/C交互栈上取得
// 的数据，都会转换成为StkId，也就是lua_TValue*
typedef TValue *StkId;  



// [@lobject]: lua string类型
// 主要字段tsv，tsv包含有：
// hash 字符串hash值
// len 字符串长度
// reserved 这个是冗余，记录的是这个字符串在全局变量luaX_tokens@llex的索引
// ooooooooooooooooooooooooooooooooooooooo
// 除了tsv以外，TString还有一个奇特的隐藏字段
// <c_str>，紧紧跟随在tsv后面，// 保存着raw c
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
// [@lobject]: lua string类型
// 主要字段tsv，tsv包含有：
// hash 字符串hash值
// len 字符串长度
// reserved ??<TODO: 尚不了解用途>
// ooooooooooooooooooooooooooooooooooooooo
// 除了tsv以外，TString还有一个奇特的隐藏字段
// <c_str>，紧紧跟随在tsv后面，// 保存着raw c
// string(const char*)
TString;

// [@lobject]: TString方法
// 从TString中获取到c字符串地址
// PS: ts+1是因为vm中，字符串的保存单元就是前面
// 一个TString结构后面紧跟一个const char*。
// 详细内容需要查看lstring.c
#define getstr(ts)	cast(const char *, (ts) + 1)
// [@lobject]: lua_TValue/TValue方法
// 从lua_TValue的实例o中获得c字符串的地址
#define svalue(o)       getstr(tsvalue(o))


// [@lobject]: Udata类型，对应于lua的userdata
// 内心吐槽：这可怜的类型，连一个成员方法都没有！
// 主要字段是uv，提供：
// metatable 对应于userdata的元表
// env 对应与userdata所在的环境表（TODO: 还不知道干嘛用）
// len TODO: 还不知道干吗用
typedef union Udata {
  L_Umaxalign dummy;  /* ensures maximum alignment for `local' udata */
  struct {
    CommonHeader;
    struct Table *metatable;
    struct Table *env;
    size_t len;
  } uv;
} 
// [@lobject]: Udata类型，对应于lua的userdata
// 内心吐槽：这可怜的类型，连一个成员方法都没有！
// 主要字段是uv，提供：
// metatable 对应于userdata的元表
// env 对应与userdata所在的环境表（TODO: 还不知道干嘛用）
// len alloc出来的内存大小
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
  lu_byte numparams;	//记录function(a,b,c,...)中，有名字的变量的数量
  lu_byte is_vararg;
  lu_byte maxstacksize;
} 
// [@lobject]: 对应一个lua function的数据结构
// 最复杂的GCObject,有效字段包括
// k+sizek 函数用到的常量，组成一个数组
// code+sizecode 字节码指令序列，组成一个数组
// p+sizep 一个数组，记录在当前函数代码里面定义的嵌套函数的引用
// lineinfo+sizelineinfo 一个数组，下标对应code的下标，记录对应的每一个instruction在原文件中的行号
// locvars+sizelocvars 用到的局部变量
// upvalues+sizeupvalues 用到的up value列表
// source 函数的源代码
// linedefined
// lastlinedefined
// gclist
// nups
// numparams
// is_vararg
// maxstacksize 
Proto;


/* masks for new-style vararg */
#define VARARG_HASARG		1	// [@lobject]: 用于为Proto::is_vararg赋值
#define VARARG_ISVARARG		2	// [@lobject]: 用于为Proto::is_vararg赋值
#define VARARG_NEEDSARG		4	// [@lobject]: 用于为Proto::is_vararg赋值


typedef struct LocVar {
  TString *varname;
  int startpc;  /* first point where variable is active */
  int endpc;    /* first point where variable is dead */
} 
// [@lobject]: 局部变量结构
// varname lua字符串，保存变量名
// startpc 变量首次使用的指令位置
// endpc 变量最后一次使用的指令位置
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
// v 保存一个lua_TValue的指针，指向调用栈往上的参数的指针TValue*
// u 这是一个union。含有
// u.value <TValue>，指向真正的UpVal对象
// u.l <DLinkList<UpValPtr>> UpVal的双端链表
UpVal;


/*
** Closures
*/

// [@lobject]: 公用闭包属性
// isC 是否封装的C函数
// nupvalues 引用的up-value的数量
// gclist TODO
// env 闭包指令用到的env表
#define ClosureHeader \
	CommonHeader; lu_byte isC; lu_byte nupvalues; GCObject *gclist; \
	struct Table *env

typedef struct CClosure {
  ClosureHeader;
  lua_CFunction f;
  TValue upvalue[1];
} 
// [@lobject]: C类型闭包
// isC 是否封装的C函数
// nupvalues 引用的up-value的数量
// gclist TODO
// env 闭包指令用到的env表
// f 真正用到的C函数int (*f)(lua_State *L)
// upvalue 这是一个truck。总之，upvalue是一个TValue[nupvalues]，是实体TValue数组
CClosure;


typedef struct LClosure {
  ClosureHeader;
  struct Proto *p;
  UpVal *upvals[1]; //这也是一个trick，总之，可以认为upvals是一个UpValPtr[nupvalues]，也就是指针数组
} 
// [@lobject]: lua类型闭包
// isC 是否封装的C函数
// nupvalues 引用的up-value的数量
// gclist TODO
// env 闭包指令用到的env表
// p 真正用到的lua函数对象，也就是Proto对象
// upvals 这也是一个trick，总之，可以认为upvals是一个UpValPtr[nupvalues]，也就是指针数组
LClosure;


typedef union Closure {
  CClosure c;
  LClosure l;
} Closure;

// [@lobject]: Closure方法
// 检查Closure实例o是否是封装c的函数
#define iscfunction(o)	(ttype(o) == LUA_TFUNCTION && clvalue(o)->c.isC)
// [@lobject]: Closure方法
// 检查Closure实例o是否是一个纯lua函数
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
// [@lobject]: 定义Table对象的key
// tvk 用来作为Table的key的lua_TValue对象
// nk 虽然是一个union，但是其实内存布局是 tvk -> next，实际上只有一个Node*用来
// 连接到下一个Node。
TKey;


typedef struct Node {
  TValue i_val;
  TKey i_key;
} 
// [@lobject]: 定义Table对象的一个key-value对
// i_key 一个TKey对象，保存key
// i_val 一个lua_TValue对象，保存value
Node;


// [@lobject]: 定义一个lua代码中用到的Table对象
// flag TODO
// lsizenode table容量值取2为底的对数值
// metatable 对应的元彪
// array+sizearray 一个TValue数组，保存着数组的部分
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
// [@lobject]: 定义一个lua代码中用到的Table对象
// flag 一个逐位标识对应于元表的某个tag method是否存在的u32，1表示不存在，0表示不一定
// lsizenode table容量值取2为底的对数值
// metatable 对应的元表
// array+sizearray 一个TValue数组，保存着数组的部分
// node 保存一个数组，数组元素是Node，table的k-v部分 <Node[]>
// lastfree node里面，最后一个空位位置 <Node*>
// gclist TODO
Table;


/*
** `module' operation for hashing (size is always a power of 2)
*/

// [@lobject]: 辅助函数
// 功能：截取 s 在size下的数值。相当于 s % size
// 约束条件：size必须是2的幂
#define lmod(s,size) \
	(check_exp((size&(size-1))==0, (cast(int, (s) & ((size)-1)))))

// [@lobject]: 辅助函数
// 功能：取 2 的x次幂
#define twoto(x)	(1<<(x))
// [@lobject]: Table方法
// 功能: 得到Table的node容量
#define sizenode(t)	(twoto((t)->lsizenode))

// [@lobject]: 全局nil常量
#define luaO_nilobject		(&luaO_nilobject_)

LUAI_DATA const TValue luaO_nilobject_;

#define ceillog2(x)	(luaO_log2((x)-1) + 1)

// [@lobject]: 辅助函数
// 功能：求出 math.floor( log2(x) )
LUAI_FUNC int luaO_log2 (unsigned int x);

// [@lobject]: 将int转换成为fb格式
// fb格式：8b := XXXXXYYY
// 计算公式：(YYY|0x08) * 2^(XXXXX-1)
// 解释：这个格式转换实际上是有损的。转换
// 后的结果k将略大于x，k是一个偶数，并且
// k-x的增加趋势比较平缓
LUAI_FUNC int luaO_int2fb (unsigned int x);

// [@lobject]: fb转换成int
// fb格式：8b := XXXXXYYY
// 计算公式：(YYY|0x08) * 2^(XXXXX-1)
LUAI_FUNC int luaO_fb2int (int x);

// [@lobject]: lua_TValue/TValue方法
// 比较两个TValue是否在数学意义上相等
LUAI_FUNC int luaO_rawequalObj (const TValue *t1, const TValue *t2);

// [@lobject]: 辅助方法
// 将一个符合c规范的数字字符串读入到一个浮点数变量中
// s 符合c规范的数字字符串，可以是整数或者浮点数或者16进制整数
// result 解析s得到的数字要写入的目标变量地址
// 返回：1 成功 0失败
LUAI_FUNC int luaO_str2d (const char *s, lua_Number *result);


// [@lobject]: 用valist的方式，往lua虚拟机栈顶压入格式化过的字符串
LUAI_FUNC const char *luaO_pushvfstring (lua_State *L, const char *fmt,
                                                       va_list argp);

// [@lobject]: 用可变参数的方式，往lua虚拟机栈顶压入格式化过的字符串
LUAI_FUNC const char *luaO_pushfstring (lua_State *L, const char *fmt, ...);

// [@lobject]: 辅助方法
// 根据source设置out，用于compile某个chunk的时候
// 设置这个chunk的来源（文件，控制台或者字符串）
LUAI_FUNC void luaO_chunkid (char *out, const char *source, size_t len);


#endif

