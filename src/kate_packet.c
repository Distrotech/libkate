/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#define KATE_INTERNAL

#include <stdlib.h>
#include <string.h>
#include <kate/kate.h>
#include "kate_internal.h"

/**
  \ingroup packet
  Returns a kate_packet wrapping the given data.
  The data is not copied, so should stay valid throughout the use of the
  kate_packet. The kate_packet should not be cleared.
  \param kp the kate_packet to wrap
  \param nbytes the number of bytes in the data to wrap
  \param data a pointer to the data to wrap
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_packet_wrap(kate_packet *kp,size_t nbytes,void *data)
{
  if (!kp) return KATE_E_INVALID_PARAMETER;
  if (!data && nbytes>0) return KATE_E_INVALID_PARAMETER;

  kp->nbytes=nbytes;
  kp->data=data;

  return 0;
}

/**
  \ingroup packet
  Returns a kate_packet created from the given data.
  The data is copied, so can be freed once the kate_packet is created.
  The kate_packet should be cleared when not needed anymore.
  \param kp the kate_packet to create
  \param nbytes the number of bytes in the data to wrap
  \param data a pointer to the data to wrap
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_packet_init(kate_packet *kp,size_t nbytes,void *data)
{
  void *dup_data;

  if (!kp) return KATE_E_INVALID_PARAMETER;
  if (!data && nbytes>0) return KATE_E_INVALID_PARAMETER;

  dup_data=kate_malloc(nbytes);
  if (!dup_data) return KATE_E_OUT_OF_MEMORY;

  return kate_packet_wrap(kp,nbytes,dup_data);
}

/**
  \ingroup packet
  Clears a kate_packet previously initialized by kate_packet_init
  \param kp the kate_packet to clear
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_packet_clear(kate_packet *kp)
{
  if (!kp) return KATE_E_INVALID_PARAMETER;

  if (kp->data) kate_free(kp->data);

  return 0;
}

