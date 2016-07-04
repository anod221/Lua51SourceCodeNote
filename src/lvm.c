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

// [@lvm] ���obj��lua�����֣�����obj�����obj��lua���ַ�������n��Ϊobj��Ӧ������ֵ
// �൱��lua���룺
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

// [@lvm] ���obj��Ӧ��lua�����֣����objֱ��ת��str���������棬����ʲô���������ؼ�
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

// [@lvm] ��鲢����hook����
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
	// ����ϸ�ˣ�`when jump back`����Ӧpc <= oldpc����Ҳ��newline�����Ե���
	// LDT����ͬһ��ͣѽͣ��
    if (npc == 0 || pc <= oldpc || newline != getline(p, pcRel(oldpc, p)))
      luaD_callhook(L, LUA_HOOKLINE, newline);
  }
}

// [@lvm] ����lua�ĺ���f������1������2��p1,p2��f�ķ���ֵд��res
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


// [@lvm] ����3����Ԫ����f�������ֱ���p1, p2, p3��f�����з���ֵ
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

// [@lvm] ��Tableʵ��t��ȡkey��Ӧ��value��val��
// �൱��lua���룺 val = t[key]
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

// [@lvm] ��Tableʵ��t������key�Ķ�ӦvalueΪval
// �൱��lua���룺t[key] = val
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

// [@lvm] ���ͬһ��Class������event��Ӧ��Ԫ�������������p1, p2��д�뷵��ֵ��res
// ��call_orderTM����ͬ��
// 1 ����2Ԫ����
// 2 ���з���ֵ
// 3 binTMҪ�����������1������metamethod��orderTM��Ҫ��ȫ��������ͬ��metamethod
// 4 binTM�ķ���ֵд��res��orderTM�ķ���ֵд��top
static int call_binTM (lua_State *L, const TValue *p1, const TValue *p2,
                       StkId res, TMS event) {
  const TValue *tm = luaT_gettmbyobj(L, p1, event);  /* try first operand */
  if (ttisnil(tm))
    tm = luaT_gettmbyobj(L, p2, event);  /* try second operand */
  if (!ttisfunction(tm)) return 0;
  callTMres(L, res, tm, p1, p2);
  return 1;
}

// [@lvm] ������Ԫ��mt1��mt2ȡ��event�ֶε�TValue
// ע���б�֤����Ԫ���event�ֶζ�Ӧ��ֵ��ͬ����ͬ�᷵��NULL
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

// [@lvm] ������p1,p2����ֵ��Ԫ�ȽϷ���
// ��call_orderTM����ͬ��
// 1 ����2Ԫ����
// 2 ���з���ֵ
// 3 binTMҪ�����������1������metamethod��orderTM��Ҫ��ȫ��������ͬ��metamethod
// 4 binTM�ķ���ֵд��res��orderTM�ķ���ֵд��top
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

// [@lvm] �Ƚ�����luaֵ��С
// ����޷��Ƚϣ������׳��쳣
// if nil < nil Ҳ���׳��쳣����
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

// [@lvm] �������lua������ֵ�Ƿ����
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

// [@ltm] �Բ���ջ�ϵĲ���ִ��concat����
// total ��Ҫconcat�Ĳ����ĸ���
// last concat���������һ��Ԫ���±�
void luaV_concat (lua_State *L, int total, int last) {
  do {
    StkId top = L->base + last + 1;
    int n = 2;  /* number of elements handled in this pass (at least 2) */
    if (!tostring(L, top-2) || !tostring(L, top-1)) {
	  // ����������ȫ����string������ֻ��ͨ��metamethod������
      if (!call_binTM(L, top-2, top-1, top-2, TM_CONCAT))
        luaG_concaterror(L, top-2, top-1);
    } else if (tsvalue(top-1)->len > 0) {  /* if len=0, do nothing<�Ż�����·ѽ> */
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
	  // ���ﱣ����Ҫconcat��string���ܴ�С
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

// [@lvm] �����������㣬ra����ֵ, rb, rc�Ƕ�Ԫ������, op�������
// ע�⣬�˺�����������������Ͳ�ǿ��ת��Ϊ����
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

// [@lvm] Instruction����
// ��ȡInstruction��Ӧ����A��StkId��Ҳ����lua����
// i Instruction*
// ���أ�TValue*
// base �����c�������渳ֵ�ľֲ���������ӦL->base
// GETARG_A ָ��Ĳ���A�������lua������ջ�ϵ�����
#define RA(i)	(base+GETARG_A(i))
/* to be used after possible stack reallocation */
// [@lvm] Instruction����
// ��ȡInstruction��Ӧ����B��StkId��Ҳ����lua����
// i Instruction*
// ���أ�TValue*
#define RB(i)	check_exp(getBMode(GET_OPCODE(i)) == OpArgR, base+GETARG_B(i))
// [@lvm] Instruction����
// ��ȡInstruction��Ӧ����C��StkId��Ҳ����lua����
// i Instruction*
// ���أ�TValue*
#define RC(i)	check_exp(getCMode(GET_OPCODE(i)) == OpArgR, base+GETARG_C(i))
// [@lvm] Instruction����
// ��ȡInstruction��Ӧ����B��StkId(Proto::base+index)���߳���(Proto::k)
// i Instruction*
// ���أ�TValue*
// �²ۣ�֪��inline�ĺô��˰ɣ������������赭д�ĵ���RKB(i)��ʵ���϶�ô���ӣ�
#define RKB(i)	check_exp(getBMode(GET_OPCODE(i)) == OpArgK, \
	ISK(GETARG_B(i)) ? k+INDEXK(GETARG_B(i)) : base+GETARG_B(i))

// [@lvm] Instruction����
// ��ȡInstruction��Ӧ����C��StkId(Proto::base+index)���߳���(Proto::k)
// i Instruction*
// ���أ�TValue*
#define RKC(i)	check_exp(getCMode(GET_OPCODE(i)) == OpArgK, \
	ISK(GETARG_C(i)) ? k+INDEXK(GETARG_C(i)) : base+GETARG_C(i))

// [@lvm] Instruction����
// ��ȡInstruction��Ӧ����B�ĳ���(Proto::k)
// i Instruction*
// ���أ�TValue*
#define KBx(i)	check_exp(getBMode(GET_OPCODE(i)) == OpArgK, k+GETARG_Bx(i))


// [@lvm] �����������ڽ���ָ���ʱ��ִ��
// ��������һ��ָ��ĵ�ַ
#define dojump(L,pc,i)	{(pc) += (i); luai_threadyield(L);}

// [@lvm] ��������
// x ���ô��롣����
// ���x�п��ܻ����luaD_call�������Protect���Ա�֤����ǰ�󲻻����PC/ջ����ƫ��
#define Protect(x)	{ L->savedpc = pc; {x;}; base = L->base; }

// [@lvm] ����ģ��
// �����������������ָ�op�ǲ������ʽ��tm���������Ӧ��Ԫ��������
// ��Ȼ��ģ�壬��Ȼ��������Ϊ������븴��ճ�����ˣ���
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


// �Ѿ�׼������lua�����ĵ��û�������ʼ���ִ��lua������ָ��
void luaV_execute (lua_State *L, int nexeccalls) {
  LClosure *cl;
  StkId base;
  TValue *k;
  const Instruction *pc;
 reentry:  /* entry point */
  pc = L->savedpc;					//��ʱ���Ѿ�������lua�����ĵ�һ��ָ��λ��
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

	// �Ķ�˵����
	// ÿlua��һ�������ڱ����������������������Ϣ���������õ���upvalue�������̶�����
	// �������Ƿ��пɱ������ָ�����еȵȣ���Щ����¼��Proto�ṹ���С�
	// ����ͨ�� luac -o tmp <luafile> | luac -l tmp ��������Ӧ���ֽ����Լ�Proto��Ϣ
	// ��������lua���룺
	// -- t.lua
	// local x, y, z
	// x = x*y + y*z + x*z - (x*x + y*y + z*z)
	// �õ����������
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
	// ��������Եõ�����Ϣ������
	//  1 �����˶��ٸ�Proto
	//  2 Proto��Ӧ��luaԴ���������� (<t.lua:0,0>)
	//  3 Proto�е�sizecode (12 instructions, 48 bytes at 0074B6A0)
	//  4 Proto�еĹ̶���������numparams (0 + params�������0)
	//  5 Proto�Ƿ��пɱ����is_vararg (0 + params�������+��ʾ���пɱ������û�пɱ�������� 0 params)
	//  6 Proto����ջ���õ�����ʱ��������maxstacksize (6 slots����ʾlocal����+�����и����õ���ʱ����=6��)
	//  7 Proto���õ���upvalue����nups (0 upvalues����ʾ�õ���0��upvalue)
	//  8 Proto���õ���local��������sizelocvars (3 locals���պ�t.lua�õ���x,y,z����local����)
	//  9 Proto���õ������泣������sizek (0 constants)
	// 10 Proto���õ���Closure����sizep (0 functions)
	// 11 Proto�����ɵ��ֽ���ָ������code��ÿ��ָ�������
	//    a ָ���±�
	//    b ָ����Դ�����ж�Ӧ���к�
	//    c ָ��opcode
	//    d ָ�����
	// 
	// PS����6���͵�8�������ڼ���һ�����ʽ��Ҫ�õ��ĸ�����ʱ������Ŀ�ǲ����ģ������ǿ���ͨ��
	// ����һ�����ʽ��AST��ȷ��������Ҫ������ʱ��������������+�沨��ʽ���棩��
	// PS��lua�ǻ�Ա��ʽ���г��������Ż��ģ����� x = x + 5*60*60*1000��ֻ��һ������18000000
	// PS������ִ�е�ʱ����Ҫ�õ���һ�Ρ�����ջ�ϵĿռ䣬Ҳ���ǵ�6����ν����ʱ��������һ�οռ��
	// ��Χ��L->base��ʼ����L->top������ͨ�����L->base���±�����ʶ����ı������ĸ���һ����˵��
	// �̶������ĺ�����L->baseָ���һ���̶���������L->base-1ָ��ǰ�������еĺ��������ɱ����
	// �ĺ�����L->base�͵�ǰ�������еĺ����м䣬������ȫ���Ĵ��������
    switch (GET_OPCODE(i)) {

	  // ���ܣ���һ�����еı�������һ���µı���
	  // ��һ�� lua_TValue���ó���һ��lua_TValue������
      // iABC: A�����������ڲ���ջ������B����ջ����lua������������
      case OP_MOVE: {		
		// local x, y   -----> ��¼ index(x) = 0, index(y) = 1
		// x = ...
		// .....
		// y = x    -----> OP_MOVE: 1, 0
        setobjs2s(L, ra, RB(i));
        continue;
      }

	  // ���ܣ���һ������������һ���µı���
	  // �ӳ����أ�������Proto�����У��б���ĳ�����ֵ��ջ�ϵı���
	  // iABx: A�����������ڲ���ջ������Bx�����ڳ����ص�����
      case OP_LOADK: {		
		// local x = 9	-----> ��¼ index(x) = 0, index(constval(9)) = 1 
		//              >----> OP_LOADK: 0, 1
        setobj2s(L, ra, KBx(i));
        continue;
      }

	  // ���ܣ���һ������ֵ������һ���µı���
	  // iABC: A�����������ڲ���ջ������B����ֵ��Cͨ����0
      case OP_LOADBOOL: {	
		// ע�⣬local c = true���֣�true�Ͳ���Ϊһ�������ŵ�k����
		// ������Ϊ����ֵ�ŵ�����B�����ˣ����Բ���ҪKB(i)��
		// local a = false -----> ��¼ index(a) = 0
		//                 >----> OP_LOADBOOL 0 0 0
		// local b = true  -----> ��¼ index(b) = 1
		//                 >----> OP_LOADBOOL 1 1 0
        setbvalue(ra, GETARG_B(i));
        if (GETARG_C(i)) pc++;  /* skip next instruction (if C) */
        continue;
      }

	  // ���ܣ���nil����ʼ��һ�����������
	  // ������bzero�����ָ����һ���ڴ��еı�����Ϊnil
	  // iABC: A��һ��Ҫ��nil�ı�������ջ������B���һ��Ҫ��nil�ı�������ջ����
      case OP_LOADNIL: {
		// local a, b, c, d, e, f, g = 1, 2, 3, 4 -----> index(a~g) = 0~6
		//                                        >----> OP_LOADNIL 4 6
        TValue *rb = RB(i);
        do {
          setnilvalue(rb--);
        } while (rb >= ra);
        continue;
      }

	  // ���ܣ���upvalue������һ���µı���
	  // ��ν�ġ���������������ʵ�����Ĳ��Ǹ�����������
	  // iABC: A�����������ڲ���ջ������B��ǰ������upvalue�������
      case OP_GETUPVAL: {	
		// local x = {}
		// ...  -- do something to x
		// function f() local a = x[1] end   -----> ��¼index(a) = 0, index(upval(x)) = 1
		//                                   >----> OP_GETUPVAL 0 1
        int b = GETARG_B(i);
        setobj2s(L, ra, cl->upvals[b]->v);
        continue;
      }

	  // ���ܣ���ȫ�ֱ���ȡĳ��key��ֵ������һ���µı���
	  // iABx��A�����������ڲ���ջ������Bxkey��Ӧ�ĳ����ڳ����ص�����
      case OP_GETGLOBAL: {
		// local a = dofile    ------> ��¼ index(a) = 0, index(constval("dofile")) = 1
		//                     >-----> OP_GETGLOBAL 0 1
        TValue g;
        TValue *rb = KBx(i);
        sethvalue(L, &g, cl->env);
        lua_assert(ttisstring(rb));
        Protect(luaV_gettable(L, &g, rb, ra));
        continue;
      }

	  // ���ܣ���ĳ��table��ȡĳ��key��ֵ������һ���µı���
	  // iABC��A�����������ڲ���ջ������BҪȡ��key��table�����ڲ���ջ��������Ckey��Ӧ�Ĳ���ջ�±���߳������±�
      case OP_GETTABLE: {
		// local a = hello["world"] -----> ��¼ index(a) = 0, index(hello) = 1 index(constval("world")) = 0
		//                          >----> OP_GETTABLE 0 1 0|BITRK
        Protect(luaV_gettable(L, RB(i), RKC(i), ra));
        continue;
      }

	  // ���ܣ�������ջ�ϱ������õ�ȫ�ֱ���
	  // iABx��AҪд��ȫ�ֱ�ı�����ջ�ϵ�������Bxд�뵽ȫ�ֱ��key�ڳ������е��±�
      case OP_SETGLOBAL: {
		// ������Ҫ�滻 bit��
		// local mybit = {}
		// mybit.band = ...
		// mybit.bor = ...
		// mybit.bxor = ...
		// ...
		// bit = mybit -----> ��¼ index(mybit) = 0, index(constval("bit")) = 1
		//             >----> OP_SETGLOBAL 0 1
        TValue g;
        sethvalue(L, &g, cl->env);
        lua_assert(ttisstring(KBx(i)));
        Protect(luaV_settable(L, &g, KBx(i), ra));
        continue;
      }

	  // ���ܣ��޸�upvalue��ֵ
	  // iABC��AҪд��upvalue�ı����ڲ���ջ�ϵ�������B��д���upvalue��upvalue�������
      case OP_SETUPVAL: {
		// local a = 5
		// function p()
		//  a = "hello" -----> ��¼ index(upval(a)) = 0, index(constval("hello")) = 1
		//              >----> OP_SETUPVAL 0 1
		// end
        UpVal *uv = cl->upvals[GETARG_B(i)];
        setobj(L, uv->v, ra);
        luaC_barrier(L, uv, ra);
        continue;
      }

	  // ���ܣ��޸�ĳ��table��Ӧ��key
	  // iABC��AҪд��table�����ڲ���ջ��������BҪд���key�ı�����ջ�������߳���������CҪд���value�ı����������߳�������
      case OP_SETTABLE: {
		// local a = {}
		// a[5] = 3
        Protect(luaV_settable(L, ra, RKB(i), RKC(i)));
        continue;
      }

	  // ���ܣ���ջ�ϴ���һ��table����
	  // iABC��A���table�����Ĳ���ջ������B������table����������������C������table�������ֵ�����
      case OP_NEWTABLE: {
		// local a = {}   -----> index(a) = 0
		//                >----> OP_NEWTABLE 0 szArray szHash
        int b = GETARG_B(i);
        int c = GETARG_C(i);
        sethvalue(L, ra, luaH_new(L, luaO_fb2int(b), luaO_fb2int(c)));
        Protect(luaC_checkGC(L));			// ע�⣬����table���ܻ�����GC
        continue;
      }

	  // ���ܣ���self.method��self�ŵ�����ջ�����ڵ�����λ�á�
	  // Ϊ��Ա�������õ��﷨���ṩ֧��
	  // iABC��A���self.method�Ĳ���ջ������B���self�Ĳ���ջ������C��Ҫ��self�е��õķ�����Ӧ�ı����������߳�������
	  // ִ����ɺ�ջ������Ϊ�� ... -> self.method -> self -> ...
	  //                                   ^
	  //                                   RA
	  // ��Ȼ��OP_SELF֮���ܿ���OP_CALL����Ӱ
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

	//---------------------------------------------------------------------------�����ָ��
	  // ���ܣ�ʵ�ֶ�Ԫ�������+, -, *, /, %, ^
	  // iABC��A����������Ĳ���ջ������B��ŵ�һ�������Ĳ���ջ������C��ŵڶ��������Ĳ���ջ����
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
		// ��������⣡����luaû������������mod�ɲ���%����������
		// ���ﶨ�� mod(x, y) => (x - floor(x/y)*y)
		// see OP_ADD
        arith_op(luai_nummod, TM_MOD);
        continue;
      }
      case OP_POW: {
		// see OP_ADD
        arith_op(luai_numpow, TM_POW);
        continue;
      }

	  // ���ܣ�ʵ��һԪ����� -, not, #
	  // iABC��A����������Ĳ���ջ������B��Ų������Ĳ���ջ����
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
		// ��local a = not true�أ��˼ұ����ھ͸��㴦�����
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

	  // ���ܣ�ʵ���ַ���ƴ������� ..
	  // iABC��Aƴ�Ӻ��Ž���Ĳ���ջ������B��һ��Ҫƴ�ӵı����Ĳ���ջ������C���һ��Ҫƴ�ӵı����Ĳ���ջ����
	  // Ҫִ�����ָ��Բ���ջ������Ҫ��
	  // ... -> string1 -> string2 ... -> stringN -> ...
	  //          ^                          ^
	  //          RB                         RC
      case OP_CONCAT: {
		// ����OP_LOADNIL��ֻ��������η�Χ��[rb,rc]��loadnil��[ra,rb]
		// local b, c, d, a = "hello", "world", "!"
		// a = b .. c .. d -----> index(a) = 4, index(b~d) = 1~3
		//                 >----> OP_CONCAT 4 1 3
		// ���������b~d���ܱ�֤����������ô�죿����һ����MOVE��ȥ��OP_CONCAT...
        int b = GETARG_B(i);
        int c = GETARG_C(i);
        Protect(luaV_concat(L, c-b+1, c); luaC_checkGC(L));
        setobjs2s(L, RA(i), base+b);
        continue;
      }

	//---------------------------------------------------------------------------��תָ��
	  // ���ܣ���������ת
	  // iAsBx��A��ʹ�ã�sBx��תƫ��
	  // һ�������䲻�������֣�������һЩ���������к�������������תָ�����ʹ�õġ�
      case OP_JMP: {
		// ��������תָ�������תƫ������������ͷ���֮�ֵģ�������Ҫ�õ�
		// �������Ǿ�ֻ����iAsBx���͵�ָ���ˡ���sBx���г������Ƶģ�
		// ���ԣ�������ɵ�ָ��ܶ࣬������sBx�ĳ������ƣ����ܾͻ����ʧ��
        dojump(L, pc, GETARG_sBx(i));
        continue;
      }

	  // ���ܣ�������������Ƿ���ȣ�����Ԥ������ת�����OP_JMPʹ�á�
	  // iABC��A�����֣��ԱȽϽ����Ԥ�ڣ�����Ԥ������ת��B����1��������C����2������
      case OP_EQ: {
		// if a == b then -----> index(a) = 1, index(b) = 2
		//                >----> ���ｫ��������ָ�
		//                >----> OP_EQ  0 1 2	// ��0����Ϊ�����Ļ���������������ִ��
		//                >----> OP_JMP N		// N ��ʾthen ... end�м��ָ������
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
	
	  // ���ܣ�������������Ƿ�С�ڣ�����Ԥ������ת�����OP_JMPʹ�á�
	  // iABC��A�����֣��ԱȽϽ����Ԥ�ڣ�����Ԥ������ת��B����1��������C����2������
      case OP_LT: {
		// if a < b then -----> index(a) = 1, index(b) = 2
		//               >----> ���ｫ��������ָ�
		//               >----> OP_LT  0 1 2	// ��0����Ϊ�����Ļ���������������ִ��
		//               >----> OP_JMP N		// N ��ʾthen ... end�м��ָ������
		//               >----> ...				// Instructions between "then" and "end"
        Protect(
          if (luaV_lessthan(L, RKB(i), RKC(i)) == GETARG_A(i))
            dojump(L, pc, GETARG_sBx(*pc));
        )
        pc++;
        continue;
      }

	  // ���ܣ�������������Ƿ�С�ڵ��ڣ�����Ԥ������ת�����OP_JMPʹ�á�
	  // iABC��A�����֣��ԱȽϽ����Ԥ�ڣ�����Ԥ������ת��B����1��������C����2������
      case OP_LE: {
		  // if a <= b then -----> index(a) = 1, index(b) = 2
		  //                >----> ���ｫ��������ָ�
		  //                >----> OP_LE  0 1 2	// ��0����Ϊ�����Ļ���������������ִ��
		  //                >----> OP_JMP N		// N ��ʾthen ... end�м��ָ������
		  //                >----> ...			// Instructions between "then" and "end"
		  Protect(
          if (lessequal(L, RKB(i), RKC(i)) == GETARG_A(i))
            dojump(L, pc, GETARG_sBx(*pc));
        )
        pc++;
        continue;
      }

	  // ���ܣ����ĳ�������Ĳ���ֵ����Ԥ����ͬ����ת�����OP_JMPʹ�á�
	  // iABC��A�����ڲ���ջ�ϵ�������C�����֣���ʾ����A�Ĳ���ֵԤ��
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
      case OP_TESTSET: {// TODO: û����������������...
        TValue *rb = RB(i);
        if (l_isfalse(rb) != GETARG_C(i)) {
          setobjs2s(L, ra, rb);
          dojump(L, pc, GETARG_sBx(*pc));
        }
        pc++;
        continue;
      }

	  // ���ܣ�����ĳ��Closure����
	  // iABC��AClosure�������ڵĲ���ջ������B�����������+1��C�ڴ����ص�����+1
	  // Ҫִ�����ָ��Բ���ջ������Ҫ��
	  // ... -> Closure -> param1 -> param2 ... -> paramN -> ...
	  //           ^         |___________ ___________|
	  //           |                     V
	  //           RA                   RB-1
	  // ע�⣬���û�����RA -> ... -> RA+RC-2�ϣ�������Closure���ص����ݣ������Ļ���nil
      case OP_CALL: {		// 
		// local a = foo(1,2,3)
		// ���÷����ˣ�ֱ����luac�õ����ֽ���
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

	  // ���ܣ������á�ĳ��Closure����������ɵ��ú�����ǰstack frame��
	  // iABC��AClosure�������ڵĲ���ջ������B�����������+1��C��Զ��0
	  // ע�⣺���ȣ����ָ���ǻ�����������õģ�����Ҳ��������OP_TAILCALL�Ͳ���Ҫһ��OP_CALL�����ʹ���ˡ�
	  //      ��Σ�����Ҫһ��OP_RETURN�����ʹ�ã���Ϊ���Ѿ��ǵ�ǰ���������һ������ˣ�
      case OP_TAILCALL: {	// iABC��Alua������B�����������+1��C�ڴ����ص�����+1
		// function f(n) 
		//   return f(n-1)
		// end
		// ���ɣ�   GETGLOBAL 1, -1 -- index(constant("f")) = -1
		//         SUB 2, 0, -2		-- index(constant(-1)) = -2
		//         TAILCALL 1, 2, 0
		//         RETURN 1, 0
		//         RETURN 0, 1
        int b = GETARG_B(i);
        if (b != 0) L->top = ra+b;  /* else previous instruction set top */
        L->savedpc = pc;
        lua_assert(GETARG_C(i) - 1 == LUA_MULTRET);
        switch (luaD_precall(L, ra, LUA_MULTRET)) {	// ע�⣬���������luaD_precall
          case PCRLUA: {
            /* tail call: put new frame in place of previous one */
			// luaD_precall����� ++L->ci ����������TAILCALL���ڵ�callinfo��L->ci - 1 [ldo.c: now `enter' new function]
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

	  // ���ܣ�������ǰ����
      // iABC: A��һ������ֵ��ջ���� B����ֵ������+1
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

	  // ���ܣ�forѭ����ʹ���� for+step �﷨���� for i=1,100 do sum = sum+i end
	  // iAsBx��Aѭ��������ʼֵ sBxָ����תƫ��
	  // Ҫִ�����ָ���ջ�������������󡣼���for���Ϊ for variable=init,ended,step��
	  //   ... -> init -> ended -> step -> loopv -> ...
	  //           ^                         ^
	  //       RA is here               use in loop block
	  // ���԰�����ѭ�������޸�ѭ��������ʱ�򣬲����ѭ����������κ�Ӱ��
      case OP_FORLOOP: {
        lua_Number step = nvalue(ra+2);
        lua_Number idx = luai_numadd(nvalue(ra), step); // ����FORLOOP��FORPREP�����ʹ�õģ�FORPREP����һ�μ���������������һ�μӷ�
        lua_Number limit = nvalue(ra+1);
        if (luai_numlt(0, step) ? luai_numle(idx, limit)
                                : luai_numle(limit, idx)) {
          dojump(L, pc, GETARG_sBx(i));  /* jump back */
          setnvalue(ra, idx);  /* update internal index... */
          setnvalue(ra+3, idx);  /* ...and external index */
        }
        continue;
      }

	  // ���ܣ���OP_FORLOOP���ʹ�á���ִ��һ��FORPREP����ִ��FORLOOP
	  // ���� FORPREP ��Ҫ����֤����û���⡣
	  // iAsBx��AͬOP_FORLOOP��sBx��Ӧ��OP_FORLOOP��ָ��ƫ��
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
		//����ļ�������Ϊ����ԣ�����֤��FORLOOP�еļӷ���������Ҫ����һ��if��䣡
		//Ҫ֪��FORLOOP��ÿ��ѭ����ִ��һ�εģ���һ������������һ��ʱ��
        setnvalue(ra, luai_numsub(nvalue(ra), nvalue(pstep)));
        dojump(L, pc, GETARG_sBx(i));
        continue;
      }
	  
	  // ���ܣ�forѭ�������� for+������ �ķ��������ָ��û��TFORPREP���ף�Ҫ��FORLOOP����
	  // ���ȣ���ϰһ�µ�������Ҫ��
	  // for k, v in f( container ) do ... end
	  // ���У�f��Ϊ iterator generator�������᷵������ֵ��iterator, temporary_container, iterator_key
	  // �˺�iterator generator�����ٱ����á�ÿ��ѭ��ֻ�ǵ��� iterator( temporary_container, iterator_key )
	  // ���ѵ��õķ���ֵ��ֵ�� k, v �ϣ��Ҹ��� iterator_key = k
	  // Ҫ������ָ���Ҫ�������������õ�������ָ�����е������ģ�
	  //     �������ú�ջ�� ... -> f -> container -> ...
	  //     Ȼ�����f��    CALL index(f), 2, 4   -- ������container��ֻ��һ��������B=2������3��ֵ��������4
	  //     Ȼ����ת��     JMP  offset(TFORLOOP)
	  //     �м���ѭ���壺 ...                   -- ÿ��ѭ��ִ�е�ָ��
	  //     �����ǵ�����   TFORLOOP index(f)     -- offset(TFORLOOP)ָ�ľ�������
	  //     �������ת��   JMP  offset(...)      -- ָʾ��ת��ȥ��λ��
	  // ���f������3��ֵ���Ǿ��ǣ� ... -> iterator[ԭ����index(f)] -> temporary_container -> iterator_key -> ...
	  //                                      ^
	  //                                      RA
	  // iABC��Aiterator generator�ڲ���ջ������
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
		  // cb-1,�������iterator_key��luaD_call֮��cb����ľ���k���������������� iterator_key = k
          setobjs2s(L, cb-1, cb);  /* save control variable */
          dojump(L, pc, GETARG_sBx(*pc));  /* jump back */
        }
        pc++;
        continue;
      }

	  // ������������ջ��ָ��λ�ý�ȡһ��lua_TValue�����浽ָ����Table�е����鲿��
	  // ���� memcpy(&RA[RC], &localvar, B-1)
	  // iABC��AĿ��table����, B���鳤��, C�����A��ƫ��
	  // Ҫִ�����ָ�ջ��ʵ���ڷ���Ҫ����(localval����RA)��
	  //      Target -> localvar1 -> localvar2 ...
	  // ���ָ������϶�����table�ĳ�ʼ����䡣��
	  // a = {1,2,3,'a'}
      case OP_SETLIST: {
        int n = GETARG_B(i);
        int c = GETARG_C(i);
        int last;
        Table *h;
		// ֻ���ں�OP_VARARG���õ�ʱ�򣬲Ż���RB=0
        if (n == 0) {
          n = cast_int(L->top - ra) - 1;
          L->top = L->ci->top;
        }
        if (c == 0) c = cast_int(*pc++);	// Ӧ���Ǹ����⴦����rc�Ų��£��ͷ�����һ��ָ����
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

	  // �������ܣ�û�ҵ��������������ָ��
      case OP_CLOSE: {
        luaF_close(L, ra);
        continue;
      }
	  
	  // ���ܣ���ջ��ָ��λ�ô���һ��Closure����Ҫ֪����Closure�����ǿɻ��յ�
	  // Ҫ����һ��Closure������Ҫ�����ݰ���
	  // 1 ����ԭ��(Protoʵ��)
	  // 2 ������
	  // 3 upvalue����
	  // iABx��AҪ������Closure�����λ�ã�Bx����ԭ���ڵ�ǰClosure�е�����
	  // ���ָ�����һϵ�еĸ���ָ����ڳ�ʼ��upvalues��ÿ��ָ���ʼ��һ��upvalue��
	  // ��Щ��������ʹ��luac -l��ʱ����Բ鿴��Ӧ��function���������� `1 upvalues` ����������
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

	  // ���ܣ�����memcpy(RA, &varg[0], RB-1)�������ɱ�����б�ջ��ָ��λ�ò��������
	  // iABC��Aָ������λ�ã�B����������+1������B=0��ʾ#{...}
      case OP_VARARG: {
		// �����Ļ������� local a, b, c = ... �����Ĵ��룬ֻҪ��֤ index(a)~index(c)��
		// ���������ջ�ϵģ��ǾͿ���һ��ָ��㶨
		// ��Ȼ������������Ĵ��룺
		// local a, b, c, d
		// b, d = ...
		// �ǾͲ���һ��ָ��㶨�ˣ�Ҫ��VARARG�����Ȼ���� OP_MOVE��index(b)���ٴ�OP_MOVE��index(d)
		// ��ô����ȻΪ��ִ���ٶȣ�ǧ���д������ûЧ�ʵ��������룡����

		// ��ldo�ĺ���adjust_varargs�У���һ������ĵط���
		// �ں�����󣬵���stack frame������ʽ(nargs��ʾ�������������nparam��ʾ����������������)
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
		// ci->base - ci->func: �õ� `�����������`
		// ���� n = #{...}
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
		// �����ϣ�ִ�е����stack frame����
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
		// ������������û��ô������
        continue;
      }
    }
  }
}

