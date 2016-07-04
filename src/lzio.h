/*
** $Id: lzio.h,v 1.21 2005/05/17 19:49:15 roberto Exp $
** Buffered streams
** [@lzio]: ����һ�׳����򵥵� ByteArray ���ڶ�д����
** See Copyright Notice in lua.h
*/


#ifndef lzio_h
#define lzio_h

#include "lua.h"

#include "lmem.h"


#define EOZ	(-1)			/* end of stream */

typedef struct Zio ZIO;		// [@lzio]: һ�������˻������α�Ͷ�ȡ�������õ��ı����Ľṹ

// [@lzio]: util��������charǿת��Ϊint����λ��0������char2int(-1) -> 0xff
#define char2int(c)	cast(int, cast(unsigned char, (c)))

// [@lzio]��ZIO��������Zio��ʵ��z�ж�ȡһ��char����
#define zgetc(z)  (((z)->n--)>0 ?  char2int(*(z)->p++) : luaZ_fill(z))

typedef struct Mbuffer {	
  char *buffer;
  size_t n;
  size_t buffsize;
} Mbuffer;/* [@lzio]��ByteArray���ƽṹ����¼һ���������������ͽ��� */

// [@lzio]: Mbuffer��������ʵ��buff�Ļ�����ָ����0��������0��L�ǲ�ʹ�õı���
#define luaZ_initbuffer(L, buff) ((buff)->buffer = NULL, (buff)->buffsize = 0)

// [@lzio]: Mbuffer������getʵ��buff�еĻ�����ָ��
#define luaZ_buffer(buff)	((buff)->buffer)
// [@lzio]: Mbuffer������getʵ��buff�е�����
#define luaZ_sizebuffer(buff)	((buff)->buffsize)
// [@lzio]: Mbuffer������getʵ��buff�еĽ���
#define luaZ_bufflen(buff)	((buff)->n)

// [@lzio]: Mbuffer������ʵ��buff������0
#define luaZ_resetbuffer(buff) ((buff)->n = 0)

// [@lzio]: Mbuffer������Ϊʵ��buff�Ļ��������·�������������ֵΪsize
#define luaZ_resizebuffer(L, buff, size) \
	(luaM_reallocvector(L, (buff)->buffer, (buff)->buffsize, size, char), \
	(buff)->buffsize = size)

// [@lzio]: Mbuffer��������ʵ��buff�Ļ������ͷź���0��������0
#define luaZ_freebuffer(L, buff)	luaZ_resizebuffer(L, buff, 0)

// [@lzio]: Mbuffer������ȷ��buff�Ļ���������������n
LUAI_FUNC char *luaZ_openspace (lua_State *L, Mbuffer *buff, size_t n);
// [@lzio]: ZIO��������ʵ��z�ĸ���Ա�������г�ʼ����ֵ
LUAI_FUNC void luaZ_init (lua_State *L, ZIO *z, lua_Reader reader,
                                        void *data);
// [@lzio]��ZIO��������ZIO��ǰ�α괦����n�ֽ����ݵ�b��λ�ã��൱��ByteArray.readBytes
// ���أ�n - �ѿ�����������
LUAI_FUNC size_t luaZ_read (ZIO* z, void* b, size_t n);	
// [@lzio]: ZIO��������peek��࣬���ص�ǰ���Զ�ȡ����һ���ַ�ֵ
LUAI_FUNC int luaZ_lookahead (ZIO *z);



/* --------- Private Part ------------------ */

struct Zio {			// [@lzio]: һ�������˻������α�Ͷ�ȡ�������õ��ı����Ľṹ
  size_t n;				/* [@lzio]: �൱��ByteArray�ĳ�Ա����bytesAvailable */
  const char *p;		/* [@lzio]: ������ByteArray�ĳ�Ա����position */
  //[@lzio]: ���ڶ�����������õĺ���
  // ԭ�ͣ�
  // const char* reader(lua_State *L, void* userdata, OUTPUT size_t *psize)
  // ������
  // userdata: �Զ������ݽṹ
  // psize: �����˶����ֽڣ�д�뵽���������
  // ���أ�
  // ������Щ�ֽں󣬴�ŵĻ������׵�ַ
  lua_Reader reader;	
  void* data;			/* [@lzio]: ������������ڴ����reader */
  lua_State *L;			/* [@lzio]: Lua����������ڴ����reader */
};

// [@lzio]: Zio��������Zioû�пɶ����ݵ�ʱ����ã���reader����������ݺ󷵻ص�һ��byte����
LUAI_FUNC int luaZ_fill (ZIO *z);

#endif
