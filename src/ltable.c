/*
** $Id: ltable.c,v 2.32 2006/01/18 11:49:02 roberto Exp $
** Lua tables (hash)
** See Copyright Notice in lua.h
*/


/*
** Implementation of tables (aka arrays, objects, or hash tables).
** Tables keep its elements in two parts: an array part and a hash part.
** Non-negative integer keys are all candidates to be kept in the array
** part. The actual size of the array is the largest `n' such that at
** least half the slots between 0 and n are in use.
** Hash uses a mix of chained scatter table with Brent's variation.
** A main invariant of these tables is that, if an element is not
** in its main position (i.e. the `original' position that its hash gives
** to it), then the colliding element is in its own main position.
** Hence even when the load factor reaches 100%, performance remains good.
*/

#include <math.h>
#include <string.h>

#define ltable_c
#define LUA_CORE

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "ltable.h"


/*
** max size of array part is 2^MAXBITS
*/
#if LUAI_BITSINT > 26
#define MAXBITS		26
#else
#define MAXBITS		(LUAI_BITSINT-2)
#endif

// [@ltable] array的最大size
#define MAXASIZE	(1 << MAXBITS)

// [@ltable] Table方法
// 获取Table实例t中哈希值n的Node对象
// 返回：Node*
#define hashpow2(t,n)      (gnode(t, lmod((n), sizenode(t))))
  
// [@ltable] Table方法
// 对str<TString*>进行哈希后取得其
// 在Table对象中的Node
// t Table*
// str TString*
// 返回：Node*
#define hashstr(t,str)  hashpow2(t, (str)->tsv.hash)
// [@ltable] Table方法
// 对布尔值进行哈希后取在Table中的Node
// t Table*
// p int
// 返回：Node*
#define hashboolean(t,p)        hashpow2(t, p)


/*
** for some types, it is better to avoid modulus by power of 2, as
** they tend to have many 2 factors.
*/
// [@ltable] Table方法
// see hashpow2
// 主要用在使用 hashpow2 冲突严重的类型上
// 目前主要用于指针的hash。因为一般指针习惯2,4,8字节对齐，模2^n容易冲突
#define hashmod(t,n)	(gnode(t, ((n) % ((sizenode(t)-1)|1))))

// [@ltable] Table方法
// 对某个指针进行哈希后获取在Table实例t的Node节点值
// t Table*
// p void*
// 返回：Node*
#define hashpointer(t,p)	hashmod(t, IntPoint(p))


/*
** number of ints inside a lua_Number
*/
// [@ltable] lua中一个数字占据的字节长度
#define numints		cast_int(sizeof(lua_Number)/sizeof(int))


// [@ltable] 占位符
#define dummynode		(&dummynode_)

static const Node dummynode_ = {
  {{NULL}, LUA_TNIL},  /* value */
  {{{NULL}, LUA_TNIL, NULL}}  /* key */
};


/*
** hash for lua_Numbers
*/
// [@ltable] Table方法
// 对某个数字进行哈希后获取在Table实例t的Node节点值
// t Table*
// n 浮点数
// 返回：Node*
static Node *hashnum (const Table *t, lua_Number n) {
  unsigned int a[numints];
  int i;
  n += 1;  /* normalize number (avoid -0) */
  lua_assert(sizeof(a) <= sizeof(n));
  memcpy(a, &n, sizeof(a));
  for (i = 1; i < numints; i++) a[0] += a[i];
  return hashmod(t, a[0]);
}



/*
** returns the `main' position of an element in a table (that is, the index
** of its hash value)
*/
// [@ltable] Table方法
// 根据key的类型对其哈希，返回哈希后对应的槽的Node
static Node *mainposition (const Table *t, const TValue *key) {
  switch (ttype(key)) {
    case LUA_TNUMBER:
      return hashnum(t, nvalue(key));
    case LUA_TSTRING:
      return hashstr(t, rawtsvalue(key));
    case LUA_TBOOLEAN:
      return hashboolean(t, bvalue(key));
    case LUA_TLIGHTUSERDATA:
      return hashpointer(t, pvalue(key));
    default:
      return hashpointer(t, gcvalue(key));
  }
}


/*
** returns the index for `key' if `key' is an appropriate key to live in
** the array part of the table, -1 otherwise.
*/
// [@ltable] 辅助函数
// 如果key是“整数”则返回这个整数值否则返回-1
static int arrayindex (const TValue *key) {
  if (ttisnumber(key)) {
    lua_Number n = nvalue(key);
    int k;
    lua_number2int(k, n);
    if (luai_numeq(cast_num(k), n))
      return k;
  }
  return -1;  /* `key' did not match some condition */
}


/*
** returns the index of a `key' for table traversals. First goes all
** elements in the array part, then elements in the hash part. The
** beginning of a traversal is signalled by -1.
*/
// [@ltable] 辅助方法
// 从Table对象t中找到对应key的“下标”
// 如果key是位于array部分，则返回array的下标
// 如果key是位于hash部分，则返回 hash部分Node的下标，当然为了和array下标区分，还要
// 加上sizearray作为偏移值
static int findindex (lua_State *L, Table *t, StkId key) {
  int i;
  if (ttisnil(key)) return -1;  /* first iteration */
  i = arrayindex(key);
  if (0 < i && i <= t->sizearray)  /* is `key' inside array part? */
    return i-1;  /* yes; that's the index (corrected to C) */
  else {
    Node *n = mainposition(t, key);
    do {  /* check whether `key' is somewhere in the chain */
      /* key may be dead already, but it is ok to use it in `next' */
      if (luaO_rawequalObj(key2tval(n), key) ||
            (ttype(gkey(n)) == LUA_TDEADKEY && iscollectable(key) &&
             gcvalue(gkey(n)) == gcvalue(key))) {
        i = cast_int(n - gnode(t, 0));  /* key index in hash table */
        /* hash elements are numbered after array ones */
        return i + t->sizearray;
      }
      else n = gnext(n);
    } while (n);
    luaG_runerror(L, "invalid key to " LUA_QL("next"));  /* key not found */
    return 0;  /* to avoid warnings */
  }
}

// [@ltable] Table方法
// 找到某个key的后继节点
// 返回：1找到了后继节点 0没找到后继节点
// L lua_State对象
// t 需要查找的表项
// key 输出容器，除了key，还有一个隐藏容器key+1，下标输出到key中，而value则输出到key+1中
int luaH_next (lua_State *L, Table *t, StkId key) {
  int i = findindex(L, t, key);  /* find original element */
  for (i++; i < t->sizearray; i++) {  /* try first array part */
    if (!ttisnil(&t->array[i])) {  /* a non-nil value? */
      setnvalue(key, cast_num(i+1));
      setobj2s(L, key+1, &t->array[i]);
      return 1;
    }
  }
  for (i -= t->sizearray; i < sizenode(t); i++) {  /* then hash part */
    if (!ttisnil(gval(gnode(t, i)))) {  /* a non-nil value? */
      setobj2s(L, key, key2tval(gnode(t, i)));
      setobj2s(L, key+1, gval(gnode(t, i)));
      return 1;
    }
  }
  return 0;  /* no more elements */
}


/*
** {=============================================================
** Rehash
** ==============================================================
*/

// [@table] 辅助函数
// 为Table计算一个合适的array部分的容量大小
// “合适”的量化条件：容量n能容纳超过当前全部下标的一半
// 返回：最终将放置到array部分的元素数量
// nums 记录有array部分每个阶段放了多少个元素的黑板
// narray out变量，输出合适的array部分容量大小到这里
static int computesizes (int nums[], int *narray) {
  int i;
  int twotoi;  /* 2^i */
  int a = 0;  /* number of elements smaller than 2^i */
  int na = 0;  /* number of elements to go to array part */
  int n = 0;  /* optimal size for array part */
  for (i = 0, twotoi = 1; twotoi/2 < *narray; i++, twotoi *= 2) {
    if (nums[i] > 0) {
      a += nums[i];
      if (a > twotoi/2) {  /* more than half elements present? */
        n = twotoi;  /* optimal size (till now) */
        na = a;  /* all elements smaller than n will go to array part */
      }
    }
    if (a == *narray) break;  /* all elements already counted */
  }
  *narray = n;
  lua_assert(*narray/2 <= na && na <= *narray);
  return na;
}

// [@ltable] 辅助函数
// 检查key能不能当合法数组下标，并更新到nums
// 返回：key能当下标返回1否则返回0
// key 待检查的TValue对象
// nums 作为输出的黑板使用。下标n数组部分下标为2^(n-1)~2^n中非空元素个数
static int countint (const TValue *key, int *nums) {
  int k = arrayindex(key);
  if (0 < k && k <= MAXASIZE) {  /* is `key' an appropriate array index? */
    nums[ceillog2(k)]++;  /* count as such */
    return 1;
  }
  else
    return 0;
}

// [@ltable] 辅助函数
// 计算Table实例t中的array部分的非空元素个数
// 返回：t的数组部分value非空元素个数
// nums：作为输出的黑板使用。下标n对应t的数组部分下标为2^(n-1)~2^n中非空元素个数
static int numusearray (const Table *t, int *nums) {
  int lg;
  int ttlg;  /* 2^lg */
  int ause = 0;  /* summation of `nums' */
  int i = 1;  /* count to traverse all array keys */
  for (lg=0, ttlg=1; lg<=MAXBITS; lg++, ttlg*=2) {  /* for each slice */
    int lc = 0;  /* counter */
    int lim = ttlg;
    if (lim > t->sizearray) {
      lim = t->sizearray;  /* adjust upper limit */
      if (i > lim)
        break;  /* no more elements to count */
    }
    /* count elements in range (2^(lg-1), 2^lg] */
    for (; i <= lim; i++) {
      if (!ttisnil(&t->array[i-1]))
        lc++;
    }
    nums[lg] += lc;
    ause += lc;
  }
  return ause;
}

// [@ltable] 辅助函数
// 计算Table的hash部分的非空元素数目
// 返回：Table实例t的hash部分的非空元素数量
// t 要计算的Table
// nums 作为输出的黑板使用。下标n对应t的数组部分下标为2^(n-1)~2^n中非空元素个数
// pnasize 由于hash部分可能存在扩容后放入array部分的元素，最终输出合适的array部分的size到这里
static int numusehash (const Table *t, int *nums, int *pnasize) {
  int totaluse = 0;  /* total number of elements */
  int ause = 0;  /* summation of `nums' */
  int i = sizenode(t);
  while (i--) {
    Node *n = &t->node[i];
    if (!ttisnil(gval(n))) {
      ause += countint(key2tval(n), nums);
      totaluse++;
    }
  }
  *pnasize += ause;
  return totaluse;
}

// [@ltable] 在t->array上创建一块连续的
// 内存设为TValue，大小是size，并初始化为nil
static void setarrayvector (lua_State *L, Table *t, int size) {
  int i;
  luaM_reallocvector(L, t->array, t->sizearray, size, TValue);
  for (i=t->sizearray; i<size; i++)
     setnilvalue(&t->array[i]);
  t->sizearray = size;
}

// 为node分配size的空间
// node的空间是连续的，分配数量是2的幂
static void setnodevector (lua_State *L, Table *t, int size) {
  int lsize;
  if (size == 0) {  /* no elements to hash part? */
    t->node = cast(Node *, dummynode);  /* use common `dummynode' */
    lsize = 0;
  }
  else {
    int i;
    lsize = ceillog2(size);
    if (lsize > MAXBITS)
      luaG_runerror(L, "table overflow");
    size = twoto(lsize);
    t->node = luaM_newvector(L, size, Node);
    for (i=0; i<size; i++) {
      Node *n = gnode(t, i);
      gnext(n) = NULL;
      setnilvalue(gkey(n));
      setnilvalue(gval(n));
    }
  }
  t->lsizenode = cast_byte(lsize);
  t->lastfree = gnode(t, size);  /* all positions are free */
}

// [@ltable] Table方法，重新为t调整array/hash两个部分的容量
static void resize (lua_State *L, Table *t, int nasize, int nhsize) {
  int i;
  int oldasize = t->sizearray;
  int oldhsize = t->lsizenode;
  Node *nold = t->node;  /* save old hash ... */
  if (nasize > oldasize)  /* array part must grow? */
    setarrayvector(L, t, nasize);
  /* create new hash part with appropriate size */
  setnodevector(L, t, nhsize);  
  if (nasize < oldasize) {  /* array part must shrink? */
    t->sizearray = nasize;
    /* re-insert elements from vanishing slice */
    for (i=nasize; i<oldasize; i++) {
	  // 在这里把array中超出来array下标的部分挪到hash里面
	  // 至于为什么setnum是i+1，是因为lua的下标，数组部分是0开始对应
	  // lua中的1开始，而hash部分确是一一对应的
      if (!ttisnil(&t->array[i]))
        setobjt2t(L, luaH_setnum(L, t, i+1), &t->array[i]);
    }
    /* shrink array */
    luaM_reallocvector(L, t->array, oldasize, nasize, TValue);
  }
  /* re-insert elements from hash part */
  for (i = twoto(oldhsize) - 1; i >= 0; i--) {
    Node *old = nold+i;
    if (!ttisnil(gval(old)))
      setobjt2t(L, luaH_set(L, t, key2tval(old)), gval(old));
  }
  if (nold != dummynode)
    luaM_freearray(L, nold, twoto(oldhsize), Node);  /* free old array */
}

// [@ltable] Table方法
// 把Table实例t的数组部分扩容到nasize
void luaH_resizearray (lua_State *L, Table *t, int nasize) {
  int nsize = (t->node == dummynode) ? 0 : sizenode(t);
  resize(L, t, nasize, nsize);
}

// [@ltable] Table方法，为Table扩容
static void rehash (lua_State *L, Table *t, const TValue *ek) {
  int nasize, na;
  int nums[MAXBITS+1];  /* nums[i] = number of keys between 2^(i-1) and 2^i */
  int i;
  int totaluse;
  for (i=0; i<=MAXBITS; i++) nums[i] = 0;  /* reset counts */
  nasize = numusearray(t, nums);  /* count keys in array part */
  totaluse = nasize;  /* all those keys are integer keys */
  totaluse += numusehash(t, nums, &nasize);  /* count keys in hash part */
  /* count extra key */
  nasize += countint(ek, nums);
  totaluse++;
  /* compute new size for array part */
  na = computesizes(nums, &nasize);
  /* resize the table to new computed sizes */
  resize(L, t, nasize, totaluse - na);
}



/*
** }=============================================================
*/

// [@ltable] 创建一个table对象并返回
// narray 这个table对象的数组部分包含的元素数量
// nhash 这个table的k-v部分包含的元素数量
Table *luaH_new (lua_State *L, int narray, int nhash) {
  Table *t = luaM_new(L, Table);
  luaC_link(L, obj2gco(t), LUA_TTABLE);
  t->metatable = NULL;
  t->flags = cast_byte(~0);
  /* temporary values (kept only if some malloc fails) */
  t->array = NULL;
  t->sizearray = 0;
  t->lsizenode = 0;
  t->node = cast(Node *, dummynode);
  setarrayvector(L, t, narray);
  setnodevector(L, t, nhash);
  return t;
}

// [@ltable] 从vm中删除Table实例t
void luaH_free (lua_State *L, Table *t) {
  if (t->node != dummynode)
    luaM_freearray(L, t->node, sizenode(t), Node);
  luaM_freearray(L, t->array, t->sizearray, TValue);
  luaM_free(L, t);
}

// [@ltable] 从t.node中找到一个为nil的Node返回，同时更新lastfree目标
static Node *getfreepos (Table *t) {
  while (t->lastfree-- > t->node) {
    if (ttisnil(gkey(t->lastfree)))
      return t->lastfree;
  }
  return NULL;  /* could not find a free place */
}



/*
** inserts a new key into a hash table; first, check whether key's main 
** position is free. If not, check whether colliding node is in its main 
** position or not: if it is not, move colliding node to an empty place and 
** put new key in its main position; otherwise (colliding node is in its main 
** position), new key goes to an empty position. 
*/
// [@ltable] 辅助函数，整个ltable.c最重要的函数
// 从t中取出key对应的value的容器TValue*
// 返回：TValue*，这里的TValue*其实是作为容器使用的
// 返回值主要是使用 setobj 系列函数把TValue的值tt和value设置好
static TValue *newkey (lua_State *L, Table *t, const TValue *key) {
  // 算法解释：
  // 1 数据结构依然是stringtable的格式：哈希桶+冲突链表
  // 2 和stringtable不同的地方在于，stringtable冲突链表的内存是新分配的，
  //   node的数据是从桶里面的空位取的。取不到就重新分配
  
// 举个实际例子说明冲突链的处理方案：(例子中，{m:n}，m表示格子名字,n表示key的名字)
// 假设 a, b, c三个key的哈希值相同，都是1；而d, e这两个key的哈希值是2
// 有1，2，3，4，5这几个空格子
// 先放入 a，则a必然被放在格子1中 {1:a}
// 再放入 b，本来b应该放在1中的，但是1已经放了a了，所以选择空格2放置b。并且构造链 {1:a}->{2:b}
// 再放入 c，本来c应该放在1中的，但是1已经放了a了，所以选择空格3放置c。并且构造链 {1:a}->{3:c}->{2:b} (step8)
// 再放入 d，本来d应该放在2中的，但是2已经放了b了，所以 “要把{2:b}赶走！！！”。
// Q1：为什么要把{2:b}赶走？
// A1：因为b的哈希是1，它放在格子2这里只是因为格子2还没有找到它真正的主人！现在符合条件的主人d已经出现了，所以要驱逐b
// Q2：把b从2里面赶走了，那b放哪里？
// A2：只要还有空位，就随便找一个放着！！（吐槽：真随便！）
// Q3：对于某个格子，真正的主人需要满足什么条件？
// A3：只需要满足key的哈希值和格子的下标一致即可
// 所以选择空格4放置b，把空格2让出来放置d，最后构建链：{1:a}->{3:c}->{4:b}, {2:d}
// 再放入 e，本来e应该放在2中的，但是2已经放了d了，所以选择空格5放置e，并且构造链：{2:d}->{5:e}, {1:a}->{3:c}->{4:b}
// 这样就构造完成了

  // 注意：下方的步骤，step8是因，step9是果。所以需要先查看step8再看step9
  
  // step1 找到key应该放置在的格子mp
  Node *mp = mainposition(t, key);

  // step2 检查格子mp是否有东西了
  if (!ttisnil(gval(mp)) || mp == dummynode) {	
	// 格子mp有东西了，需要处理冲突链
    Node *othern;

	// step3 根据算法解释2，从桶中取出一个空位n
    Node *n = getfreepos(t);  

	// step4 取不到空位，桶满了，重新分配一个更大的桶处理
    if (n == NULL) {  
      rehash(L, t, key); 
      return luaH_set(L, t, key); 
    }
    lua_assert(n != dummynode);	

	// step5 因为格子mp有东西了，故找到格子mp原本将要放置的格子位置othern
    othern = mainposition(t, key2tval(mp));

	// step6 检查mp是不是被放置在它本应该放置的位置
    if (othern != mp) {
	  // step9<step8在下方分支> mp没有被放置在本应该放置的位置。
	  // 按照step8的方案反推，应当是 othern -> ... -> mp -> mp的后继
	  // 所以构建链：othern -> ... -> n -> mp的后继
	  // 并把mp的数据拷贝到n中，空出mp
      while (gnext(othern) != mp) othern = gnext(othern); 
      gnext(othern) = n;
      *n = *mp;
      gnext(mp) = NULL;
      setnilvalue(gval(mp));
    }
    else { 
      // step8 mp放置在了它应该出现的位置。所以构建链： mp -> n -> mp的后继
	  // 然后把key写入到n上
	  gnext(n) = gnext(mp);
      gnext(mp) = n;
      mp = n;
    }
  }

  // step10 写入key的数据，返回value容器
  gkey(mp)->value = key->value; gkey(mp)->tt = key->tt;
  luaC_barriert(L, t, key);
  lua_assert(ttisnil(gval(mp)));
  return gval(mp);
}


/*
** search function for integers
*/
// [@ltable] Table方法
// 从t中获取数组字段，下标是key
// 相当于代码
// function luaH_getnum( t, key )
//   assert( type(key)=="number" )
//   return reference( t[key] ) or nil
// end
const TValue *luaH_getnum (Table *t, int key) {
  /* (1 <= key && key <= t->sizearray) */
  if (cast(unsigned int, key-1) < cast(unsigned int, t->sizearray))
    return &t->array[key-1];
  else {
    lua_Number nk = cast_num(key);
    Node *n = hashnum(t, nk);
    do {  /* check whether `key' is somewhere in the chain */
      if (ttisnumber(gkey(n)) && luai_numeq(nvalue(gkey(n)), nk))
        return gval(n);  /* that's it */
      else n = gnext(n);
    } while (n);
    return luaO_nilobject;
  }
}


/*
** search function for strings
*/
// [@ltable] Table方法
// 从t中获取kv字段，key = key
// 相当于代码：
// function luaH_getstr( t, key ) 
//   assert( type(key)=="string" )
//   return reference( t[key] ) or nil
// end
const TValue *luaH_getstr (Table *t, TString *key) {
  Node *n = hashstr(t, key);
  do {  /* check whether `key' is somewhere in the chain */
    if (ttisstring(gkey(n)) && rawtsvalue(gkey(n)) == key)
      return gval(n);  /* that's it */
    else n = gnext(n);
  } while (n);
  return luaO_nilobject;
}


/*
** main search function
*/
// [@ltable] Table方法
// 从t中获取key对应的value
// 相当于代码：
// function luaH_get( t, key ) 
//   assert( type(key)=="anytype" )
//   return reference( t[key] ) or nil
// end
const TValue *luaH_get (Table *t, const TValue *key) {
  switch (ttype(key)) {
    case LUA_TNIL: return luaO_nilobject;
    case LUA_TSTRING: return luaH_getstr(t, rawtsvalue(key));
    case LUA_TNUMBER: {
      int k;
      lua_Number n = nvalue(key);
      lua_number2int(k, n);
      if (luai_numeq(cast_num(k), nvalue(key))) /* index is int? */
        return luaH_getnum(t, k);  /* use specialized version */
      /* else go through */
    }
    default: {
      Node *n = mainposition(t, key);
      do {  /* check whether `key' is somewhere in the chain */
        if (luaO_rawequalObj(key2tval(n), key))
          return gval(n);  /* that's it */
        else n = gnext(n);
      } while (n);
      return luaO_nilobject;
    }
  }
}

// [@ltable] Table方法
// 从t中获取key对应的value，如果key不存在就创建
// 相当于代码：
// function luaH_set( t, key ) 
//   assert( type(key)~="nil" and key~=NaN )
//   if t[key] == nil then
//     t[key] = new_TValue()
//   end
//   return reference( t[key] )
// end
// luaH_set和luaH_get的不同在于，luaH_set会
// 对key做检查，在key为nil或者nan时候抛出异常
// 并且保证索引不存在的key时会创建新的，保证不会
// 出现返回luaO_nilobject的情况
TValue *luaH_set (lua_State *L, Table *t, const TValue *key) {
  const TValue *p = luaH_get(t, key);
  t->flags = 0;
  if (p != luaO_nilobject)
    return cast(TValue *, p);
  else {
    if (ttisnil(key)) luaG_runerror(L, "table index is nil");
    else if (ttisnumber(key) && luai_numisnan(nvalue(key)))
      luaG_runerror(L, "table index is NaN");
    return newkey(L, t, key);
  }
}

// [@ltable] Table方法
// 从t中获取数组字段，下标是key，如果key不存在就创建
// 相当于代码
// function luaH_setnum( t, key )
//   assert( type(key)=="number" )
//   if t[key] == nil then
//     t[key] = new_TValue()
//   end
//   return reference( t[key] )
// end
TValue *luaH_setnum (lua_State *L, Table *t, int key) {
  const TValue *p = luaH_getnum(t, key);
  if (p != luaO_nilobject)
    return cast(TValue *, p);
  else {
    TValue k;
    setnvalue(&k, cast_num(key));
    return newkey(L, t, &k);
  }
}

// [@ltable] Table方法
// 从t中获取kv字段，key = key，如果key不存在就创建
// 相当于代码：
// function luaH_getstr( t, key ) 
//   assert( type(key)=="string" )
//   if t[key] == nil then
//     t[key] = new_TValue()
//   end
//   return reference( t[key] )
// end
TValue *luaH_setstr (lua_State *L, Table *t, TString *key) {
  const TValue *p = luaH_getstr(t, key);
  if (p != luaO_nilobject)
    return cast(TValue *, p);
  else {
    TValue k;
    setsvalue(L, &k, key);
    return newkey(L, t, &k);
  }
}


static int unbound_search (Table *t, unsigned int j) {
  unsigned int i = j;  /* i is zero or a present index */
  j++;
  /* find `i' and `j' such that i is present and j is not */
  while (!ttisnil(luaH_getnum(t, j))) {
    i = j;
    j *= 2;
    if (j > cast(unsigned int, MAX_INT)) {  /* overflow? */
      /* table was built with bad purposes: resort to linear search */
      i = 1;
      while (!ttisnil(luaH_getnum(t, i))) i++;
      return i - 1;
    }
  }
  /* now do a binary search between them */
  while (j - i > 1) {
    unsigned int m = (i+j)/2;
    if (ttisnil(luaH_getnum(t, m))) j = m;
    else i = m;
  }
  return i;
}


/*
** Try to find a boundary in table `t'. A `boundary' is an integer index
** such that t[i] is non-nil and t[i+1] is nil (and 0 if t[1] is nil).
*/
int luaH_getn (Table *t) {
  unsigned int j = t->sizearray;
  if (j > 0 && ttisnil(&t->array[j - 1])) {
    /* there is a boundary in the array part: (binary) search for it */
    unsigned int i = 0;
    while (j - i > 1) {
      unsigned int m = (i+j)/2;
      if (ttisnil(&t->array[m - 1])) j = m;
      else i = m;
    }
    return i;
  }
  /* else must find a boundary in hash part */
  else if (t->node == dummynode)  /* hash part is empty? */
    return j;  /* that is easy... */
  else return unbound_search(t, j);
}



#if defined(LUA_DEBUG)

Node *luaH_mainposition (const Table *t, const TValue *key) {
  return mainposition(t, key);
}

int luaH_isdummy (Node *n) { return n == dummynode; }

#endif
