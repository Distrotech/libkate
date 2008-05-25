/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#ifndef _KATE_RLE_H_
#define _KATE_RLE_H_

#include <kate/kate.h>
#include "kate_bitwise.h"
#include "kate_internal.h"

extern int kate_rle_encode(size_t width,size_t height,const unsigned char *values,size_t bits,kate_pack_buffer *kpb) kate_internal;
extern int kate_rle_decode(size_t width,size_t height,unsigned char *values,size_t bits,kate_pack_buffer *kpb) kate_internal;

#endif

