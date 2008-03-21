/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#define KATE_INTERNAL

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <ogg/ogg.h>
#include <kate/kate.h>
#include "kate_fp.h"

#define MERGE_STREAMS
#define MERGE_STREAMS_COUNT_THRESHOLD 0
/* #define WRITE_64_IN_TWO_32 */

#define FLOAT_SCALE ((kate_float)(((kate_fp)1)<<(KATE_FP_FRAC)))
#define kate_fp_bits (sizeof(kate_fp)*8)
#define kate_fp_cut_bits ((kate_fp_bits)/2-1)
#define kate_fp_cut_bits_bits (kate_fp_bits==16?3:kate_fp_bits==32?4:kate_fp_bits==64?5:8)
#define kate_fp_sign_bit (kate_fp_bits==16?0x8000:kate_fp_bits==32?0x80000000:kate_fp_bits==64?0x8000000000000000ull:0)

static inline kate_fp f2kfp(kate_float f)
{
  kate_fp test=(kate_fp)(f*(FLOAT_SCALE*2));
  if (test&1)
    return (kate_fp)(f*FLOAT_SCALE+(kate_float)0.5);
  else
    return (kate_fp)(f*FLOAT_SCALE);
}

static inline kate_float kfp2f(kate_fp v)
{
  return v/FLOAT_SCALE;
}

static int kate_fp_scan(size_t count,const kate_fp *values,size_t streams,int *head,int *tail)
{
  kate_fp v=0,tst;
  size_t n;

  if (!values || !head || !tail) return KATE_E_INVALID_PARAMETER;

  while (count--) {
    kate_fp value=*values;
    values+=streams;
    if (value<0) value=-value;
    v|=value;
  }

  tst=v;
  for (n=0;n<kate_fp_cut_bits;++n) {
    if (tst&kate_fp_sign_bit) break;
    tst<<=1;
  }
  *head=n;

  tst=v;
  for (n=0;n<kate_fp_cut_bits;++n) {
    if (tst&1) break;
    tst>>=1;
  }
  *tail=n;

  return 0;
}

#if 0
static int kate_fp_scan_constant(size_t count,const kate_fp *values,size_t streams,kate_fp constant)
{
  while (count--) {
    kate_fp value=*values;
    values+=streams;
    if (value!=constant) return 1;
  }
  return 0;
}
#endif

/* need 8 bits, then a bit for sign and kate_fp_bits-(t+h) per value */
int kate_fp_encode(size_t count,const kate_fp *values,size_t streams,oggpack_buffer *opb)
{
  int head,tail;
  int bits;

  if (!opb || count==0 || !values) return KATE_E_INVALID_PARAMETER;

  kate_fp_scan(count,values,streams,&head,&tail);
  oggpack_write(opb,head,kate_fp_cut_bits_bits); /* can be between 0 and kate_fp_cut_bits */
  oggpack_write(opb,tail,kate_fp_cut_bits_bits); /* can be between 0 and kate_fp_cut_bits */
  bits=kate_fp_bits-tail-head;
  while (count--) {
    kate_fp v=*values++;
    if (head>0) {
      if (v<0) {
        oggpack_write(opb,1,1);
        v=-v;
      }
      else {
        oggpack_write(opb,0,1);
      }
    }
    v>>=tail;
#ifdef WRITE_64_IN_TWO_32
    if (bits>32) {
      oggpack_write(opb,v,32);
      v>>=32;
      oggpack_write(opb,v,bits-32); /* will autoclip the head */
    }
    else
#endif
    {
      oggpack_write(opb,v,bits); /* will autoclip the head */
    }
  }

  return 0;
}

int kate_fp_encode_kate_float(size_t count,const kate_float *values,size_t streams,oggpack_buffer *opb)
{
  kate_fp *fp_values;
  size_t s,n;
  int ret;

  if (count*streams==0) return 0;

  if (streams>1 && count>MERGE_STREAMS_COUNT_THRESHOLD) {
#ifdef MERGE_STREAMS
    count*=streams;
    streams=1;
    oggpack_write(opb,1,1);
#else
    oggpack_write(opb,0,1);
#endif
  }

  fp_values=(kate_fp*)kate_malloc(count*sizeof(kate_fp));
  if (!fp_values) return KATE_E_OUT_OF_MEMORY;

  for (s=0;s<streams;++s) {
    for (n=0;n<count;++n) {
      kate_float v=values[n*streams+s];
      fp_values[n]=f2kfp(v);
    }

    ret=kate_fp_encode(count,fp_values,1,opb);
    if (ret<0) {
      kate_free(fp_values);
      return ret;
    }
  }

  kate_free(fp_values);

  return 0;
}

int kate_fp_decode(size_t count,kate_fp *values,size_t streams,oggpack_buffer *opb)
{
  int head,tail,bits;
  kate_fp v;

  if (!opb || count==0 || !values) return KATE_E_INVALID_PARAMETER;

  head=oggpack_read(opb,kate_fp_cut_bits_bits);
  tail=oggpack_read(opb,kate_fp_cut_bits_bits);
  bits=kate_fp_bits-head-tail;
  while (count--) {
    int sign=0;
    if (head>0) {
      sign=oggpack_read(opb,1);
    }
#ifdef WRITE_64_IN_TWO_32
    if (bits>32) {
      kate_fp low=oggpack_read(opb,32);
      v=oggpack_read(opb,bits-32);
      v<<=32;
      v|=low;
    }
    else
#endif
    {
      v=oggpack_read(opb,bits);
    }
    v<<=tail;
    if (sign) v=-v;
    *values=v;
    values+=streams;
  }

  return 0;
}

int kate_fp_decode_kate_float(size_t count,kate_float *values,size_t streams,oggpack_buffer *opb)
{
  kate_fp *fp_values;
  size_t s,n;
  int ret;

  if (count*streams==0) return 0;

  if (streams>1 && count>MERGE_STREAMS_COUNT_THRESHOLD) {
    if (oggpack_read(opb,1)) {
      count*=streams;
      streams=1;
    }
  }

  fp_values=(kate_fp*)kate_malloc(count*sizeof(kate_fp));
  if (!fp_values) return KATE_E_OUT_OF_MEMORY;

  for (s=0;s<streams;++s) {
    ret=kate_fp_decode(count,fp_values,1,opb);
    if (ret<0) {
      kate_free(fp_values);
      return ret;
    }

    for (n=0;n<count;++n) {
      values[n*streams+s]=kfp2f(fp_values[n]);
    }
  }

  kate_free(fp_values);

  return 0;
}

