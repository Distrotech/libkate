/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#define KATE_INTERNAL
#include "kate_internal.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include "kate/kate.h"
#include "kate_rle.h"

/* 4 seems to be optimal for basic - 723 KB */
/* 3 seems to be optimal for basic_startend - 685 KB */
/* 4 seems to be optimal for basic_stop - 697 KB */
#define KATE_RLE_RUN_LENGTH_BITS_BASIC 4
#define KATE_RLE_RUN_LENGTH_BITS_BASIC_IN_DELTA 3
#define KATE_RLE_RUN_LENGTH_BITS_BASIC_STARTEND 3
#define KATE_RLE_RUN_LENGTH_BITS_BASIC_STOP 6
#define KATE_RLE_RUN_LENGTH_BITS_DELTA 6
#define KATE_RLE_RUN_LENGTH_BITS_BASIC_STARTEND_START 9
#define KATE_RLE_RUN_LENGTH_BITS_BASIC_STARTEND_END 8
#define KATE_RLE_RUN_LENGTH_BITS_BASIC_STOP_START 8
#define KATE_RLE_RUN_LENGTH_BITS_BASIC_IN_DELTA_STOP 3
#define KATE_RLE_RUN_LENGTH_BITS_DELTA_STOP 5
#define KATE_RLE_RUN_LENGTH_BITS_BASIC_ZERO 8
#define KATE_RLE_RUN_LENGTH_BITS_BASIC_NON_ZERO 3

#define KATE_RLE_TYPE_EMPTY 0
#define KATE_RLE_TYPE_BASIC 1
#define KATE_RLE_TYPE_DELTA 2
#define KATE_RLE_TYPE_BASIC_STOP 3
#define KATE_RLE_TYPE_BASIC_STARTEND 4
#define KATE_RLE_TYPE_DELTA_STOP 5
#define KATE_RLE_TYPE_BASIC_ZERO 6

#define KATE_RLE_TYPE_BITS 3

#define kate_unused __attribute__((unused))

static size_t get_run_length(size_t max_run_length,size_t count,const unsigned char *values)
{
  const size_t limit=max_run_length>count?count:max_run_length;
  size_t run_length=1;
  while (run_length<limit && values[run_length]==values[0])
    ++run_length;
  return run_length;
}

static size_t get_run_length_identical(size_t max_run_length,size_t count,const unsigned char *values,const unsigned char *previous,unsigned char zero)
{
  const size_t limit=max_run_length>count?count:max_run_length;
  size_t run_length=0;
  if (previous) {
    while (run_length<limit && values[run_length]==previous[run_length])
      ++run_length;
  }
  else {
    while (run_length<limit && values[run_length]==zero)
      ++run_length;
  }
  return run_length;
}

static size_t get_run_length_zero(size_t max_run_length,size_t count,const unsigned char *values,unsigned char zero)
{
  const size_t limit=max_run_length>count?count:max_run_length;
  size_t run_length=1;
  while (run_length<limit && values[run_length]==zero)
    ++run_length;
  return run_length;
}

static size_t get_run_length_zero_back(size_t max_run_length,size_t count,const unsigned char *values,size_t nvalues,unsigned char zero)
{
  const size_t limit=max_run_length>count?count:max_run_length;
  size_t run_length=0;
  while (run_length<limit && values[nvalues-1-run_length]==zero)
    ++run_length;
  return run_length;
}

static int kate_rle_encode_line_basic(size_t count,const unsigned char *values,size_t bits,unsigned char zero kate_unused,const unsigned char *previous kate_unused,kate_pack_buffer *kpb)
{
  const size_t run_length_bits=KATE_RLE_RUN_LENGTH_BITS_BASIC;
  const size_t run_length_cutoff=1<<run_length_bits;
  size_t run_length,max_run_length;

  while (count>0) {
    max_run_length=run_length_cutoff;
    run_length=get_run_length(max_run_length,count,values);
    kate_pack_write(kpb,run_length-1,run_length_bits);
    kate_pack_write(kpb,values[0],bits);
    values+=run_length;
    count-=run_length;
  }

  return 0;
}

static int kate_rle_encode_line_basic_zero(size_t count,const unsigned char *values,size_t bits,unsigned char zero kate_unused,const unsigned char *previous kate_unused,kate_pack_buffer *kpb)
{
  const size_t run_length_bits_zero=KATE_RLE_RUN_LENGTH_BITS_BASIC_ZERO;
  const size_t run_length_bits_non_zero=KATE_RLE_RUN_LENGTH_BITS_BASIC_NON_ZERO;
  const size_t run_length_zero_cutoff=1<<run_length_bits_zero;
  const size_t run_length_non_zero_cutoff=1<<run_length_bits_non_zero;
  size_t run_length,max_run_length,run_length_bits;

  while (count>0) {
    if (values[0]==zero) {
      max_run_length=run_length_zero_cutoff;
      run_length_bits=run_length_bits_zero;
    }
    else {
      max_run_length=run_length_non_zero_cutoff;
      run_length_bits=run_length_bits_non_zero;
    }
    run_length=get_run_length(max_run_length,count,values);
    kate_pack_write(kpb,values[0],bits);
    kate_pack_write(kpb,run_length-1,run_length_bits);
    values+=run_length;
    count-=run_length;
  }

  return 0;
}

static int kate_rle_encode_line_basic_startend(size_t count,const unsigned char *values,size_t bits,unsigned char zero,const unsigned char *previous kate_unused,kate_pack_buffer *kpb)
{
  const size_t run_length_bits=KATE_RLE_RUN_LENGTH_BITS_BASIC_STARTEND;
  const size_t run_length_cutoff=1<<run_length_bits;
  size_t run_length,run_length_start,run_length_end,max_run_length;

  max_run_length=(1<<KATE_RLE_RUN_LENGTH_BITS_BASIC_STARTEND_START)-1;
  run_length_start=get_run_length_zero(max_run_length,count,values,zero);
  kate_pack_write(kpb,run_length_start,KATE_RLE_RUN_LENGTH_BITS_BASIC_STARTEND_START);
  values+=run_length_start;
  count-=run_length_start;

  max_run_length=(1<<KATE_RLE_RUN_LENGTH_BITS_BASIC_STARTEND_END)-1;
  run_length_end=get_run_length_zero_back(max_run_length,count,values,count,zero);
  kate_pack_write(kpb,run_length_end,KATE_RLE_RUN_LENGTH_BITS_BASIC_STARTEND_END);
  count-=run_length_end;

  while (count>0) {
    max_run_length=run_length_cutoff;
    run_length=get_run_length(max_run_length,count,values);
    kate_pack_write(kpb,run_length-1,run_length_bits);
    kate_pack_write(kpb,values[0],bits);
    values+=run_length;
    count-=run_length;
  }

  return 0;
}

static int kate_rle_encode_line_basic_stop(size_t count,const unsigned char *values,size_t bits,unsigned char zero,const unsigned char *previous kate_unused,kate_pack_buffer *kpb)
{
  const size_t run_length_bits=KATE_RLE_RUN_LENGTH_BITS_BASIC_STOP;
  const size_t run_length_cutoff=(1<<run_length_bits)-1;
  size_t run_length,run_length_start,max_run_length;

  max_run_length=(1<<KATE_RLE_RUN_LENGTH_BITS_BASIC_STOP_START)-1;
  run_length_start=get_run_length_zero(max_run_length,count,values,zero);
  kate_pack_write(kpb,run_length_start,KATE_RLE_RUN_LENGTH_BITS_BASIC_STOP_START);
  values+=run_length_start;
  count-=run_length_start;

  while (count>0) {
    if (values[0]==zero) {
      run_length=get_run_length(count,count,values);
      if (run_length==count) {
        kate_pack_write(kpb,0,run_length_bits);
        break;
      }
    }
    max_run_length=run_length_cutoff;
    run_length=get_run_length(max_run_length,count,values);
    kate_pack_write(kpb,run_length,run_length_bits);
    kate_pack_write(kpb,values[0],bits);
    values+=run_length;
    count-=run_length;
  }

  return 0;
}

static int kate_rle_encode_line_empty(size_t count,const unsigned char *values,size_t bits kate_unused,unsigned char zero,const unsigned char *previous kate_unused,kate_pack_buffer *kpb kate_unused)
{
  size_t run_length=get_run_length_zero(count,count,values,zero);
  if (run_length<count) return -1;
  return 0;
}

static int kate_rle_encode_line_delta(size_t count,const unsigned char *values,size_t bits,unsigned char zero kate_unused,const unsigned char *previous,kate_pack_buffer *kpb)
{
  const size_t run_length_delta_bits=KATE_RLE_RUN_LENGTH_BITS_DELTA;
  const size_t run_length_delta_cutoff=(1<<run_length_delta_bits);
  const size_t run_length_basic_bits=KATE_RLE_RUN_LENGTH_BITS_BASIC_IN_DELTA;
  const size_t run_length_basic_cutoff=(1<<run_length_basic_bits);
  size_t max_run_length_delta,max_run_length_basic;
  size_t run_length_delta,run_length_basic;

  while (count>0) {
    max_run_length_delta=run_length_delta_cutoff;
    run_length_delta=get_run_length_identical(max_run_length_delta,count,values,previous,zero);

    max_run_length_basic=run_length_basic_cutoff;
    run_length_basic=get_run_length(max_run_length_basic,count,values);

    if (run_length_delta>run_length_basic) {
      kate_pack_write(kpb,1,1);
      kate_pack_write(kpb,run_length_delta-1,run_length_delta_bits);
      values+=run_length_delta;
      if (previous) previous+=run_length_delta;
      count-=run_length_delta;
    }
    else {
      kate_pack_write(kpb,0,1);
      kate_pack_write(kpb,run_length_basic-1,run_length_basic_bits);
      kate_pack_write(kpb,values[0],bits);
      values+=run_length_basic;
      if (previous) previous+=run_length_basic;
      count-=run_length_basic;
    }
  }

  return 0;
}

static int kate_rle_encode_line_delta_stop(size_t count,const unsigned char *values,size_t bits,unsigned char zero,const unsigned char *previous,kate_pack_buffer *kpb)
{
  const size_t run_length_delta_bits=KATE_RLE_RUN_LENGTH_BITS_DELTA_STOP;
  const size_t run_length_delta_cutoff=(1<<run_length_delta_bits);
  const size_t run_length_basic_bits=KATE_RLE_RUN_LENGTH_BITS_BASIC_IN_DELTA_STOP;
  const size_t run_length_basic_cutoff=(1<<run_length_basic_bits)-1;
  size_t max_run_length_delta,max_run_length_basic;
  size_t run_length_delta,run_length_basic,run_length;

  while (count>0) {
    if (values[0]==zero) {
      run_length=get_run_length(count,count,values);
      if (run_length==count) {
        kate_pack_write(kpb,0,1);
        kate_pack_write(kpb,0,run_length_basic_bits);
        break;
      }
    }

    max_run_length_delta=run_length_delta_cutoff;
    run_length_delta=get_run_length_identical(max_run_length_delta,count,values,previous,zero);

    max_run_length_basic=run_length_basic_cutoff;
    run_length_basic=get_run_length(max_run_length_basic,count,values);

    if (run_length_delta>run_length_basic) {
      kate_pack_write(kpb,1,1);
      run_length=run_length_delta;
      kate_pack_write(kpb,run_length-1,run_length_delta_bits);
    }
    else {
      kate_pack_write(kpb,0,1);
      run_length=run_length_basic;
      kate_pack_write(kpb,run_length,run_length_basic_bits);
      kate_pack_write(kpb,values[0],bits);
    }
    values+=run_length;
    if (previous) previous+=run_length;
    count-=run_length;
  }

  return 0;
}

#ifdef DEBUG
int kate_rle_stats[8]={0};
int kate_rle_stats_overall[8]={0};
#endif

static int kate_rle_try_encoding(kate_pack_buffer *kpb,int (*encoder)(size_t,const unsigned char*,size_t,unsigned char,const unsigned char*,kate_pack_buffer*),size_t width,const unsigned char *values,size_t bits,unsigned char zero,const unsigned char *previous,int kpb_type,int type)
{
  kate_pack_buffer tmp;
  kate_pack_writeinit(&tmp);
  if ((*encoder)(width,values,bits,zero,previous,&tmp)==0) {
    long kpb_size=kate_pack_bits(kpb);
    long tmp_size=kate_pack_bits(&tmp);
    if (kpb_type<0 || tmp_size<kpb_size) {
      kate_pack_writeclear(kpb);
      memcpy(kpb,&tmp,sizeof(tmp));
      return type;
    }
  }
  kate_pack_writeclear(&tmp);
  return kpb_type;
}

static unsigned char get_zero(size_t count,const unsigned char *values)
{
  int counts[256],n,best=0;
  memset(counts,0,sizeof(counts));
  while (count-->0) ++counts[*values++];
  for (n=0;n<256;++n) if (counts[n]>counts[best]) best=n;
  return best;
}

static int kate_rle_encode_best(size_t width,size_t height,const unsigned char *values,size_t bits,kate_pack_buffer *kpb)
{
  const unsigned char *previous=NULL;
  const unsigned char zero=get_zero(width*height,values);

#ifdef DEBUG
  memset(kate_rle_stats,0,sizeof(kate_rle_stats));
#endif

  kate_pack_write(kpb,zero,bits);
  while (height>0) {
    int best_type=-1;
    kate_pack_buffer best_buffer;
    kate_pack_writeinit(&best_buffer);

    best_type=kate_rle_try_encoding(&best_buffer,&kate_rle_encode_line_empty,width,values,bits,zero,previous,best_type,KATE_RLE_TYPE_EMPTY);
    best_type=kate_rle_try_encoding(&best_buffer,&kate_rle_encode_line_delta,width,values,bits,zero,previous,best_type,KATE_RLE_TYPE_DELTA);
    best_type=kate_rle_try_encoding(&best_buffer,&kate_rle_encode_line_basic,width,values,bits,zero,previous,best_type,KATE_RLE_TYPE_BASIC);
    best_type=kate_rle_try_encoding(&best_buffer,&kate_rle_encode_line_basic_startend,width,values,bits,zero,previous,best_type,KATE_RLE_TYPE_BASIC_STARTEND);
    best_type=kate_rle_try_encoding(&best_buffer,&kate_rle_encode_line_basic_stop,width,values,bits,zero,previous,best_type,KATE_RLE_TYPE_BASIC_STOP);
    best_type=kate_rle_try_encoding(&best_buffer,&kate_rle_encode_line_delta_stop,width,values,bits,zero,previous,best_type,KATE_RLE_TYPE_DELTA_STOP);
    best_type=kate_rle_try_encoding(&best_buffer,&kate_rle_encode_line_basic_zero,width,values,bits,zero,previous,best_type,KATE_RLE_TYPE_BASIC_ZERO);

#ifdef DEBUG
    ++kate_rle_stats[best_type];
    ++kate_rle_stats_overall[best_type];
#endif

    kate_pack_write(kpb,best_type,KATE_RLE_TYPE_BITS);
    kate_pack_writecopy(kpb,kate_pack_get_buffer(&best_buffer),kate_pack_bits(&best_buffer));

    kate_pack_writeclear(&best_buffer);

    previous=values;

    values+=width;
    --height;
  }

  return 0;
}

int kate_rle_encode(size_t width,size_t height,const unsigned char *values,size_t bits,kate_pack_buffer *kpb)
{
  return kate_rle_encode_best(width,height,values,bits,kpb);
}

static int kate_rle_decode_line_empty(size_t count,unsigned char *values,size_t bits kate_unused,unsigned char zero,kate_pack_buffer *kpb kate_unused)
{
  memset(values,zero,count);
  return 0;
}

static int kate_rle_decode_line_basic(size_t count,unsigned char *values,size_t bits,kate_pack_buffer *kpb)
{
  const size_t run_length_bits=KATE_RLE_RUN_LENGTH_BITS_BASIC;
  size_t run_length;
  int value;

  while (count>0) {
    run_length=1+kate_pack_read(kpb,run_length_bits);
    if (run_length==0 || run_length>count) return KATE_E_BAD_PACKET;
    value=kate_pack_read(kpb,bits);
    memset(values,value,run_length);
    values+=run_length;
    count-=run_length;
  }

  return 0;
}

static int kate_rle_decode_line_basic_zero(size_t count,unsigned char *values,size_t bits,unsigned char zero,kate_pack_buffer *kpb)
{
  const size_t run_length_bits_zero=KATE_RLE_RUN_LENGTH_BITS_BASIC_ZERO;
  const size_t run_length_bits_non_zero=KATE_RLE_RUN_LENGTH_BITS_BASIC_NON_ZERO;
  size_t run_length;
  int value;

  while (count>0) {
    value=kate_pack_read(kpb,bits);
    if (value==zero) {
      run_length=1+kate_pack_read(kpb,run_length_bits_zero);
    }
    else {
      run_length=1+kate_pack_read(kpb,run_length_bits_non_zero);
    }
    if (run_length==0 || run_length>count) return KATE_E_BAD_PACKET;
    memset(values,value,run_length);
    values+=run_length;
    count-=run_length;
  }

  return 0;
}

static int kate_rle_decode_line_basic_stop(size_t count,unsigned char *values,size_t bits,unsigned char zero,kate_pack_buffer *kpb)
{
  const size_t run_length_bits=KATE_RLE_RUN_LENGTH_BITS_BASIC_STOP;
  size_t run_length;
  int value;

  run_length=kate_pack_read(kpb,KATE_RLE_RUN_LENGTH_BITS_BASIC_STOP_START);
  if (run_length>0) {
    if (run_length>count) return KATE_E_BAD_PACKET;
    memset(values,zero,run_length);
    values+=run_length;
    count-=run_length;
  }

  while (count>0) {
    run_length=kate_pack_read(kpb,run_length_bits);
    if (run_length>count) return KATE_E_BAD_PACKET;
    if (run_length==0) {
      memset(values,zero,count);
      break;
    }
    value=kate_pack_read(kpb,bits);
    memset(values,value,run_length);
    values+=run_length;
    count-=run_length;
  }

  return 0;
}

static int kate_rle_decode_line_basic_startend(size_t count,unsigned char *values,size_t bits,unsigned char zero,kate_pack_buffer *kpb)
{
  const size_t run_length_bits=KATE_RLE_RUN_LENGTH_BITS_BASIC_STARTEND;
  size_t run_length;
  int value;

  run_length=kate_pack_read(kpb,KATE_RLE_RUN_LENGTH_BITS_BASIC_STARTEND_START);
  if (run_length>0) {
    if (run_length>count) return KATE_E_BAD_PACKET;
    memset(values,zero,run_length);
    values+=run_length;
    count-=run_length;
  }

  run_length=kate_pack_read(kpb,KATE_RLE_RUN_LENGTH_BITS_BASIC_STARTEND_END);
  if (run_length>0) {
    if (run_length>count) return KATE_E_BAD_PACKET;
    memset(values+count-run_length,zero,run_length);
    count-=run_length;
  }

  while (count>0) {
    run_length=1+kate_pack_read(kpb,run_length_bits);
    if (run_length_bits==0 || run_length>count) return KATE_E_BAD_PACKET;
    value=kate_pack_read(kpb,bits);
    memset(values,value,run_length);
    values+=run_length;
    count-=run_length;
  }

  return 0;
}

static int kate_rle_decode_line_delta(size_t count,unsigned char *values,const unsigned char *previous,size_t bits,unsigned char zero,kate_pack_buffer *kpb)
{
  const size_t run_length_delta_bits=KATE_RLE_RUN_LENGTH_BITS_DELTA;
  const size_t run_length_basic_bits=KATE_RLE_RUN_LENGTH_BITS_BASIC_IN_DELTA;
  int value;

  while (count>0) {
    int type=kate_pack_read1(kpb);
    if (type) {
      size_t run_length=1+kate_pack_read(kpb,run_length_delta_bits);
      if (run_length==0 || run_length>count) return KATE_E_BAD_PACKET;
      if (previous) {
        memcpy(values,previous,run_length);
        previous+=run_length;
      }
      else {
        memset(values,zero,run_length);
      }
      values+=run_length;
      count-=run_length;
    }
    else {
      size_t run_length=1+kate_pack_read(kpb,run_length_basic_bits);
      if (run_length==0 || run_length>count) return KATE_E_BAD_PACKET;
      value=kate_pack_read(kpb,bits);
      memset(values,value,run_length);
      values+=run_length;
      if (previous) previous+=run_length;
      count-=run_length;
    }
  }
  return 0;
}

static int kate_rle_decode_line_delta_stop(size_t count,unsigned char *values,const unsigned char *previous,size_t bits,unsigned char zero,kate_pack_buffer *kpb)
{
  const size_t run_length_delta_bits=KATE_RLE_RUN_LENGTH_BITS_DELTA_STOP;
  const size_t run_length_basic_bits=KATE_RLE_RUN_LENGTH_BITS_BASIC_IN_DELTA_STOP;
  size_t run_length;
  int value,type;

  while (count>0) {
    type=kate_pack_read1(kpb);
    if (type) {
      run_length=1+kate_pack_read(kpb,run_length_delta_bits);
      if (run_length==0 || run_length>count) return KATE_E_BAD_PACKET;
      if (previous) {
        memcpy(values,previous,run_length);
      }
      else {
        memset(values,zero,run_length);
      }
    }
    else {
      run_length=kate_pack_read(kpb,run_length_basic_bits);
      if (run_length==0) {
        memset(values,zero,count);
        break;
      }
      if (run_length>count) return KATE_E_BAD_PACKET;
      value=kate_pack_read(kpb,bits);
      memset(values,value,run_length);
    }
    values+=run_length;
    if (previous) previous+=run_length;
    count-=run_length;
  }
  return 0;
}

static int kate_rle_decode_best(size_t width,size_t height,unsigned char *values,size_t bits,kate_pack_buffer *kpb)
{
  const unsigned char *previous=NULL;
  int ret;

  const unsigned char zero=kate_pack_read(kpb,bits);
  while (height>0) {
    int type=kate_pack_read(kpb,KATE_RLE_TYPE_BITS);
    switch (type) {
      case KATE_RLE_TYPE_EMPTY:
        ret=kate_rle_decode_line_empty(width,values,bits,zero,kpb);
        break;
      case KATE_RLE_TYPE_DELTA:
        ret=kate_rle_decode_line_delta(width,values,previous,bits,zero,kpb);
        break;
      case KATE_RLE_TYPE_BASIC:
        ret=kate_rle_decode_line_basic(width,values,bits,kpb);
        break;
      case KATE_RLE_TYPE_BASIC_STARTEND:
        ret=kate_rle_decode_line_basic_startend(width,values,bits,zero,kpb);
        break;
      case KATE_RLE_TYPE_BASIC_STOP:
        ret=kate_rle_decode_line_basic_stop(width,values,bits,zero,kpb);
        break;
      case KATE_RLE_TYPE_DELTA_STOP:
        ret=kate_rle_decode_line_delta_stop(width,values,previous,bits,zero,kpb);
        break;
      case KATE_RLE_TYPE_BASIC_ZERO:
        ret=kate_rle_decode_line_basic_zero(width,values,bits,zero,kpb);
        break;
      default:
        ret=KATE_E_BAD_PACKET;
        break;
    }
    if (ret<0) return ret;

    previous=values;
    values+=width;
    --height;
  }

  return 0;
}

int kate_rle_decode(size_t width,size_t height,unsigned char *values,size_t bits,kate_pack_buffer *kpb)
{
  return kate_rle_decode_best(width,height,values,bits,kpb);
}

