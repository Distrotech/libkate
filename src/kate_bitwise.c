/********************************************************************
 * This file was modified from the libogg distribution for          *
 * inclusion into libkate                                           *
 ********************************************************************/

/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2002             *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

  function: packing variable sized words into an octet stream
  last mod: $Id: kate_bitwise.c,v 1.3 2008/08/05 20:14:21 lyrian Exp $

 ********************************************************************/

/* We're 'LSb' endian; if we write a word but read individual bits,
   then we'll read the lsb first */

#define KATE_INTERNAL
#include "kate_internal.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include "kate_bitwise.h"

#define BUFFER_INCREMENT 256

static const unsigned long mask[]=
{0x00000000,0x00000001,0x00000003,0x00000007,0x0000000f,
 0x0000001f,0x0000003f,0x0000007f,0x000000ff,0x000001ff,
 0x000003ff,0x000007ff,0x00000fff,0x00001fff,0x00003fff,
 0x00007fff,0x0000ffff,0x0001ffff,0x0003ffff,0x0007ffff,
 0x000fffff,0x001fffff,0x003fffff,0x007fffff,0x00ffffff,
 0x01ffffff,0x03ffffff,0x07ffffff,0x0fffffff,0x1fffffff,
 0x3fffffff,0x7fffffff,0xffffffff };

void kate_pack_writeinit(kate_pack_buffer *b){
  memset(b,0,sizeof(*b));
  b->ptr=b->buffer=kate_malloc(BUFFER_INCREMENT);
  b->buffer[0]='\0';
  b->storage=BUFFER_INCREMENT;
}

void kate_pack_writetrunc(kate_pack_buffer *b,long bits){
  long bytes=bits>>3;
  bits-=bytes*8;
  b->ptr=b->buffer+bytes;
  b->endbit=bits;
  b->endbyte=bytes;
  *b->ptr&=mask[bits];
}

/* Takes only up to 32 bits. */
void kate_pack_write(kate_pack_buffer *b,unsigned long value,int bits){
  if(b->endbyte+4>=b->storage){
    b->buffer=kate_realloc(b->buffer,b->storage+BUFFER_INCREMENT);
    b->storage+=BUFFER_INCREMENT;
    b->ptr=b->buffer+b->endbyte;
  }

  value&=mask[bits]; 
  bits+=b->endbit;

  b->ptr[0]|=value<<b->endbit;  
  
  if(bits>=8){
    b->ptr[1]=(unsigned char)(value>>(8-b->endbit));
    if(bits>=16){
      b->ptr[2]=(unsigned char)(value>>(16-b->endbit));
      if(bits>=24){
	b->ptr[3]=(unsigned char)(value>>(24-b->endbit));
	if(bits>=32){
	  if(b->endbit)
	    b->ptr[4]=(unsigned char)(value>>(32-b->endbit));
	  else
	    b->ptr[4]=0;
	}
      }
    }
  }

  b->endbyte+=bits/8;
  b->ptr+=bits/8;
  b->endbit=bits&7;
}

void kate_pack_writealign(kate_pack_buffer *b){
  int bits=8-b->endbit;
  if(bits<8)
    kate_pack_write(b,0,bits);
}

static void kate_pack_writecopy_helper(kate_pack_buffer *b,
				     void *source,
				     long bits,
				     void (*w)(kate_pack_buffer *,
					       unsigned long,
					       int),
				     int msb){
  unsigned char *ptr=(unsigned char *)source;

  long bytes=bits/8;
  bits-=bytes*8;

  if(b->endbit){
    int i;
    /* unaligned copy.  Do it the hard way. */
    for(i=0;i<bytes;i++)
      w(b,(unsigned long)(ptr[i]),8);    
  }else{
    /* aligned block copy */
    if(b->endbyte+bytes+1>=b->storage){
      b->storage=b->endbyte+bytes+BUFFER_INCREMENT;
      b->buffer=kate_realloc(b->buffer,b->storage);
      b->ptr=b->buffer+b->endbyte;
    }

    memmove(b->ptr,source,bytes);
    b->ptr+=bytes;
    b->endbyte+=bytes;
    *b->ptr=0;

  }
  if(bits){
    if(msb)
      w(b,(unsigned long)(ptr[bytes]>>(8-bits)),bits);    
    else
      w(b,(unsigned long)(ptr[bytes]),bits);    
  }
}

void kate_pack_writecopy(kate_pack_buffer *b,void *source,long bits){
  kate_pack_writecopy_helper(b,source,bits,kate_pack_write,0);
}

void kate_pack_reset(kate_pack_buffer *b){
  b->ptr=b->buffer;
  b->buffer[0]=0;
  b->endbit=b->endbyte=0;
}

void kate_pack_writeclear(kate_pack_buffer *b){
  kate_free(b->buffer);
  memset(b,0,sizeof(*b));
}

void kate_pack_readinit(kate_pack_buffer *b,unsigned char *buf,int bytes){
  memset(b,0,sizeof(*b));
  b->buffer=b->ptr=buf;
  b->storage=bytes;
}

/* Read in bits without advancing the bitptr; bits <= 32 */
long kate_pack_look(kate_pack_buffer *b,int bits){
  unsigned long ret;
  unsigned long m=mask[bits];

  bits+=b->endbit;

  if(b->endbyte+4>=b->storage){
    /* not the main path */
    if(b->endbyte*8+bits>b->storage*8)return(-1);
  }
  
  ret=b->ptr[0]>>b->endbit;
  if(bits>8){
    ret|=b->ptr[1]<<(8-b->endbit);  
    if(bits>16){
      ret|=b->ptr[2]<<(16-b->endbit);  
      if(bits>24){
	ret|=b->ptr[3]<<(24-b->endbit);  
	if(bits>32 && b->endbit)
	  ret|=b->ptr[4]<<(32-b->endbit);
      }
    }
  }
  return(m&ret);
}

long kate_pack_look1(kate_pack_buffer *b){
  if(b->endbyte>=b->storage)return(-1);
  return((b->ptr[0]>>b->endbit)&1);
}

void kate_pack_adv(kate_pack_buffer *b,int bits){
  bits+=b->endbit;
  b->ptr+=bits/8;
  b->endbyte+=bits/8;
  b->endbit=bits&7;
}

void kate_pack_adv1(kate_pack_buffer *b){
  if(++(b->endbit)>7){
    b->endbit=0;
    b->ptr++;
    b->endbyte++;
  }
}

/* bits <= 32 */
long kate_pack_read(kate_pack_buffer *b,int bits){
  long ret;
  unsigned long m=mask[bits];

  bits+=b->endbit;

  if(b->endbyte+4>=b->storage){
    /* not the main path */
    ret=-1L;
    if(b->endbyte*8+bits>b->storage*8)goto overflow;
  }
  
  ret=b->ptr[0]>>b->endbit;
  if(bits>8){
    ret|=b->ptr[1]<<(8-b->endbit);  
    if(bits>16){
      ret|=b->ptr[2]<<(16-b->endbit);  
      if(bits>24){
	ret|=b->ptr[3]<<(24-b->endbit);  
	if(bits>32 && b->endbit){
	  ret|=b->ptr[4]<<(32-b->endbit);
	}
      }
    }
  }
  ret&=m;
  
 overflow:

  b->ptr+=bits/8;
  b->endbyte+=bits/8;
  b->endbit=bits&7;
  return(ret);
}

long kate_pack_read1(kate_pack_buffer *b){
  long ret;
  
  if(b->endbyte>=b->storage){
    /* not the main path */
    ret=-1L;
    goto overflow;
  }

  ret=(b->ptr[0]>>b->endbit)&1;
  
 overflow:

  b->endbit++;
  if(b->endbit>7){
    b->endbit=0;
    b->ptr++;
    b->endbyte++;
  }
  return(ret);
}

long kate_pack_bytes(kate_pack_buffer *b){
  return(b->endbyte+(b->endbit+7)/8);
}

long kate_pack_bits(kate_pack_buffer *b){
  return(b->endbyte*8+b->endbit);
}

unsigned char *kate_pack_get_buffer(kate_pack_buffer *b){
  return(b->buffer);
}

long kate_pack_readable_bits(kate_pack_buffer *b){
  return b->storage*8-kate_pack_bits(b);
}

#undef BUFFER_INCREMENT
