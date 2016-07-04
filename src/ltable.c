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

// [@ltable] array�����size
#define MAXASIZE	(1 << MAXBITS)

// [@ltable] Table����
// ��ȡTableʵ��t�й�ϣֵn��Node����
// ���أ�Node*
#define hashpow2(t,n)      (gnode(t, lmod((n), sizenode(t))))
  
// [@ltable] Table����
// ��str<TString*>���й�ϣ��ȡ����
// ��Table�����е�Node
// t Table*
// str TString*
// ���أ�Node*
#define hashstr(t,str)  hashpow2(t, (str)->tsv.hash)
// [@ltable] Table����
// �Բ���ֵ���й�ϣ��ȡ��Table�е�Node
// t Table*
// p int
// ���أ�Node*
#define hashboolean(t,p)        hashpow2(t, p)


/*
** for some types, it is better to avoid modulus by power of 2, as
** they tend to have many 2 factors.
*/
// [@ltable] Table����
// see hashpow2
// ��Ҫ����ʹ�� hashpow2 ��ͻ���ص�������
// Ŀǰ��Ҫ����ָ���hash����Ϊһ��ָ��ϰ��2,4,8�ֽڶ��룬ģ2^n���׳�ͻ
#define hashmod(t,n)	(gnode(t, ((n) % ((sizenode(t)-1)|1))))

// [@ltable] Table����
// ��ĳ��ָ����й�ϣ���ȡ��Tableʵ��t��Node�ڵ�ֵ
// t Table*
// p void*
// ���أ�Node*
#define hashpointer(t,p)	hashmod(t, IntPoint(p))


/*
** number of ints inside a lua_Number
*/
// [@ltable] lua��һ������ռ�ݵ��ֽڳ���
#define numints		cast_int(sizeof(lua_Number)/sizeof(int))


// [@ltable] ռλ��
#define dummynode		(&dummynode_)

static const Node dummynode_ = {
  {{NULL}, LUA_TNIL},  /* value */
  {{{NULL}, LUA_TNIL, NULL}}  /* key */
};


/*
** hash for lua_Numbers
*/
// [@ltable] Table����
// ��ĳ�����ֽ��й�ϣ���ȡ��Tableʵ��t��Node�ڵ�ֵ
// t Table*
// n ������
// ���أ�Node*
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
// [@ltable] Table����
// ����key�����Ͷ����ϣ�����ع�ϣ���Ӧ�Ĳ۵�Node
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
// [@ltable] ��������
// ���key�ǡ��������򷵻��������ֵ���򷵻�-1
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
// [@ltable] ��������
// ��Table����t���ҵ���Ӧkey�ġ��±ꡱ
// ���key��λ��array���֣��򷵻�array���±�
// ���key��λ��hash���֣��򷵻� hash����Node���±꣬��ȻΪ�˺�array�±����֣���Ҫ
// ����sizearray��Ϊƫ��ֵ
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

// [@ltable] Table����
// �ҵ�ĳ��key�ĺ�̽ڵ�
// ���أ�1�ҵ��˺�̽ڵ� 0û�ҵ���̽ڵ�
// L lua_State����
// t ��Ҫ���ҵı���
// key �������������key������һ����������key+1���±������key�У���value�������key+1��
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

// [@table] ��������
// ΪTable����һ�����ʵ�array���ֵ�������С
// �����ʡ�����������������n�����ɳ�����ǰȫ���±��һ��
// ���أ����ս����õ�array���ֵ�Ԫ������
// nums ��¼��array����ÿ���׶η��˶��ٸ�Ԫ�صĺڰ�
// narray out������������ʵ�array����������С������
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

// [@ltable] ��������
// ���key�ܲ��ܵ��Ϸ������±꣬�����µ�nums
// ���أ�key�ܵ��±귵��1���򷵻�0
// key ������TValue����
// nums ��Ϊ����ĺڰ�ʹ�á��±�n���鲿���±�Ϊ2^(n-1)~2^n�зǿ�Ԫ�ظ���
static int countint (const TValue *key, int *nums) {
  int k = arrayindex(key);
  if (0 < k && k <= MAXASIZE) {  /* is `key' an appropriate array index? */
    nums[ceillog2(k)]++;  /* count as such */
    return 1;
  }
  else
    return 0;
}

// [@ltable] ��������
// ����Tableʵ��t�е�array���ֵķǿ�Ԫ�ظ���
// ���أ�t�����鲿��value�ǿ�Ԫ�ظ���
// nums����Ϊ����ĺڰ�ʹ�á��±�n��Ӧt�����鲿���±�Ϊ2^(n-1)~2^n�зǿ�Ԫ�ظ���
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

// [@ltable] ��������
// ����Table��hash���ֵķǿ�Ԫ����Ŀ
// ���أ�Tableʵ��t��hash���ֵķǿ�Ԫ������
// t Ҫ�����Table
// nums ��Ϊ����ĺڰ�ʹ�á��±�n��Ӧt�����鲿���±�Ϊ2^(n-1)~2^n�зǿ�Ԫ�ظ���
// pnasize ����hash���ֿ��ܴ������ݺ����array���ֵ�Ԫ�أ�����������ʵ�array���ֵ�size������
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

// [@ltable] ��t->array�ϴ���һ��������
// �ڴ���ΪTValue����С��size������ʼ��Ϊnil
static void setarrayvector (lua_State *L, Table *t, int size) {
  int i;
  luaM_reallocvector(L, t->array, t->sizearray, size, TValue);
  for (i=t->sizearray; i<size; i++)
     setnilvalue(&t->array[i]);
  t->sizearray = size;
}

// Ϊnode����size�Ŀռ�
// node�Ŀռ��������ģ�����������2����
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

// [@ltable] Table����������Ϊt����array/hash�������ֵ�����
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
	  // �������array�г�����array�±�Ĳ���Ų��hash����
	  // ����Ϊʲôsetnum��i+1������Ϊlua���±꣬���鲿����0��ʼ��Ӧ
	  // lua�е�1��ʼ����hash����ȷ��һһ��Ӧ��
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

// [@ltable] Table����
// ��Tableʵ��t�����鲿�����ݵ�nasize
void luaH_resizearray (lua_State *L, Table *t, int nasize) {
  int nsize = (t->node == dummynode) ? 0 : sizenode(t);
  resize(L, t, nasize, nsize);
}

// [@ltable] Table������ΪTable����
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

// [@ltable] ����һ��table���󲢷���
// narray ���table��������鲿�ְ�����Ԫ������
// nhash ���table��k-v���ְ�����Ԫ������
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

// [@ltable] ��vm��ɾ��Tableʵ��t
void luaH_free (lua_State *L, Table *t) {
  if (t->node != dummynode)
    luaM_freearray(L, t->node, sizenode(t), Node);
  luaM_freearray(L, t->array, t->sizearray, TValue);
  luaM_free(L, t);
}

// [@ltable] ��t.node���ҵ�һ��Ϊnil��Node���أ�ͬʱ����lastfreeĿ��
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
// [@ltable] ��������������ltable.c����Ҫ�ĺ���
// ��t��ȡ��key��Ӧ��value������TValue*
// ���أ�TValue*�������TValue*��ʵ����Ϊ����ʹ�õ�
// ����ֵ��Ҫ��ʹ�� setobj ϵ�к�����TValue��ֵtt��value���ú�
static TValue *newkey (lua_State *L, Table *t, const TValue *key) {
  // �㷨���ͣ�
  // 1 ���ݽṹ��Ȼ��stringtable�ĸ�ʽ����ϣͰ+��ͻ����
  // 2 ��stringtable��ͬ�ĵط����ڣ�stringtable��ͻ������ڴ����·���ģ�
  //   node�������Ǵ�Ͱ����Ŀ�λȡ�ġ�ȡ���������·���
  
// �ٸ�ʵ������˵����ͻ���Ĵ�������(�����У�{m:n}��m��ʾ��������,n��ʾkey������)
// ���� a, b, c����key�Ĺ�ϣֵ��ͬ������1����d, e������key�Ĺ�ϣֵ��2
// ��1��2��3��4��5�⼸���ո���
// �ȷ��� a����a��Ȼ�����ڸ���1�� {1:a}
// �ٷ��� b������bӦ�÷���1�еģ�����1�Ѿ�����a�ˣ�����ѡ��ո�2����b�����ҹ����� {1:a}->{2:b}
// �ٷ��� c������cӦ�÷���1�еģ�����1�Ѿ�����a�ˣ�����ѡ��ո�3����c�����ҹ����� {1:a}->{3:c}->{2:b} (step8)
// �ٷ��� d������dӦ�÷���2�еģ�����2�Ѿ�����b�ˣ����� ��Ҫ��{2:b}���ߣ���������
// Q1��ΪʲôҪ��{2:b}���ߣ�
// A1����Ϊb�Ĺ�ϣ��1�������ڸ���2����ֻ����Ϊ����2��û���ҵ������������ˣ����ڷ�������������d�Ѿ������ˣ�����Ҫ����b
// Q2����b��2��������ˣ���b�����
// A2��ֻҪ���п�λ���������һ�����ţ������²ۣ�����㣡��
// Q3������ĳ�����ӣ�������������Ҫ����ʲô������
// A3��ֻ��Ҫ����key�Ĺ�ϣֵ�͸��ӵ��±�һ�¼���
// ����ѡ��ո�4����b���ѿո�2�ó�������d����󹹽�����{1:a}->{3:c}->{4:b}, {2:d}
// �ٷ��� e������eӦ�÷���2�еģ�����2�Ѿ�����d�ˣ�����ѡ��ո�5����e�����ҹ�������{2:d}->{5:e}, {1:a}->{3:c}->{4:b}
// �����͹��������

  // ע�⣺�·��Ĳ��裬step8����step9�ǹ���������Ҫ�Ȳ鿴step8�ٿ�step9
  
  // step1 �ҵ�keyӦ�÷����ڵĸ���mp
  Node *mp = mainposition(t, key);

  // step2 ������mp�Ƿ��ж�����
  if (!ttisnil(gval(mp)) || mp == dummynode) {	
	// ����mp�ж����ˣ���Ҫ�����ͻ��
    Node *othern;

	// step3 �����㷨����2����Ͱ��ȡ��һ����λn
    Node *n = getfreepos(t);  

	// step4 ȡ������λ��Ͱ���ˣ����·���һ�������Ͱ����
    if (n == NULL) {  
      rehash(L, t, key); 
      return luaH_set(L, t, key); 
    }
    lua_assert(n != dummynode);	

	// step5 ��Ϊ����mp�ж����ˣ����ҵ�����mpԭ����Ҫ���õĸ���λ��othern
    othern = mainposition(t, key2tval(mp));

	// step6 ���mp�ǲ��Ǳ�����������Ӧ�÷��õ�λ��
    if (othern != mp) {
	  // step9<step8���·���֧> mpû�б������ڱ�Ӧ�÷��õ�λ�á�
	  // ����step8�ķ������ƣ�Ӧ���� othern -> ... -> mp -> mp�ĺ��
	  // ���Թ�������othern -> ... -> n -> mp�ĺ��
	  // ����mp�����ݿ�����n�У��ճ�mp
      while (gnext(othern) != mp) othern = gnext(othern); 
      gnext(othern) = n;
      *n = *mp;
      gnext(mp) = NULL;
      setnilvalue(gval(mp));
    }
    else { 
      // step8 mp����������Ӧ�ó��ֵ�λ�á����Թ������� mp -> n -> mp�ĺ��
	  // Ȼ���keyд�뵽n��
	  gnext(n) = gnext(mp);
      gnext(mp) = n;
      mp = n;
    }
  }

  // step10 д��key�����ݣ�����value����
  gkey(mp)->value = key->value; gkey(mp)->tt = key->tt;
  luaC_barriert(L, t, key);
  lua_assert(ttisnil(gval(mp)));
  return gval(mp);
}


/*
** search function for integers
*/
// [@ltable] Table����
// ��t�л�ȡ�����ֶΣ��±���key
// �൱�ڴ���
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
// [@ltable] Table����
// ��t�л�ȡkv�ֶΣ�key = key
// �൱�ڴ��룺
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
// [@ltable] Table����
// ��t�л�ȡkey��Ӧ��value
// �൱�ڴ��룺
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

// [@ltable] Table����
// ��t�л�ȡ�����ֶΣ��±���key�����key�����ھʹ���
// �൱�ڴ���
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

// [@ltable] Table����
// ��t�л�ȡkv�ֶΣ�key = key�����key�����ھʹ���
// �൱�ڴ��룺
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
