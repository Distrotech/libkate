/* Copyright (C) 2008, 2009 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#define KATE_INTERNAL
#include "kate_internal.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include "kate/kate.h"
#include "kate_decode_state.h"

kate_decode_state *kate_decode_state_create()
{
  kate_decode_state *kds;

  kds=(kate_decode_state*)kate_malloc(sizeof(kate_decode_state));
  if (!kds) return NULL;
  kds->ki=NULL;
  kds->kc=NULL;
  kds->event=NULL;
  kds->n_active_events=0;
  kds->active_events=NULL;

  return kds;
}

int kate_decode_state_clear(kate_decode_state *kds,const kate_info *ki,int new)
{
  if (!kds || !ki) return KATE_E_INVALID_PARAMETER;

  if (kds->event) {
    kate_event_release(kds->event);
    kds->event=NULL;
  }

  if (new) {
    kds->event=kate_event_create(ki);
    if (!kds->event) return KATE_E_OUT_OF_MEMORY;
    kate_event_track(kds->event);
  }

  return 0;
}

int kate_decode_state_flush_events(kate_decode_state *kds,kate_int64_t granule)
{
  size_t n;

  if (!kds) return KATE_E_INVALID_PARAMETER;

  /* -1 will flush everything */
  for (n=0;n<kds->n_active_events;++n) {
    if (granule<kds->active_events[n].start || granule>kds->active_events[n].end) {
      kds->active_events[n--]=kds->active_events[--kds->n_active_events];
    }
  }

  return 0;
}

int kate_decode_state_find_event(kate_decode_state *kds,kate_int32_t id)
{
  size_t n;

  if (!kds) return KATE_E_INVALID_PARAMETER;
  if (id<0) return KATE_E_INVALID_PARAMETER;

  for (n=0;n<kds->n_active_events;++n) {
    if (kds->active_events[n].id==id) return 0;
  }

  return KATE_E_NOT_FOUND;
}

int kate_decode_state_add_event(kate_decode_state *kds,const kate_event *ev)
{
  size_t n;
  kate_active_event *events;
  int ret;

  if (!kds) return KATE_E_INVALID_PARAMETER;
  if (!ev) return KATE_E_INVALID_PARAMETER;

  ret=kate_check_add_overflow(kds->n_active_events,1,NULL);
  if (ret<0) return ret;

  /* check if it is there already */
  for (n=0;n<kds->n_active_events;++n) {
    if (kds->active_events[n].id==ev->id) {
      return 1;
    }
  }

  /* if it is not there, add it */
  events=(kate_active_event*)kate_checked_realloc(kds->active_events,(kds->n_active_events+1),sizeof(kate_active_event));
  if (!events) return KATE_E_OUT_OF_MEMORY;

  kds->active_events=events;
  kds->active_events[kds->n_active_events].id=ev->id;
  kds->active_events[kds->n_active_events].start=ev->start;
  kds->active_events[kds->n_active_events].end=ev->start+ev->duration-1;
  ++kds->n_active_events;

  return 0;
}

int kate_decode_state_destroy(kate_decode_state *kds)
{
  if (!kds) return KATE_E_INVALID_PARAMETER;

  kate_event_release(kds->event);

  if (kds->ki) kate_info_clear(kds->ki);
  if (kds->kc) kate_comment_clear(kds->kc);

  kate_free(kds->active_events);

  kate_free(kds);

  return 0;
}
