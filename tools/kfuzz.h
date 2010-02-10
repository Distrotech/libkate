/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#ifndef KATE_kfuzz_h_GUARD
#define KATE_kfuzz_h_GUARD

#include <ogg/ogg.h>
#include "kate/kate.h"

#ifdef DEBUG
extern unsigned long gen_fuzz(unsigned long *seed);
extern unsigned long gen_fuzz_n(unsigned long *seed,unsigned long n);
extern void fuzz_kate_packet(unsigned long *seed,kate_packet *kp);
extern void fuzz_ogg_packet(unsigned long *seed,ogg_packet *op);
#endif

#endif

