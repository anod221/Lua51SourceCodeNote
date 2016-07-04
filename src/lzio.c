/*
** $Id: lzio.c,v 1.31 2005/06/03 20:15:29 roberto Exp $
** a generic input stream interface
** See Copyright Notice in lua.h
*/


#include <string.h>

#define lzio_c
#define LUA_CORE

#include "lua.h"

#include "llimits.h"
#include "lmem.h"
#include "lstate.h"
#include "lzio.h"

// [@lzio]: Zio方法，当Zio没有可读数据的时候调用，从reader读入更多数据后返回第一个byte数据
int luaZ_fill (ZIO *z) {
  size_t size;
  lua_State *L = z->L;
  const char *buff;
  lua_unlock(L);
  buff = z->reader(L, z->data, &size);
  lua_lock(L);
  if (buff == NULL || size == 0) return EOZ;
  z->n = size - 1;//[@lzio]：因为马上返回一个z->p++，所以可用数据要减去1
  z->p = buff;
  return char2int(*(z->p++));
}

// [@lzio]: ZIO方法，和peek差不多，返回当前可以读取的下一个字符值
int luaZ_lookahead (ZIO *z) {
  if (z->n == 0) {
    if (luaZ_fill(z) == EOZ)
      return EOZ;
    else {
      z->n++;  /* luaZ_fill removed first byte; put back it */
      z->p--;
    }
  }
  return char2int(*z->p);
}

// [@lzio]: ZIO方法，对实例z的各成员变量进行初始化赋值
void luaZ_init (lua_State *L, ZIO *z, lua_Reader reader, void *data) {
  z->L = L;
  z->reader = reader;
  z->data = data;
  z->n = 0;
  z->p = NULL;
}


// [@lzio]：ZIO方法，从ZIO当前游标处拷贝n字节数据到b的位置，相当于ByteArray.readBytes
// 返回：n - 已拷贝数据容量
size_t luaZ_read (ZIO *z, void *b, size_t n) {
  while (n) {
    size_t m;
    if (luaZ_lookahead(z) == EOZ)
      return n;  /* return number of missing bytes */
    m = (n <= z->n) ? n : z->n;  /* min. between n and z->n */
    memcpy(b, z->p, m);
    z->n -= m;
    z->p += m;
    b = (char *)b + m;
    n -= m;
  }
  return 0;
}

// [@lzio]: Mbuffer方法，确保buff的缓冲区容量不少于n
char *luaZ_openspace (lua_State *L, Mbuffer *buff, size_t n) {
  if (n > buff->buffsize) {
    if (n < LUA_MINBUFFER) n = LUA_MINBUFFER;
    luaZ_resizebuffer(L, buff, n);
  }
  return buff->buffer;
}


