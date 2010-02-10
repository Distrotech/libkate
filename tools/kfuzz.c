/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <ogg/ogg.h>
#include "kate/kate.h"
#include "kfuzz.h"

#ifdef DEBUG

unsigned long gen_fuzz(unsigned long *seed)
{
  return *seed=*seed*65521+0x9e370001UL;
}

unsigned long gen_fuzz_n(unsigned long *seed,unsigned long n)
{
  return gen_fuzz(seed)%n;
}

static void fuzz_packet(unsigned long *seed,unsigned long nbytes,unsigned char *data)
{
  unsigned long n=gen_fuzz_n(seed,4),i;
  if (n!=0) return;
  n=gen_fuzz_n(seed,8);
  for (i=0;i<n;++i) {
    unsigned long offset=gen_fuzz_n(seed,nbytes);
    data[offset]^=1;
  }
}

void fuzz_kate_packet(unsigned long *seed,kate_packet *kp)
{
  fuzz_packet(seed,kp->nbytes,kp->data);
}

void fuzz_ogg_packet(unsigned long *seed,ogg_packet *op)
{
  fuzz_packet(seed,op->bytes,op->packet);
}

#endif

