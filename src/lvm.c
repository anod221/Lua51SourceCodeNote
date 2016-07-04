/*
** $Id: lvm.c,v 2.62 2006/01/23 19:51:43 roberto Exp $
** Lua virtual machine
** See Copyright Notice in lua.h
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define lvm_c
#define LUA_CORE

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lvm.h"



/* limit for table tag-method chains (to avoid loops) */
#define MAXTAGLOOP	100

// [@lvm] 如果obj是lua的数字，返回obj；如果obj是lua的字符串，将n设为obj对应的数字值
// 相当于lua代码：
// function luaV_tonumber(&obj, &n)
//   if type(obj)=="number" then return obj end
//   n = tonumber(obj)
//   return n
// end
const TValue *luaV_tonumber (const TValue *obj, TValue *n) {
  lua_Number num;
  if (ttisnumber(obj)) return obj;
  if (ttisstring(obj) && luaO_str2d(svalue(obj), &num)) {
    setnvalue(n, num);
    return n;
  }
  else
    return NULL;
}

// [@lvm] 如果obj对应于lua的数字，则把obj直接转成str，并返回真，否则什么都不做返回假
int luaV_tostring (lua_State *L, StkId obj) {
  if (!ttisnumber(obj))
    return 0;
  else {
    char s[LUAI_MAXNUMBER2STR];
    lua_Number n = nvalue(obj);
    lua_number2str(s, n);
    setsvalue2s(L, obj, luaS_new(L, s));
    return 1;
  }
}

// [@lvm] 检查并调用hook函数
static void traceexec (lua_State *L, const Instruction *pc) {
  lu_byte mask = L->hookmask;
  const Instruction *oldpc = L->savedpc;
  L->savedpc = pc;
  if (mask > LUA_MASKLINE) {  /* instruction-hook set? */
    if (L->hookcount == 0) {
      resethookcount(L);
      luaD_callhook(L, LUA_HOOKCOUNT, -1);
    }
  }
  if (mask & LUA_MASKLINE) {
    Proto *p = ci_func(L->ci)->l.p;
    int npc = pcRel(pc, p);			//now pc
    int newline = getline(p, npc);
    /* call linehook when enter a new function, when jump back (loop),
       or when enter a new line */
	// 看仔细了：`when jump back`，对应pc <= oldpc。这也算newline，所以导致
	// LDT会在同一行停呀停！
    if (npc == 0 || pc <= oldpc || newline != getline(p, pcRel(oldpc, p)))
      luaD_callhook(L, LUA_HOOKLINE, newline);
  }
}

// [@lvm] 调用lua的函数f，参数1，参数2是p1,p2，f的返回值写入res
static void callTMres (lua_State *L, StkId res, const TValue *f,
                        const TValue *p1, const TValue *p2) {
  ptrdiff_t result = savestack(L, res);
  setobj2s(L, L->top, f);  /* push function */
  setobj2s(L, L->top+1, p1);  /* 1st argument */
  setobj2s(L, L->top+2, p2);  /* 2nd argument */
  luaD_checkstack(L, 3);
  L->top += 3;
  luaD_call(L, L->top - 3, 1);
  res = restorestack(L, result);
  L->top--;
  setobjs2s(L, res, L->top);
}


// [@lvm] 调用3参数元方法f，参数分别是p1, p2, p3，f不含有返回值
static void callTM (lua_State *L, const TValue *f, const TValue *p1,
                    const TValue *p2, const TValue *p3) {
  setobj2s(L, L->top, f);  /* push function */
  setobj2s(L, L->top+1, p1);  /* 1st argument */
  setobj2s(L, L->top+2, p2);  /* 2nd argument */
  setobj2s(L, L->top+3, p3);  /* 3th argument */
  luaD_checkstack(L, 4);
  L->top += 4;
  luaD_call(L, L->top - 4, 0);
}

// [@lvm] 从Table实例t获取key对应的value到val中
// 相当于lua代码： val = t[key]
void luaV_gettable (lua_State *L, const TValue *t, TValue *key, StkId val) {
  int loop;
  for (loop = 0; loop < MAXTAGLOOP; loop++) {
    const TValue *tm;
    if (ttistable(t)) {  /* `t' is a table? */
      Table *h = hvalue(t);
      const TValue *res = luaH_get(h, key); /* do a primitive get */
      if (!ttisnil(res) ||  /* result is no nil? */
          (tm = fasttm(L, h->metatable, TM_INDEX)) == NULL) { /* or no TM? */
        setobj2s(L, val, res);
        return;
      }
      /* else will try the tag method */
    }
    else if (ttisnil(tm = luaT_gettmbyobj(L, t, TM_INDEX)))
      luaG_typeerror(L, t, "index");
    if (ttisfunction(tm)) {
      callTMres(L, val, tm, t, key);
      return;
    }
    t = tm;  /* else repeat with `tm' */ 
  }
  luaG_runerror(L, "loop in gettable");
}

// [@lvm] 从Table实例t中设置key的对应value为val
// 相当于lua代码：t[key] = val
void luaV_settable (lua_State *L, const TValue *t, TValue *key, StkId val) {
  int loop;
  for (loop = 0; loop < MAXTAGLOOP; loop++) {
    const TValue *tm;
    if (ttistable(t)) {  /* `t' is a table? */
      Table *h = hvalue(t);
      TValue *oldval = luaH_set(L, h, key); /* do a primitive set */
      if (!ttisnil(oldval) ||  /* result is no nil? */
          (tm = fasttm(L, h->metatable, TM_NEWINDEX)) == NULL) { /* or no TM? */
        setobj2t(L, oldval, val);
        luaC_barriert(L, h, val);
        return;
      }
      /* else will try the tag method */
    }
    else if (ttisnil(tm = luaT_gettmbyobj(L, t, TM_NEWINDEX)))
      luaG_typeerror(L, t, "index");
    if (ttisfunction(tm)) {
      callTM(L, tm, t, key, val);
      return;
    }
    t = tm;  /* else repeat with `tm' */ 
  }
  luaG_runerror(L, "loop in settable");
}

// [@lvm] 针对同一个Class，调用event对应的元方法，传入参数p1, p2，写入返回值到res
// 和call_orderTM的异同：
// 1 都是2元运算
// 2 都有返回值
// 3 binTM要求参数至少有1个含有metamethod，orderTM则要求全都含有相同的metamethod
// 4 binTM的返回值写入res，orderTM的返回值写入top
static int call_binTM (lua_State *L, const TValue *p1, const TValue *p2,
                       StkId res, TMS event) {
  const TValue *tm = luaT_gettmbyobj(L, p1, event);  /* try first operand */
  if (ttisnil(tm))
    tm = luaT_gettmbyobj(L, p2, event);  /* try second operand */
  if (!ttisfunction(tm)) return 0;
  callTMres(L, res, tm, p1, p2);
  return 1;
}

// [@lvm] 从两个元表mt1和mt2取出event字段的TValue
// 注：有保证两个元表的event字段对应的值相同。不同会返回NULL
static const TValue *get_compTM (lua_State *L, Table *mt1, Table *mt2,
                                  TMS event) {
  const TValue *tm1 = fasttm(L, mt1, event);
  const TValue *tm2;
  if (tm1 == NULL) return NULL;  /* no metamethod */
  if (mt1 == mt2) return tm1;  /* same metatables => same metamethods */
  tm2 = fasttm(L, mt2, event);
  if (tm2 == NULL) return NULL;  /* no metamethod */
  if (luaO_rawequalObj(tm1, tm2))  /* same metamethods? */
    return tm1;
  return NULL;
}

// [@lvm] 检查调用p1,p2两个值的元比较方法
// 和call_orderTM的异同：
// 1 都是2元运算
// 2 都有返回值
// 3 binTM要求参数至少有1个含有metamethod，orderTM则要求全都含有相同的metamethod
// 4 binTM的返回值写入res，orderTM的返回值写入top
static int call_orderTM (lua_State *L, const TValue *p1, const TValue *p2,
                         TMS event) {
  const TValue *tm1 = luaT_gettmbyobj(L, p1, event);
  const TValue *tm2;
  if (ttisnil(tm1)) return -1;  /* no metamethod? */
  tm2 = luaT_gettmbyobj(L, p2, event);
  if (!luaO_rawequalObj(tm1, tm2))  /* different metamethods? */
    return -1;
  callTMres(L, L->top, tm1, p1, p2);
  return !l_isfalse(L->top);
}


static int l_strcmp (const TString *ls, const TString *rs) {
  const char *l = getstr(ls);
  size_t ll = ls->tsv.len;
  const char *r = getstr(rs);
  size_t lr = rs->tsv.len;
  for (;;) {
    int temp = strcoll(l, r);
    if (temp != 0) return temp;
    else {  /* strings are equal up to a `\0' */
      size_t len = strlen(l);  /* index of first `\0' in both strings */
      if (len == lr)  /* r is finished? */
        return (len == ll) ? 0 : 1;
      else if (len == ll)  /* l is finished? */
        return -1;  /* l is smaller than r (because r is not finished) */
      /* both strings longer than `len'; go on comparing (after the `\0') */
      len++;
      l += len; ll -= len; r += len; lr -= len;
    }
  }
}

// [@lvm] 比较两个lua值大小
// 如果无法比较，将会抛出异常
// if nil < nil 也会抛出异常！！
int luaV_lessthan (lua_State *L, const TValue *l, const TValue *r) {
  int res;
  if (ttype(l) != ttype(r))
    return luaG_ordererror(L, l, r);
  else if (ttisnumber(l))
    return luai_numlt(nvalue(l), nvalue(r));
  else if (ttisstring(l))
    return l_strcmp(rawtsvalue(l), rawtsvalue(r)) < 0;
  else if ((res = call_orderTM(L, l, r, TM_LT)) != -1)
    return res;
  return luaG_ordererror(L, l, r);
}


static int lessequal (lua_State *L, const TValue *l, const TValue *r) {
  int res;
  if (ttype(l) != ttype(r))
    return luaG_ordererror(L, l, r);
  else if (ttisnumber(l))
    return luai_numle(nvalue(l), nvalue(r));
  else if (ttisstring(l))
    return l_strcmp(rawtsvalue(l), rawtsvalue(r)) <= 0;
  else if ((res = call_orderTM(L, l, r, TM_LE)) != -1)  /* first try `le' */
    return res;
  else if ((res = call_orderTM(L, r, l, TM_LT)) != -1)  /* else try `lt' */
    return !res;
  return luaG_ordererror(L, l, r);
}

// [@lvm] 检查两个lua变量的值是否相等
int luaV_equalval (lua_State *L, const TValue *t1, const TValue *t2) {
  const TValue *tm;
  lua_assert(ttype(t1) == ttype(t2));
  switch (ttype(t1)) {
    case LUA_TNIL: return 1;
    case LUA_TNUMBER: return luai_numeq(nvalue(t1), nvalue(t2));
    case LUA_TBOOLEAN: return bvalue(t1) == bvalue(t2);  /* true must be 1 !! */
    case LUA_TLIGHTUSERDATA: return pvalue(t1) == pvalue(t2);
    case LUA_TUSERDATA: {
      if (uvalue(t1) == uvalue(t2)) return 1;
      tm = get_compTM(L, uvalue(t1)->metatable, uvalue(t2)->metatable,
                         TM_EQ);
      break;  /* will try TM */
    }
    case LUA_TTABLE: {
      if (hvalue(t1) == hvalue(t2)) return 1;
      tm = get_compTM(L, hvalue(t1)->metatable, hvalue(t2)->metatable, TM_EQ);
      break;  /* will try TM */
    }
    default: return gcvalue(t1) == gcvalue(t2);
  }
  if (tm == NULL) return 0;  /* no TM? */
  callTMres(L, L->top, tm, t1, t2);  /* call TM */
  return !l_isfalse(L->top);
}

// [@ltm] 对参数栈上的参数执行concat操作
// total 需要concat的参数的个数
// last concat的数组最后一个元素下标
void luaV_concat (lua_State *L, int total, int last) {
  do {
    StkId top = L->base + last + 1;
    int n = 2;  /* number of elements handled in this pass (at least 2) */
    if (!tostring(L, top-2) || !tostring(L, top-1)) {
	  // 两个参数不全都是string，所以只能通过metamethod来处理
      if (!call_binTM(L, top-2, top-1, top-2, TM_CONCAT))
        luaG_concaterror(L, top-2, top-1);
    } else if (tsvalue(top-1)->len > 0) {  /* if len=0, do nothing<优化的套路呀> */
      /* at least two string values; get as many as possible */
      size_t tl = tsvalue(top-1)->len;
      char *buffer;
      int i;
      /* collect total length */
      for (n = 1; n < total && tostring(L, top-n-1); n++) {
        size_t l = tsvalue(top-n-1)->len;
        if (l >= MAX_SIZET - tl) luaG_runerror(L, "string length overflow");
        tl += l;
      }
	  // 这里保存了要concat的string的总大小
      buffer = luaZ_openspace(L, &G(L)->buff, tl);
      tl = 0;
      for (i=n; i>0; i--) {  /* concat all strings */
        size_t l = tsvalue(top-i)->len;
        memcpy(buffer+tl, svalue(top-i), l);
        tl += l;
      }
      setsvalue2s(L, top-n, luaS_newlstr(L, buffer, tl));
    }
    total -= n-1;  /* got `n' strings to create 1 new */
    last -= n-1;
  } while (total > 1);  /* repeat until only 1 result left */
}

// [@lvm] 进行算术计算，ra是左值, rb, rc是二元操作数, op是运算符
// 注意，此函数会检查操作数的类型并强制转换为数字
static void Arith (lua_State *L, StkId ra, const TValue *rb,
                   const TValue *rc, TMS op) {
  TValue tempb, tempc;
  const TValue *b, *c;
  if ((b = luaV_tonumber(rb, &tempb)) != NULL &&
      (c = luaV_tonumber(rc, &tempc)) != NULL) {
    lua_Number nb = nvalue(b), nc = nvalue(c);
    switch (op) {
      case TM_ADD: setnvalue(ra, luai_numadd(nb, nc)); break;
      case TM_SUB: setnvalue(ra, luai_numsub(nb, nc)); break;
      case TM_MUL: setnvalue(ra, luai_nummul(nb, nc)); break;
      case TM_DIV: setnvalue(ra, luai_numdiv(nb, nc)); break;
      case TM_MOD: setnvalue(ra, luai_nummod(nb, nc)); break;
      case TM_POW: setnvalue(ra, luai_numpow(nb, nc)); break;
      case TM_UNM: setnvalue(ra, luai_numunm(nb)); break;
      default: lua_assert(0); break;
    }
  }
  else if (!call_binTM(L, rb, rc, ra, op))
    luaG_aritherror(L, rb, rc);
}



/*
** some macros for common tasks in `luaV_execute'
*/

#define runtime_check(L, c)	{ if (!(c)) break; }

// [@lvm] Instruction方法
// 获取Instruction对应参数A的StkId，也就是lua对象
// i Instruction*
// 返回：TValue*
// base 这个是c函数里面赋值的局部变量，对应L->base
// GETARG_A 指令的参数A保存的是lua变量在栈上的索引
#define RA(i)	(base+GETARG_A(i))
/* to be used after possible stack reallocation */
// [@lvm] Instruction方法
// 获取Instruction对应参数B的StkId，也就是lua对象
// i Instruction*
// 返回：TValue*
#define RB(i)	check_exp(getBMode(GET_OPCODE(i)) == OpArgR, base+GETARG_B(i))
// [@lvm] Instruction方法
// 获取Instruction对应参数C的StkId，也就是lua对象
// i Instruction*
// 返回：TValue*
#define RC(i)	check_exp(getCMode(GET_OPCODE(i)) == OpArgR, base+GETARG_C(i))
// [@lvm] Instruction方法
// 获取Instruction对应参数B的StkId(Proto::base+index)或者常量(Proto::k)
// i Instruction*
// 返回：TValue*
// 吐槽：知道inline的好处了吧？看看下面轻描淡写的调用RKB(i)，实际上多么复杂！
#define RKB(i)	check_exp(getBMode(GET_OPCODE(i)) == OpArgK, \
	ISK(GETARG_B(i)) ? k+INDEXK(GETARG_B(i)) : base+GETARG_B(i))

// [@lvm] Instruction方法
// 获取Instruction对应参数C的StkId(Proto::base+index)或者常量(Proto::k)
// i Instruction*
// 返回：TValue*
#define RKC(i)	check_exp(getCMode(GET_OPCODE(i)) == OpArgK, \
	ISK(GETARG_C(i)) ? k+INDEXK(GETARG_C(i)) : base+GETARG_C(i))

// [@lvm] Instruction方法
// 获取Instruction对应参数B的常量(Proto::k)
// i Instruction*
// 返回：TValue*
#define KBx(i)	check_exp(getBMode(GET_OPCODE(i)) == OpArgK, k+GETARG_Bx(i))


// [@lvm] 辅助方法，在解释指令的时候执行
// 调整“下一句指令”的地址
#define dojump(L,pc,i)	{(pc) += (i); luai_threadyield(L);}

// [@lvm] 辅助方法
// x 调用代码。。。
// 如果x中可能会进行luaD_call，则加上Protect，以保证调用前后不会出现PC/栈出现偏差
#define Protect(x)	{ L->savedpc = pc; {x;}; base = L->base; }

// [@lvm] 算术模板
// 用来处理基本的算术指令。op是操作表达式，tm是运算符对应的元方法常量
// 既然是模板，当然是用来作为避免代码复制粘贴的了！！
#define arith_op(op,tm) { \
        TValue *rb = RKB(i); \
        TValue *rc = RKC(i); \
        if (ttisnumber(rb) && ttisnumber(rc)) { \
          lua_Number nb = nvalue(rb), nc = nvalue(rc); \
          setnvalue(ra, op(nb, nc)); \
        } \
        else \
          Protect(Arith(L, ra, rb, rc, tm)); \
      }


// 已经准备好了lua函数的调用环境，开始逐句执行lua函数的指令
void luaV_execute (lua_State *L, int nexeccalls) {
  LClosure *cl;
  StkId base;
  TValue *k;
  const Instruction *pc;
 reentry:  /* entry point */
  pc = L->savedpc;					//这时候已经保存了lua函数的第一个指令位置
  cl = &clvalue(L->ci->func)->l;
  base = L->base;
  k = cl->p->k;
  /* main loop of interpreter */
  for (;;) {
    const Instruction i = *pc++;
    StkId ra;
    if ((L->hookmask & (LUA_MASKLINE | LUA_MASKCOUNT)) &&
        (--L->hookcount == 0 || L->hookmask & LUA_MASKLINE)) {
      traceexec(L, pc);
      if (L->status == LUA_YIELD) {  /* did hook yield? */
        L->savedpc = pc - 1;
        return;
      }
      base = L->base;
    }
    /* warning!! several calls may realloc the stack and invalidate `ra' */
    ra = RA(i);
    lua_assert(base == L->base && L->base == L->ci->base);
    lua_assert(base <= L->top && L->top <= L->stack + L->stacksize);
    lua_assert(L->top == L->ci->top || luaG_checkopenop(i));

	// 阅读说明：
	// 每lua的一个函数在编译器会生成这个函数的信息：函数引用到的upvalue，函数固定参数
	// 数量，是否含有可变参数，指令序列等等，这些都记录在Proto结构体中。
	// 可以通过 luac -o tmp <luafile> | luac -l tmp 来函数对应的字节码以及Proto信息
	// 例如以下lua代码：
	// -- t.lua
	// local x, y, z
	// x = x*y + y*z + x*z - (x*x + y*y + z*z)
	// 得到以下输出：
	// main <t.lua:0,0> (12 instructions, 48 bytes at 0074B6A0)
	// 0 + params, 6 slots, 0 upvalues, 3 locals, 0 constants, 0 functions
	//	1[2]	MUL      	3 0 1
	//	2[2]	MUL      	4 1 2
	//	3[2]	ADD      	3 3 4
	//	4[2]	MUL      	4 0 2
	//	5[2]	ADD      	3 3 4
	//	6[2]	MUL      	4 0 0
	//	7[2]	MUL      	5 1 1
	//	8[2]	ADD      	4 4 5
	//	9[2]	MUL      	5 2 2
	//	10[2]	ADD      	4 4 5
	//	11[2]	SUB      	0 3 4
	//	12[2]	RETURN   	0 1
	// 从输出可以得到的信息包括：
	//  1 生成了多少个Proto
	//  2 Proto对应的lua源代码在哪里 (<t.lua:0,0>)
	//  3 Proto中的sizecode (12 instructions, 48 bytes at 0074B6A0)
	//  4 Proto中的固定参数数量numparams (0 + params，这里的0)
	//  5 Proto是否有可变参数is_vararg (0 + params，这里的+表示带有可变参数，没有可变参数就是 0 params)
	//  6 Proto中在栈上用到的临时变量总数maxstacksize (6 slots，表示local变量+计算中辅助用的临时变量=6个)
	//  7 Proto中用到的upvalue数量nups (0 upvalues，表示用到了0个upvalue)
	//  8 Proto中用到的local变量数量sizelocvars (3 locals，刚好t.lua用到了x,y,z三个local变量)
	//  9 Proto中用到的字面常量数量sizek (0 constants)
	// 10 Proto中用到的Closure数量sizep (0 functions)
	// 11 Proto中生成的字节码指令内容code，每条指令包括：
	//    a 指令下标
	//    b 指令在源代码中对应的行号
	//    c 指令opcode
	//    d 指令参数
	// 
	// PS：第6条和第8条，由于计算一条表达式需要用到的辅助临时变量数目是不定的，但是是可以通过
	// 分析一条表达式的AST来确定最少需要几个临时变量（后续遍历+逆波兰式拟真）。
	// PS：lua是会对表达式进行常量计算优化的！例如 x = x + 5*60*60*1000，只有一个常量18000000
	// PS：函数执行的时候需要用到“一段”参数栈上的空间，也就是第6条所谓的临时变量。这一段空间的
	// 范围由L->base开始，到L->top结束。通过相对L->base的下标来标识具体的变量是哪个。一般来说，
	// 固定参数的函数，L->base指向第一个固定参数，而L->base-1指向当前正在运行的函数；而可变参数
	// 的函数，L->base和当前正在运行的函数中间，保存有全部的传入参数。
    switch (GET_OPCODE(i)) {

	  // 功能：用一个已有的变量创建一个新的变量
	  // 将一个 lua_TValue设置成另一个lua_TValue的样子
      // iABC: A待创建变量在参数栈索引，B参数栈已有lua变量的索引。
      case OP_MOVE: {		
		// local x, y   -----> 记录 index(x) = 0, index(y) = 1
		// x = ...
		// .....
		// y = x    -----> OP_MOVE: 1, 0
        setobjs2s(L, ra, RB(i));
        continue;
      }

	  // 功能：用一个常量来创建一个新的变量
	  // 从常量池（保存在Proto类型中）中保存的常量赋值给栈上的变量
	  // iABx: A待创建变量在参数栈索引，Bx常量在常量池的索引
      case OP_LOADK: {		
		// local x = 9	-----> 记录 index(x) = 0, index(constval(9)) = 1 
		//              >----> OP_LOADK: 0, 1
        setobj2s(L, ra, KBx(i));
        continue;
      }

	  // 功能：用一个布尔值来创建一个新的变量
	  // iABC: A待创建变量在参数栈索引，B布尔值，C通常是0
      case OP_LOADBOOL: {	
		// 注意，local c = true这种，true就不作为一个常量放到k里面
		// 而是作为字面值放到参数B里面了！所以不需要KB(i)！
		// local a = false -----> 记录 index(a) = 0
		//                 >----> OP_LOADBOOL 0 0 0
		// local b = true  -----> 记录 index(b) = 1
		//                 >----> OP_LOADBOOL 1 1 0
        setbvalue(ra, GETARG_B(i));
        if (GETARG_C(i)) pc++;  /* skip next instruction (if C) */
        continue;
      }

	  // 功能：用nil来初始化一个到多个变量
	  // 类似于bzero，这个指令会把一段内存中的变量置为nil
	  // iABC: A第一个要置nil的变量参数栈索引，B最后一个要置nil的变量参数栈索引
      case OP_LOADNIL: {
		// local a, b, c, d, e, f, g = 1, 2, 3, 4 -----> index(a~g) = 0~6
		//                                        >----> OP_LOADNIL 4 6
        TValue *rb = RB(i);
        do {
          setnilvalue(rb--);
        } while (rb >= ra);
        continue;
      }

	  // 功能：用upvalue来创建一个新的变量
	  // 所谓的“创建”操作，其实创建的不是副本而是引用
	  // iABC: A待创建变量在参数栈索引，B当前函数的upvalue表的索引
      case OP_GETUPVAL: {	
		// local x = {}
		// ...  -- do something to x
		// function f() local a = x[1] end   -----> 记录index(a) = 0, index(upval(x)) = 1
		//                                   >----> OP_GETUPVAL 0 1
        int b = GETARG_B(i);
        setobj2s(L, ra, cl->upvals[b]->v);
        continue;
      }

	  // 功能：从全局表中取某个key的值来创建一个新的变量
	  // iABx：A待创建变量在参数栈索引，Bxkey对应的常量在常量池的索引
      case OP_GETGLOBAL: {
		// local a = dofile    ------> 记录 index(a) = 0, index(constval("dofile")) = 1
		//                     >-----> OP_GETGLOBAL 0 1
        TValue g;
        TValue *rb = KBx(i);
        sethvalue(L, &g, cl->env);
        lua_assert(ttisstring(rb));
        Protect(luaV_gettable(L, &g, rb, ra));
        continue;
      }

	  // 功能：从某个table中取某个key的值来创建一个新的变量
	  // iABC：A待创建变量在参数栈索引，B要取出key的table变量在参数栈的索引，Ckey对应的参数栈下标或者常量池下标
      case OP_GETTABLE: {
		// local a = hello["world"] -----> 记录 index(a) = 0, index(hello) = 1 index(constval("world")) = 0
		//                          >----> OP_GETTABLE 0 1 0|BITRK
        Protect(luaV_gettable(L, RB(i), RKC(i), ra));
        continue;
      }

	  // 功能：将参数栈上变量设置到全局表中
	  // iABx：A要写入全局表的变量在栈上的索引，Bx写入到全局表的key在常量池中的下标
      case OP_SETGLOBAL: {
		// 假设我要替换 bit库
		// local mybit = {}
		// mybit.band = ...
		// mybit.bor = ...
		// mybit.bxor = ...
		// ...
		// bit = mybit -----> 记录 index(mybit) = 0, index(constval("bit")) = 1
		//             >----> OP_SETGLOBAL 0 1
        TValue g;
        sethvalue(L, &g, cl->env);
        lua_assert(ttisstring(KBx(i)));
        Protect(luaV_settable(L, &g, KBx(i), ra));
        continue;
      }

	  // 功能：修改upvalue的值
	  // iABC：A要写入upvalue的变量在参数栈上的索引，B待写入的upvalue在upvalue表的索引
      case OP_SETUPVAL: {
		// local a = 5
		// function p()
		//  a = "hello" -----> 记录 index(upval(a)) = 0, index(constval("hello")) = 1
		//              >----> OP_SETUPVAL 0 1
		// end
        UpVal *uv = cl->upvals[GETARG_B(i)];
        setobj(L, uv->v, ra);
        luaC_barrier(L, uv, ra);
        continue;
      }

	  // 功能：修改某个table对应的key
	  // iABC：A要写入table变量在参数栈的索引，B要写入的key的变量的栈索引或者常量索引，C要写入的value的变量索引或者常量索引
      case OP_SETTABLE: {
		// local a = {}
		// a[5] = 3
        Protect(luaV_settable(L, ra, RKB(i), RKC(i)));
        continue;
      }

	  // 功能：在栈上创建一个table变量
	  // iABC：A存放table变量的参数栈索引，B创建的table变量的数组容量，C创建的table变量的字典容量
      case OP_NEWTABLE: {
		// local a = {}   -----> index(a) = 0
		//                >----> OP_NEWTABLE 0 szArray szHash
        int b = GETARG_B(i);
        int c = GETARG_C(i);
        sethvalue(L, ra, luaH_new(L, luaO_fb2int(b), luaO_fb2int(c)));
        Protect(luaC_checkGC(L));			// 注意，创建table可能会引起GC
        continue;
      }

	  // 功能：把self.method和self放到参数栈上相邻的两个位置。
	  // 为成员方法调用的语法糖提供支持
	  // iABC：A存放self.method的参数栈索引，B存放self的参数栈索引，C需要从self中调用的方法对应的变量索引或者常量索引
	  // 执行完成后，栈上内容为： ... -> self.method -> self -> ...
	  //                                   ^
	  //                                   RA
	  // 当然，OP_SELF之后能看到OP_CALL的身影
      case OP_SELF: {
		// CCNode:create()  -> index(constants("CCNode")) = 1, index(constants("create")) = 2
		//                  -> OP_GETGLOBAL 0 1
		//                  -> OP_SELF 0 0 2
		//                  -> OP_CALL 0 2 1
        StkId rb = RB(i);
        setobjs2s(L, ra+1, rb);
        Protect(luaV_gettable(L, rb, RKC(i), ra));
        continue;
      }

	//---------------------------------------------------------------------------运算符指令
	  // 功能：实现二元运算符：+, -, *, /, %, ^
	  // iABC：A存放运算结果的参数栈索引，B存放第一操作数的参数栈索引，C存放第二操作数的参数栈索引
      case OP_ADD: {
		// local a, b, c = ... -----> index(a) = 0, index(b) = 1, index(c) = 2
		// a = b + c -----> OP_ADD 0 1|BITRK 2|BITRK
		// a = 1 + b -----> index(constval(1)) = 0
		//           >----> OP_ADD 0 0 1|BITRK
		// a = 1 + 100 -----> index(constval(1)) = 0, index(constval(100)) = 1
		//             >----> OP_ADD 0 0 1
        arith_op(luai_numadd, TM_ADD);
        continue;
      }
      case OP_SUB: {
		// see OP_ADD
        arith_op(luai_numsub, TM_SUB);
        continue;
      }
      case OP_MUL: {
		// see OP_ADD
        arith_op(luai_nummul, TM_MUL);
        continue;
      }
      case OP_DIV: {
		// see OP_ADD
        arith_op(luai_numdiv, TM_DIV);
        continue;
      }
      case OP_MOD: {
		// 这个很特殊！由于lua没有整数，所以mod可不是%这个运算符！
		// 这里定义 mod(x, y) => (x - floor(x/y)*y)
		// see OP_ADD
        arith_op(luai_nummod, TM_MOD);
        continue;
      }
      case OP_POW: {
		// see OP_ADD
        arith_op(luai_numpow, TM_POW);
        continue;
      }

	  // 功能：实现一元运算符 -, not, #
	  // iABC：A存放运算结果的参数栈索引，B存放操作数的参数栈索引
      case OP_UNM: {
		// local a = -b -----> index(a) = 1, index(b) = 2
		//              >----> OP_UNM 1 2
        TValue *rb = RB(i);
        if (ttisnumber(rb)) {
          lua_Number nb = nvalue(rb);
          setnvalue(ra, luai_numunm(nb));
        }
        else {
          Protect(Arith(L, ra, rb, rb, TM_UNM));
        }
        continue;
      }
      case OP_NOT: {
		// local a = not b -----> index(a) = 1, index(b) = 2
		//                 >----> OP_NOT 1 2
		// 那local a = not true呢？人家编译期就给你处理好了
        int res = l_isfalse(RB(i));  /* next assignment may change this value */
        setbvalue(ra, res);
        continue;
      }
      case OP_LEN: {
		// local a = #b -----> index(a) = 1, index(b) = 2
		//              >----> OP_LEN 1 2
        const TValue *rb = RB(i);
        switch (ttype(rb)) {
          case LUA_TTABLE: {
            setnvalue(ra, cast_num(luaH_getn(hvalue(rb))));
            break;
          }
          case LUA_TSTRING: {
            setnvalue(ra, cast_num(tsvalue(rb)->len));
            break;
          }
          default: {  /* try metamethod */
            Protect(
              if (!call_binTM(L, rb, luaO_nilobject, ra, TM_LEN))
                luaG_typeerror(L, rb, "get length of");
            )
          }
        }
        continue;
      }

	  // 功能：实现字符串拼接运算符 ..
	  // iABC：A拼接后存放结果的参数栈索引，B第一个要拼接的变量的参数栈索引，C最后一个要拼接的变量的参数栈索引
	  // 要执行这个指令，对参数栈有特殊要求：
	  // ... -> string1 -> string2 ... -> stringN -> ...
	  //          ^                          ^
	  //          RB                         RC
      case OP_CONCAT: {
		// 类似OP_LOADNIL，只不过，这次范围是[rb,rc]，loadnil是[ra,rb]
		// local b, c, d, a = "hello", "world", "!"
		// a = b .. c .. d -----> index(a) = 4, index(b~d) = 1~3
		//                 >----> OP_CONCAT 4 1 3
		// 问题是如果b~d不能保证是连续的怎么办？答案是一个个MOVE上去在OP_CONCAT...
        int b = GETARG_B(i);
        int c = GETARG_C(i);
        Protect(luaV_concat(L, c-b+1, c); luaC_checkGC(L));
        setobjs2s(L, RA(i), base+b);
        continue;
      }

	//---------------------------------------------------------------------------跳转指令
	  // 功能：无条件跳转
	  // iAsBx：A不使用，sBx跳转偏移
	  // 一般这个语句不单独出现，都是在一些条件控制中和其他的条件跳转指令配合使用的。
      case OP_JMP: {
		// 无条件跳转指令。由于跳转偏移总是有正向和反向之分的，所以需要用到
		// 负数。那就只能用iAsBx类型的指令了。而sBx是有长度限制的！
		// 所以，如果生成的指令很多，超过了sBx的长度限制，可能就会编译失败
        dojump(L, pc, GETARG_sBx(i));
        continue;
      }

	  // 功能：检查两个变量是否相等，满足预期则跳转。配合OP_JMP使用。
	  // iABC：A纯数字，对比较结果的预期，满足预期则跳转，B参数1的索引，C参数2的索引
      case OP_EQ: {
		// if a == b then -----> index(a) = 1, index(b) = 2
		//                >----> 这里将生成两行指令：
		//                >----> OP_EQ  0 1 2	// 用0，因为成立的话跳过，不成立才执行
		//                >----> OP_JMP N		// N 表示then ... end中间的指令数量
		//                >----> ...			// Instructions between "then" and "end"
        TValue *rb = RKB(i);
        TValue *rc = RKC(i);
        Protect(
          if (equalobj(L, rb, rc) == GETARG_A(i))
            dojump(L, pc, GETARG_sBx(*pc));
        )
        pc++;
        continue;
      }
	
	  // 功能：检查两个变量是否小于，满足预期则跳转。配合OP_JMP使用。
	  // iABC：A纯数字，对比较结果的预期，满足预期则跳转，B参数1的索引，C参数2的索引
      case OP_LT: {
		// if a < b then -----> index(a) = 1, index(b) = 2
		//               >----> 这里将生成两行指令：
		//               >----> OP_LT  0 1 2	// 用0，因为成立的话跳过，不成立才执行
		//               >----> OP_JMP N		// N 表示then ... end中间的指令数量
		//               >----> ...				// Instructions between "then" and "end"
        Protect(
          if (luaV_lessthan(L, RKB(i), RKC(i)) == GETARG_A(i))
            dojump(L, pc, GETARG_sBx(*pc));
        )
        pc++;
        continue;
      }

	  // 功能：检查两个变量是否小于等于，满足预期则跳转。配合OP_JMP使用。
	  // iABC：A纯数字，对比较结果的预期，满足预期则跳转，B参数1的索引，C参数2的索引
      case OP_LE: {
		  // if a <= b then -----> index(a) = 1, index(b) = 2
		  //                >----> 这里将生成两行指令：
		  //                >----> OP_LE  0 1 2	// 用0，因为成立的话跳过，不成立才执行
		  //                >----> OP_JMP N		// N 表示then ... end中间的指令数量
		  //                >----> ...			// Instructions between "then" and "end"
		  Protect(
          if (lessequal(L, RKB(i), RKC(i)) == GETARG_A(i))
            dojump(L, pc, GETARG_sBx(*pc));
        )
        pc++;
        continue;
      }

	  // 功能：检查某个变量的布尔值，和预期相同则跳转。配合OP_JMP使用。
	  // iABC：A变量在参数栈上的索引，C纯数字，表示变量A的布尔值预期
      case OP_TEST: {
		// if a == false then -----> index(a) = 1
		//                    >----> OP_TEST 1 n 0
		//                    >----> OP_JMP N
		//                    >----> ...
        if (l_isfalse(ra) != GETARG_C(i))
          dojump(L, pc, GETARG_sBx(*pc));
        pc++;
        continue;
      }
      case OP_TESTSET: {// TODO: 没想明白在哪里适用...
        TValue *rb = RB(i);
        if (l_isfalse(rb) != GETARG_C(i)) {
          setobjs2s(L, ra, rb);
          dojump(L, pc, GETARG_sBx(*pc));
        }
        pc++;
        continue;
      }

	  // 功能：调用某个Closure对象
	  // iABC：AClosure对象所在的参数栈索引，B传入参数数量+1，C期待返回的数量+1
	  // 要执行这个指令，对参数栈有特殊要求：
	  // ... -> Closure -> param1 -> param2 ... -> paramN -> ...
	  //           ^         |___________ ___________|
	  //           |                     V
	  //           RA                   RB-1
	  // 注意，调用回来后，RA -> ... -> RA+RC-2上，保存着Closure返回的内容，不够的会填nil
      case OP_CALL: {		// 
		// local a = foo(1,2,3)
		// 懒得分析了，直接贴luac得到的字节码
		//    1[1]     GETGLOBAL       0 - 1; foo
		//	  2[1]     LOADK           1 - 2; 1
		//	  3[1]     LOADK           2 - 3; 2
		//	  4[1]     LOADK           3 - 4; 3
		//	  5[1]     CALL            0 4 2
		//	  6[1]     RETURN          0 1
        int b = GETARG_B(i);
        int nresults = GETARG_C(i) - 1;
        if (b != 0) L->top = ra+b;  /* else previous instruction set top */
        L->savedpc = pc;
        switch (luaD_precall(L, ra, nresults)) {
          case PCRLUA: {
            nexeccalls++;
            goto reentry;  /* restart luaV_execute over new Lua function */
          }
          case PCRC: {
            /* it was a C function (`precall' called it); adjust results */
            if (nresults >= 0) L->top = L->ci->top;
            base = L->base;
            continue;
          }
          default: {
            return;  /* yield */
          }
        }
      }

	  // 功能：“调用”某个Closure，并在其完成调用后清理当前stack frame。
	  // iABC：AClosure对象所在的参数栈索引，B传入参数数量+1，C永远是0
	  // 注意：首先，这个指令是会产生函数调用的！！！也就是有了OP_TAILCALL就不需要一个OP_CALL来配合使用了。
	  //      其次，它需要一个OP_RETURN来配合使用，因为它已经是当前函数的最后一条语句了！
      case OP_TAILCALL: {	// iABC：Alua函数，B传入参数数量+1，C期待返回的数量+1
		// function f(n) 
		//   return f(n-1)
		// end
		// 生成：   GETGLOBAL 1, -1 -- index(constant("f")) = -1
		//         SUB 2, 0, -2		-- index(constant(-1)) = -2
		//         TAILCALL 1, 2, 0
		//         RETURN 1, 0
		//         RETURN 0, 1
        int b = GETARG_B(i);
        if (b != 0) L->top = ra+b;  /* else previous instruction set top */
        L->savedpc = pc;
        lua_assert(GETARG_C(i) - 1 == LUA_MULTRET);
        switch (luaD_precall(L, ra, LUA_MULTRET)) {	// 注意，这里调用了luaD_precall
          case PCRLUA: {
            /* tail call: put new frame in place of previous one */
			// luaD_precall会进行 ++L->ci 操作，所以TAILCALL所在的callinfo是L->ci - 1 [ldo.c: now `enter' new function]
            CallInfo *ci = L->ci - 1;  /* previous frame */
            int aux;
            StkId func = ci->func;
            StkId pfunc = (ci+1)->func;  /* previous function index */
            if (L->openupval) luaF_close(L, ci->base);	
            L->base = ci->base = ci->func + ((ci+1)->base - pfunc);
            for (aux = 0; pfunc+aux < L->top; aux++)  /* move frame down */
              setobjs2s(L, func+aux, pfunc+aux);
            ci->top = L->top = func+aux;  /* correct top */
            lua_assert(L->top == L->base + clvalue(func)->l.p->maxstacksize);
            ci->savedpc = L->savedpc;
            ci->tailcalls++;  /* one more call lost */
            L->ci--;  /* remove new frame */
            goto reentry;
          }
          case PCRC: {  /* it was a C function (`precall' called it) */
            base = L->base;
            continue;
          }
          default: {
            return;  /* yield */
          }
        }
      }

	  // 功能：跳出当前函数
      // iABC: A第一个返回值的栈索引 B返回值总数量+1
      case OP_RETURN: {		
        int b = GETARG_B(i);
        if (b != 0) L->top = ra+b-1;
        if (L->openupval) luaF_close(L, base);
        L->savedpc = pc;
        b = luaD_poscall(L, ra);
        if (--nexeccalls == 0)  /* was previous function running `here'? */
          return;  /* no: return */
        else {  /* yes: continue its execution */
          if (b) L->top = L->ci->top;
          lua_assert(isLua(L->ci));
          lua_assert(GET_OPCODE(*((L->ci)->savedpc - 1)) == OP_CALL);
          goto reentry;
        }
      }

	  // 功能：for循环，使用于 for+step 语法。如 for i=1,100 do sum = sum+i end
	  // iAsBx：A循环变量初始值 sBx指令跳转偏移
	  // 要执行这个指令，对栈变量有特殊需求。假设for语句为 for variable=init,ended,step则
	  //   ... -> init -> ended -> step -> loopv -> ...
	  //           ^                         ^
	  //       RA is here               use in loop block
	  // 所以啊，在循环里面修改循环变量的时候，不会对循环次数造成任何影响
      case OP_FORLOOP: {
        lua_Number step = nvalue(ra+2);
        lua_Number idx = luai_numadd(nvalue(ra), step); // 由于FORLOOP和FORPREP是配合使用的，FORPREP做了一次减法，所以这里来一次加法
        lua_Number limit = nvalue(ra+1);
        if (luai_numlt(0, step) ? luai_numle(idx, limit)
                                : luai_numle(limit, idx)) {
          dojump(L, pc, GETARG_sBx(i));  /* jump back */
          setnvalue(ra, idx);  /* update internal index... */
          setnvalue(ra+3, idx);  /* ...and external index */
        }
        continue;
      }

	  // 功能：和OP_FORLOOP配合使用。先执行一次FORPREP，再执行FORLOOP
	  // 进行 FORPREP 主要是验证参数没问题。
	  // iAsBx：A同OP_FORLOOP，sBx对应的OP_FORLOOP的指令偏移
      case OP_FORPREP: {
        const TValue *init = ra;
        const TValue *plimit = ra+1;
        const TValue *pstep = ra+2;
        L->savedpc = pc;  /* next steps may throw errors */
        if (!tonumber(init, ra))
          luaG_runerror(L, LUA_QL("for") " initial value must be a number");
        else if (!tonumber(plimit, ra+1))
          luaG_runerror(L, LUA_QL("for") " limit must be a number");
        else if (!tonumber(pstep, ra+2))
          luaG_runerror(L, LUA_QL("for") " step must be a number");
		//这里的减法我认为妙不可言，它保证了FORLOOP中的加法操作不需要增加一个if语句！
		//要知道FORLOOP是每个循环都执行一次的，少一个条件就少用一点时间
        setnvalue(ra, luai_numsub(nvalue(ra), nvalue(pstep)));
        dojump(L, pc, GETARG_sBx(i));
        continue;
      }
	  
	  // 功能：for循环，用于 for+迭代器 的方案。这个指令没有TFORPREP配套，要和FORLOOP区分
	  // 首先，复习一下迭代器的要求
	  // for k, v in f( container ) do ... end
	  // 其中，f称为 iterator generator，它将会返回三个值：iterator, temporary_container, iterator_key
	  // 此后，iterator generator不会再被调用。每个循环只是调用 iterator( temporary_container, iterator_key )
	  // 并把调用的返回值赋值到 k, v 上，且更新 iterator_key = k
	  // 要理解这个指令，需要看代码编译出来得到的整个指令序列的上下文：
	  //     首先设置好栈： ... -> f -> container -> ...
	  //     然后调用f：    CALL index(f), 2, 4   -- 参数是container，只有一个，所以B=2，返回3个值，所以是4
	  //     然后跳转：     JMP  offset(TFORLOOP)
	  //     中间是循环体： ...                   -- 每个循环执行的指令
	  //     接着是迭代：   TFORLOOP index(f)     -- offset(TFORLOOP)指的就是这里
	  //     最后是跳转：   JMP  offset(...)      -- 指示跳转回去的位置
	  // 如果f返回了3个值，那就是： ... -> iterator[原来的index(f)] -> temporary_container -> iterator_key -> ...
	  //                                      ^
	  //                                      RA
	  // iABC：Aiterator generator在参数栈的索引
      case OP_TFORLOOP: {
        StkId cb = ra + 3;  /* call base */
        setobjs2s(L, cb+2, ra+2);
        setobjs2s(L, cb+1, ra+1);
        setobjs2s(L, cb, ra);
        L->top = cb+3;  /* func. + 2 args (state and index) */
        Protect(luaD_call(L, cb, GETARG_C(i)));
        L->top = L->ci->top;
        cb = RA(i) + 3;  /* previous call may change the stack */
        if (!ttisnil(cb)) {  /* continue loop? */
		  // cb-1,保存的是iterator_key，luaD_call之后，cb保存的就是k，所以下面这句就是 iterator_key = k
          setobjs2s(L, cb-1, cb);  /* save control variable */
          dojump(L, pc, GETARG_sBx(*pc));  /* jump back */
        }
        pc++;
        continue;
      }

	  // 功能描述：从栈上指定位置截取一段lua_TValue，保存到指定的Table中的数组部分
	  // 类似 memcpy(&RA[RC], &localvar, B-1)
	  // iABC：A目的table对象, B数组长度, C相对于A的偏移
	  // 要执行这个指令，栈上实例摆放需要这样(localval紧跟RA)：
	  //      Target -> localvar1 -> localvar2 ...
	  // 这个指令基本上都用在table的初始化语句。如
	  // a = {1,2,3,'a'}
      case OP_SETLIST: {
        int n = GETARG_B(i);
        int c = GETARG_C(i);
        int last;
        Table *h;
		// 只有在和OP_VARARG连用的时候，才会令RB=0
        if (n == 0) {
          n = cast_int(L->top - ra) - 1;
          L->top = L->ci->top;
        }
        if (c == 0) c = cast_int(*pc++);	// 应该是个特殊处理，当rc放不下，就放在下一个指令中
        runtime_check(L, ttistable(ra));
        h = hvalue(ra);
        last = ((c-1)*LFIELDS_PER_FLUSH) + n;
        if (last > h->sizearray)  /* needs more space? */
          luaH_resizearray(L, h, last);  /* pre-alloc it at once */
        for (; n > 0; n--) {
          TValue *val = ra+n;
          setobj2t(L, luaH_setnum(L, h, last--), val);
          luaC_barriert(L, h, val);
        }
        continue;
      }

	  // 在下无能，没找到哪里生成了这个指令
      case OP_CLOSE: {
        luaF_close(L, ra);
        continue;
      }
	  
	  // 功能：在栈上指定位置创建一个Closure对象，要知道，Closure对象是可回收的
	  // 要创建一个Closure对象，需要的数据包括
	  // 1 函数原型(Proto实例)
	  // 2 环境表
	  // 3 upvalue引用
	  // iABx：A要创建的Closure对象的位置，Bx函数原型在当前Closure中的索引
	  // 这个指令会有一系列的附带指令，用于初始化upvalues，每条指令初始化一个upvalue。
	  // 这些附带，在使用luac -l的时候可以查看对应“function”的描述处 `1 upvalues` 这样的字眼
      case OP_CLOSURE: {
        Proto *p;
        Closure *ncl;
        int nup, j;
        p = cl->p->p[GETARG_Bx(i)];
        nup = p->nups;
        ncl = luaF_newLclosure(L, nup, cl->env);
        ncl->l.p = p;
        for (j=0; j<nup; j++, pc++) {
          if (GET_OPCODE(*pc) == OP_GETUPVAL)
            ncl->l.upvals[j] = cl->upvals[GETARG_B(*pc)];
          else {
            lua_assert(GET_OPCODE(*pc) == OP_MOVE);
            ncl->l.upvals[j] = luaF_findupval(L, base + GETARG_B(*pc));
          }
        }
        setclvalue(L, ra, ncl);
        Protect(luaC_checkGC(L));
        continue;
      }

	  // 功能：类似memcpy(RA, &varg[0], RB-1)，拷贝可变参数列表到栈上指定位置并连续存放
	  // iABC：A指定拷贝位置，B待拷贝长度+1，若是B=0表示#{...}
      case OP_VARARG: {
		// 这样的话，例如 local a, b, c = ... 这样的代码，只要保证 index(a)~index(c)是
		// 连续存放在栈上的，那就可以一个指令搞定
		// 当然，如果是这样的代码：
		// local a, b, c, d
		// b, d = ...
		// 那就不是一个指令搞定了，要先VARARG到最后，然后再 OP_MOVE到index(b)，再次OP_MOVE到index(d)
		// 那么，显然为了执行速度，千万别写出这样没效率的垃圾代码！！！

		// 在ldo的函数adjust_varargs中，有一个诡异的地方，
		// 在函数最后，导致stack frame保存形式(nargs表示传入参数数量，nparam表示函数命名参数数量)
		// stack-index    value
		//  0              func              <--- ci->func is here
		//  1              nil
		//  2              nil
		//  ...
		//  nparam         nil
		//  nparam+1       varg1 for func
		//  nparam+2       varg2 for func
		//  ...
		//  nargs		   vargn for func
		// :0              param1 for func   <--- ci->base is here
		// :1              param2 for func
		// :...
		// :nparam         paramN for func
		// :htab           {...} for func
		// TOP->           nil
        int b = GETARG_B(i) - 1;
        int j;
        CallInfo *ci = L->ci;
		// ci->base - ci->func: 得到 `传入参数个数`
		// 所以 n = #{...}
        int n = cast_int(ci->base - ci->func) - cl->p->numparams - 1;
        if (b == LUA_MULTRET) {
          Protect(luaD_checkstack(L, n));
          ra = RA(i);  /* previous call may change the stack */
          b = n;
          L->top = ra + n;
        }
        for (j = 0; j < b; j++) {
          if (j < n) {
            setobjs2s(L, ra + j, ci->base - n + j);	// copy varg[j] to ra[j]
          }
          else {
            setnilvalue(ra + j);
          }
        }
		// 基本上，执行到这里，stack frame就是
		// stack-index    value
		//  0              func              <--- ci->func is here
		//  1              nil
		//  2              nil
		//  ...
		//  nparam         nil
		//  nparam+1       varg1 for func
		//  nparam+2       varg2 for func
		//  ...
		//  nargs		   vargn for func
		// :0              param1 for func   <--- ci->base is here
		// :1              param2 for func
		// :...
		// :nparam         paramN for func
		// :htab           {...} for func
		// :nparam+2       varg1 for func
		// :nparam+3       varg2 for func
		// ...
		// :nargs+1        vargN for func
		// TOP->           nil
		// 这样看起来，没那么诡异了
        continue;
      }
    }
  }
}

