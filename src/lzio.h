/*
** $Id: lzio.h,v 1.21 2005/05/17 19:49:15 roberto Exp $
** Buffered streams
** [@lzio]: 定义一套超级简单的 ByteArray 用于读写缓存
** See Copyright Notice in lua.h
*/


#ifndef lzio_h
#define lzio_h

#include "lua.h"

#include "lmem.h"


#define EOZ	(-1)			/* end of stream */

typedef struct Zio ZIO;		// [@lzio]: 一个糅杂了缓冲区游标和读取缓冲区用到的变量的结构

// [@lzio]: util方法，把char强转成为int，高位补0，导致char2int(-1) -> 0xff
#define char2int(c)	cast(int, cast(unsigned char, (c)))

// [@lzio]：ZIO方法，从Zio的实例z中读取一个char出来
#define zgetc(z)  (((z)->n--)>0 ?  char2int(*(z)->p++) : luaZ_fill(z))

typedef struct Mbuffer {	
  char *buffer;
  size_t n;
  size_t buffsize;
} Mbuffer;/* [@lzio]：ByteArray类似结构，记录一个缓冲区，容量和进度 */

// [@lzio]: Mbuffer方法，把实例buff的缓冲区指针清0，容量清0，L是不使用的变量
#define luaZ_initbuffer(L, buff) ((buff)->buffer = NULL, (buff)->buffsize = 0)

// [@lzio]: Mbuffer方法，get实例buff中的缓冲区指针
#define luaZ_buffer(buff)	((buff)->buffer)
// [@lzio]: Mbuffer方法，get实例buff中的容量
#define luaZ_sizebuffer(buff)	((buff)->buffsize)
// [@lzio]: Mbuffer方法，get实例buff中的进度
#define luaZ_bufflen(buff)	((buff)->n)

// [@lzio]: Mbuffer方法，实例buff进度清0
#define luaZ_resetbuffer(buff) ((buff)->n = 0)

// [@lzio]: Mbuffer方法，为实例buff的缓冲区重新分配容量，容量值为size
#define luaZ_resizebuffer(L, buff, size) \
	(luaM_reallocvector(L, (buff)->buffer, (buff)->buffsize, size, char), \
	(buff)->buffsize = size)

// [@lzio]: Mbuffer方法，把实例buff的缓冲区释放后清0，容量清0
#define luaZ_freebuffer(L, buff)	luaZ_resizebuffer(L, buff, 0)

// [@lzio]: Mbuffer方法，确保buff的缓冲区容量不少于n
LUAI_FUNC char *luaZ_openspace (lua_State *L, Mbuffer *buff, size_t n);
// [@lzio]: ZIO方法，对实例z的各成员变量进行初始化赋值
LUAI_FUNC void luaZ_init (lua_State *L, ZIO *z, lua_Reader reader,
                                        void *data);
// [@lzio]：ZIO方法，从ZIO当前游标处拷贝n字节数据到b的位置，相当于ByteArray.readBytes
// 返回：n - 已拷贝数据容量
LUAI_FUNC size_t luaZ_read (ZIO* z, void* b, size_t n);	
// [@lzio]: ZIO方法，和peek差不多，返回当前可以读取的下一个字符值
LUAI_FUNC int luaZ_lookahead (ZIO *z);



/* --------- Private Part ------------------ */

struct Zio {			// [@lzio]: 一个糅杂了缓冲区游标和读取缓冲区用到的变量的结构
  size_t n;				/* [@lzio]: 相当于ByteArray的成员变量bytesAvailable */
  const char *p;		/* [@lzio]: 类似于ByteArray的成员变量position */
  //[@lzio]: 用于读入更多数据用的函数
  // 原型：
  // const char* reader(lua_State *L, void* userdata, OUTPUT size_t *psize)
  // 参数：
  // userdata: 自定义数据结构
  // psize: 读入了多少字节，写入到这个变量中
  // 返回：
  // 读入这些字节后，存放的缓冲区首地址
  lua_Reader reader;	
  void* data;			/* [@lzio]: 额外参数，用于传入给reader */
  lua_State *L;			/* [@lzio]: Lua虚拟机，用于传入给reader */
};

// [@lzio]: Zio方法，当Zio没有可读数据的时候调用，从reader读入更多数据后返回第一个byte数据
LUAI_FUNC int luaZ_fill (ZIO *z);

#endif
