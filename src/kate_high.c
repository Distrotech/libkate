/* Copyright (C) 2008 Vincent Penquerc'h.
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

/**
  \ingroup high
  Initializes a kate_state for high level decoding.
  \param k the kate_state structure to initialize
  \returns 0 success
  \returns 1 success, and we're at end of stream
  \returns KATE_E_* error
  */
int kate_high_decode_init(kate_state *k)
{
  int ret;
  kate_info *ki;
  kate_comment *kc;

  if (!k) return KATE_E_INVALID_PARAMETER;

  k->kes=NULL;
  k->kds=kate_decode_state_create();
  if (!k->kds) return KATE_E_OUT_OF_MEMORY;

  ki=(kate_info*)kate_malloc(sizeof(kate_info));
  if (!ki) {
    kate_decode_state_destroy(k->kds);
    return KATE_E_OUT_OF_MEMORY;
  }
  kc=(kate_comment*)kate_malloc(sizeof(kate_comment));
  if (!kc) {
    kate_free(ki);
    kate_decode_state_destroy(k->kds);
    return KATE_E_OUT_OF_MEMORY;
  }

  ret=kate_info_init(ki);
  if (ret<0) {
    kate_free(ki);
    kate_free(kc);
    kate_decode_state_destroy(k->kds);
    return ret;
  }

  ret=kate_comment_init(kc);
  if (ret<0) {
    kate_free(ki);
    kate_free(kc);
    kate_info_clear(ki);
    kate_decode_state_destroy(k->kds);
    return ret;
  }

  k->kds->ki=ki;
  k->kds->kc=kc;

  k->ki=k->kds->ki;

  return 0;
}

/**
  \ingroup high
  Processes a packet and outputs an event if possible
  \param k the kate_state structure representing the stream to decode
  \param kp the packet to decode
  \param ev where to place a pointer to an event if there we just decoded one
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_high_decode_packetin(kate_state *k,kate_packet *kp,kate_const kate_event **ev)
{
  int ret;

  if (!k || !kp) return KATE_E_INVALID_PARAMETER;
  if (!k->kds) return KATE_E_INIT;
  if (!k->kds->ki) return KATE_E_INIT;
  if (!k->kds->kc) return KATE_E_INIT;

  if (ev) *ev=NULL;
  if (k->kds->ki->probe>=0) {
    /* still probing headers */
    ret=kate_decode_headerin(k->kds->ki,k->kds->kc,kp);
    if (ret>0) {
      k->kds->ki->probe=-1;
      ret=0;
    }
    return ret;
  }
  else {
    /* in data */
    int eos=0;
    ret=kate_decode_packetin(k,kp);
    if (ret<0) return ret;
    if (ret>0) eos=1;
    ret=kate_decode_eventout(k,ev);
    if (ret<0) return ret;
    return eos;
  }
}

/**
  \ingroup high
  Clears a previously initialized kate_state structure
  \param k the kate_state structure to clear
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_high_decode_clear(kate_state *k)
{
  kate_info *ki;
  kate_comment *kc;

  if (!k) return KATE_E_INVALID_PARAMETER;
  if (!k->kds) return KATE_E_INIT;

  /* kate_clear will clear kds, so save what we need first */
  ki=k->kds->ki;
  kc=k->kds->kc;

  kate_clear(k);

  kate_free(kc);
  kate_free(ki);

  return 0;
}

/**
  \ingroup high
  Retrieves the comments structure for the given state
  \param k the kate_state structure to retrieve comments from
  \returns a pointer to the comments structure (NULL if error or not found)
  */
const kate_comment *kate_high_decode_get_comments(kate_state *k)
{
  if (!k) return NULL;
  if (!k->kds) return NULL;

  return k->kds->kc;
}

