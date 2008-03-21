/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#define KATE_INTERNAL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogg/ogg.h>
#include <kate/kate.h>
#include "kate_internal.h"
#include "kate_decode_state.h"

/**
  \ingroup high
  Initializes a kate_state for high level decoding.
  \param k the kate_state structure to initialize
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_high_decode_init(kate_state *k)
{
  int ret;

  if (!k) return KATE_E_INVALID_PARAMETER;

  k->kds=kate_decode_state_create();
  if (!k->kds) return KATE_E_OUT_OF_MEMORY;
  k->kes=NULL;

  ret=kate_info_init(&k->kds->ki);
  if (ret<0) {
    kate_decode_state_destroy(k->kds);
    return ret;
  }
  k->ki=&k->kds->ki;
  ret=kate_comment_init(&k->kds->kc);
  if (ret<0) {
    kate_info_clear(&k->kds->ki);
    kate_decode_state_destroy(k->kds);
    return ret;
  }
  return 0;
}

/**
  \ingroup high
  Processes a packet and outputs an event if possible
  \param k the kate_state structure representing the stream to decode
  \param op the packet to decode
  \param ev where to place a pointer to an event if there we just decoded one
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_high_decode_packetin(kate_state *k,kate_packet *op,kate_const kate_event **ev)
{
  int ret;

  if (!k || !op) return KATE_E_INVALID_PARAMETER;
  if (!k->kds) return KATE_E_INIT;

  *ev=NULL;
  if (k->kds->ki.probe>=0) {
    /* still probing headers */
    if (k->kds->ki.probe==0 && !kate_decode_is_idheader(op))
      return 0;
    ret=kate_decode_headerin(&k->kds->ki,&k->kds->kc,op);
    if (ret>0) {
      k->kds->ki.probe=-1;
      ret=0;
    }
    return ret;
  }
  else {
    /* in data */
    ret=kate_decode_packetin(k,op);
    if (ret<0) return ret;
    ret=kate_decode_eventout(k,ev);
    return ret;
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
  if (!k) return KATE_E_INVALID_PARAMETER;
  if (!k->kds) return KATE_E_INIT;

  kate_info_clear(&k->kds->ki);
  kate_comment_clear(&k->kds->kc);
  kate_clear(k);

  return 0;
}

