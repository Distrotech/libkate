/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#define KATE_INTERNAL

#include <stdlib.h>
#include <string.h>
#include "kate_internal.h"
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

int kate_decode_state_destroy(kate_decode_state *kds)
{
  if (!kds) return KATE_E_INVALID_PARAMETER;

  kate_event_release(kds->event);

  if (kds->ki) kate_info_clear(kds->ki);
  if (kds->kc) kate_comment_clear(kds->kc);

  kate_free(kds);

  return 0;
}
