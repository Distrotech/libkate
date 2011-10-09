/* Copyright (C) 2011 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kate/kate.h"
#include "../src/kate_bitwise.h"

static kate_uint32_t getrnd(kate_uint32_t *rnd)
{
  return *rnd=*rnd*1103515245+12345;
}

static void test_bitwise_size(size_t bits,kate_uint32_t start_rnd)
{
  kate_pack_buffer kpb;
  size_t data_size=bits*4; /* good margin */
  unsigned char *data=alloca(data_size);
  size_t n,d;
  kate_uint32_t rnd;
  int read;

  rnd=start_rnd;
  d=0;
  kate_pack_writeinit(&kpb);
  for (n=0;n<bits;) {
    kate_uint32_t b=1+getrnd(&rnd)%32;
    kate_uint32_t r=getrnd(&rnd);
    if (d>=data_size) {
      printf("Warning: cut short\n");
      break;
    }
    if (b>bits-n) b=bits-n;
    r=r&((1<<b)-1);
    data[d++]=r;
    kate_pack_write(&kpb,r,b);
    n+=b;
  }

  if (kate_pack_bits(&kpb)!=bits) {
    fprintf(stderr,"Expected %d bits, got %d\n",bits,kate_pack_bits(&kpb));
    exit(1);
  }

  /* do not reset, reuse allocated buffer as is */
  kate_pack_readinit(&kpb,kpb.buffer,kate_pack_bytes(&kpb));

  d=0;
  rnd=start_rnd;
  for (n=0;n<bits;) {
    kate_uint32_t b=1+getrnd(&rnd)%32;
    kate_uint32_t r=getrnd(&rnd);
    if (b>bits-n) b=bits-n;
    r=r&((1<<b)-1);
    if (d>=data_size) break;
    read=kate_pack_read(&kpb,b);
    if (r!=read) {
      fprintf(stderr,"Read %d from %zu bits at bit offset %zu, expected %zu\n",read,b,n,r);
      exit(1);
    }
    n+=b;
  }

  kate_pack_reset(&kpb);
}

int main()
{
  size_t n,r;

  for (n=0;n<4096;++n) {
    for (r=0;r<16;++r) {
      test_bitwise_size(n,r);
    }
  }
  return 0;
}

