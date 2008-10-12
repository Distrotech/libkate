/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#ifndef KATE_kate_fp_h_GUARD
#define KATE_kate_fp_h_GUARD

#include "kate_internal.h"
#include "kate/kate.h"
#include "kate_bitwise.h"

typedef int32_t kate_fp;

/* equal bits for int/frac */
/* #define KATE_FP_FRAC (sizeof(kate_fp)*8/2) */
/* more bits here for added frac precision (but less int range (and probably more space taken in the bitstream)) */
#define KATE_FP_FRAC (sizeof(kate_fp)*4)

#if 0
static inline kate_fp kate_fp_mul(kate_fp x,kate_fp y)
{
  int64_t v=x;
  v*=y;
  v>>=KATE_FP_FRAC;
  return v;
}

static inline kate_fp kate_fp_div(kate_fp x,kate_fp y)
{
  int64_t v=x;
  v<<=KATE_FP_FRAC;
  v/=y;
  return v;
}

static inline kate_fp kate_fp_add(kate_fp x, kate_fp y)
{
  return x+y;
}

static inline kate_fp kate_fp_sub(kate_fp x, kate_fp y)
{
  return x-y;
}
#endif

extern int kate_fp_encode(size_t count,const kate_fp *values,size_t streams,kate_pack_buffer *kpb) kate_internal;
extern int kate_fp_encode_kate_float(size_t count,const kate_float *values,size_t streams,kate_pack_buffer *kpb) kate_internal;
extern int kate_fp_decode(size_t count,kate_fp *values,size_t streams,kate_pack_buffer *kpb) kate_internal;
extern int kate_fp_decode_kate_float(size_t count,kate_float *values,size_t streams,kate_pack_buffer *kpb) kate_internal;

#endif

