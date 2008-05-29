/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#ifndef _KATE_DECODE_STATE_H_
#define _KATE_DECODE_STATE_H_

#include "kate_internal.h"
#include "kate/kate.h"

typedef struct kate_decode_state {
  kate_info *ki;
  kate_comment *kc;
  kate_event *event;
} kate_decode_state;

extern kate_decode_state *kate_decode_state_create(void) kate_internal;
extern int kate_decode_state_clear(kate_decode_state *kds,const kate_info *ki,int new) kate_internal;
extern int kate_decode_state_destroy(kate_decode_state *kds) kate_internal;

#endif
