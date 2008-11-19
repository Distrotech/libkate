/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#ifndef KATE_kate_decode_state_h_GUARD
#define KATE_kate_decode_state_h_GUARD

#include "kate_internal.h"
#include "kate/kate.h"

typedef struct kate_active_event {
  kate_int32_t id;
  kate_int64_t start;
  kate_int64_t end;
} kate_active_event;

typedef struct kate_decode_state {
  kate_info *ki;
  kate_comment *kc;
  kate_event *event;

  size_t n_active_events;
  kate_active_event *active_events;
} kate_decode_state;

extern kate_decode_state *kate_decode_state_create(void) kate_internal;
extern int kate_decode_state_clear(kate_decode_state *kds,const kate_info *ki,int new) kate_internal;
extern int kate_decode_state_destroy(kate_decode_state *kds) kate_internal;
extern int kate_decode_state_find_event(kate_decode_state *kds,kate_int32_t id) kate_internal;
extern int kate_decode_state_add_event(kate_decode_state *kds,const kate_event *ev) kate_internal;
extern int kate_decode_state_flush_events(kate_decode_state *kds,kate_int64_t granule) kate_internal;

#endif
