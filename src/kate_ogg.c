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
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include "kate/oggkate.h"
#include "kate_encode_state.h"

static void kate_packet_wrap_ogg(kate_packet *kp,const ogg_packet *op)
{
  kate_packet_wrap(kp,op->bytes,op->packet);
}

static int kate_packet_create_ogg(kate_packet *kp,ogg_packet *op,kate_encode_state *kes,int eos)
{
  op->b_o_s=(kes->packetno==0);
  op->e_o_s=eos;
  op->packetno=kes->packetno;
  op->granulepos=kes->granulepos;
  op->bytes=kp->nbytes;
  op->packet=_ogg_malloc(kp->nbytes);
  if (!op->packet) {
    kate_free(kp->data);
    return KATE_E_OUT_OF_MEMORY;
  }
  memcpy(op->packet,kp->data,kp->nbytes);
  kate_free(kp->data);
  return 0;
}

/**
  Encodes a Kate header to an Ogg packet
  The kate_state structure should have been initialized with kate_decode_init or kate_encode_init.
  \param k the kate_state structure to encode headers for
  \param kc the comments to encode in headers
  \param op the ogg_packet to encode headers to
  \returns 0 success
  \returns 1 success, and all headers have been encoded
  \returns KATE_E_* error
 */
int kate_ogg_encode_headers(kate_state *k,kate_comment *kc,ogg_packet *op)
{
  kate_packet kp;
  int ret=kate_encode_headers(k,kc,&kp);
  if (ret<0) return ret;
  if (ret>0) return ret; /* no more headers, no packet created */
  return kate_packet_create_ogg(&kp,op,k->kes,0);
}

/**
  Encodes a text data packet to an Ogg packet
  The kate_state structure should have been initialized with kate_decode_init or kate_encode_init.
  \param k the kate_state structure to encode headers for
  \param start_time the start time, in seconds, of the event
  \param stop_time the stop time, in seconds, of the event
  \param text the text this event will hold (may be empty if none)
  \param sz the size, in bytes, of the text
  \param op the ogg_packet to encode the packet to
  \returns 0 success
  \returns KATE_E_* error
 */
int kate_ogg_encode_text(kate_state *k,kate_float start_time,kate_float stop_time,const char *text,size_t sz,ogg_packet *op)
{
  kate_packet kp;
  int ret=kate_encode_text(k,start_time,stop_time,text,sz,&kp);
  if (ret<0) return ret;
  return kate_packet_create_ogg(&kp,op,k->kes,0);
}

/**
  Encodes a text data packet to an Ogg packet
  The kate_state structure should have been initialized with kate_decode_init or kate_encode_init.
  \param k the kate_state structure to encode headers for
  \param start_time the start time, in seconds, of the event
  \param stop_time the stop time, in seconds, of the event
  \param text the text this event will hold (may be empty if none)
  \param sz the size, in bytes, of the text
  \param op the ogg_packet to encode the packet to
  \returns 0 success
  \returns KATE_E_* error
 */
int kate_ogg_encode_text_raw_times(kate_state *k,kate_int64_t start_time,kate_int64_t stop_time,const char *text,size_t sz,ogg_packet *op)
{
  kate_packet kp;
  int ret=kate_encode_text_raw_times(k,start_time,stop_time,text,sz,&kp);
  if (ret<0) return ret;
  return kate_packet_create_ogg(&kp,op,k->kes,0);
}

/**
  Encodes a repeat data packet to an Ogg packet
  The kate_state structure should have been initialized with kate_decode_init or kate_encode_init.
  \param k the kate_state structure to encode headers for
  \param t the time at which to insert the repeat packet
  \param threshold the minimum age an active event must be for a repeat packet to be encoded
  \param op the ogg_packet to encode the packet to
  \returns 0 success, and no repeat packets were needed
  \returns 1 success, and a repeat packet was encoded
  \returns KATE_E_* error
 */
int kate_ogg_encode_repeat(kate_state *k,kate_float t,kate_float threshold,ogg_packet *op)
{
  kate_packet kp;
  int ret=kate_encode_repeat(k,t,threshold,&kp);
  if (ret<0) return ret;
  if (ret>0) {
    int pret=kate_packet_create_ogg(&kp,op,k->kes,0);
    if (pret<0) return pret;
  }
  return ret;
}

/**
  Encodes a repeat data packet to an Ogg packet
  The kate_state structure should have been initialized with kate_decode_init or kate_encode_init.
  \param k the kate_state structure to encode headers for
  \param t the time at which to insert the repeat packet
  \param threshold the minimum age an active event must be for a repeat packet to be encoded
  \param op the ogg_packet to encode the packet to
  \returns 0 success, and no repeat packets were needed
  \returns 1 success, and a repeat packet was encoded
  \returns KATE_E_* error
 */
int kate_ogg_encode_repeat_raw_times(kate_state *k,kate_int64_t t,kate_int64_t threshold,ogg_packet *op)
{
  kate_packet kp;
  int ret=kate_encode_repeat_raw_times(k,t,threshold,&kp);
  if (ret<0) return ret;
  if (ret>0) {
    int pret=kate_packet_create_ogg(&kp,op,k->kes,0);
    if (pret<0) return pret;
  }
  return ret;
}

/**
  Encodes a keepalive data packet to an Ogg packet
  The kate_state structure should have been initialized with kate_decode_init or kate_encode_init.
  \param k the kate_state structure to encode headers for
  \param t the time at which to insert the keepalive packet
  \param op the ogg_packet to encode the packet to
  \returns 0 success
  \returns KATE_E_* error
 */
int kate_ogg_encode_keepalive(kate_state *k,kate_float t,ogg_packet *op)
{
  kate_packet kp;
  int ret=kate_encode_keepalive(k,t,&kp);
  if (ret<0) return ret;
  return kate_packet_create_ogg(&kp,op,k->kes,0);
}

/**
  Encodes a keepalive data packet to an Ogg packet
  The kate_state structure should have been initialized with kate_decode_init or kate_encode_init.
  \param k the kate_state structure to encode headers for
  \param t the time at which to insert the keepalive packet
  \param op the ogg_packet to encode the packet to
  \returns 0 success
  \returns KATE_E_* error
 */
int kate_ogg_encode_keepalive_raw_times(kate_state *k,kate_int64_t t,ogg_packet *op)
{
  kate_packet kp;
  int ret=kate_encode_keepalive_raw_times(k,t,&kp);
  if (ret<0) return ret;
  return kate_packet_create_ogg(&kp,op,k->kes,0);
}

/**
  Encodes an end-of-stream data packet to an Ogg packet
  The kate_state structure should have been initialized with kate_decode_init or kate_encode_init.
  No other packet may be encoded afer an end of stream packet is encoded.
  \param k the kate_state structure to encode headers for
  \param t the time at which to insert the packet
  \param op the ogg_packet to encode the packet to
  \returns 0 success
  \returns KATE_E_* error
 */
int kate_ogg_encode_finish(kate_state *k,kate_float t,ogg_packet *op)
{
  kate_packet kp;
  int ret=kate_encode_finish(k,t,&kp);
  if (ret<0) return ret;
  return kate_packet_create_ogg(&kp,op,k->kes,1);
}

/**
  Encodes an end-of-stream data packet to an Ogg packet
  The kate_state structure should have been initialized with kate_decode_init or kate_encode_init.
  No other packet may be encoded afer an end of stream packet is encoded.
  \param k the kate_state structure to encode headers for
  \param t the time at which to insert the packet
  \param op the ogg_packet to encode the packet to
  \returns 0 success
  \returns KATE_E_* error
 */
int kate_ogg_encode_finish_raw_times(kate_state *k,kate_int64_t t,ogg_packet *op)
{
  kate_packet kp;
  int ret=kate_encode_finish_raw_times(k,t,&kp);
  if (ret<0) return ret;
  return kate_packet_create_ogg(&kp,op,k->kes,1);
}

/**
  Checks whether an Ogg packet contains a Kate identification header.
  \param op the ogg_packet to test
  \returns 1 success, and the packet contains a Kate identification header
  \returns 0 success, and the packet does not contain a Kate identification header
  \returns KATE_E_* error
 */
int kate_ogg_decode_is_idheader(const ogg_packet *op)
{
  kate_packet kp;
  if (!op) return 0;
  if (op->packetno!=0) return 0;
  kate_packet_wrap_ogg(&kp,op);
  return kate_decode_is_idheader(&kp);
}

/**
  Decodes a Kate header
  \param ki the kate_info structure to fill from headers
  \param kc the kate_comment structure to fill from headers
  \param op the ogg_packet to test
  \returns 0 success
  \returns 1 success, and all headers have been decoded
  \returns KATE_E_* error
 */
int kate_ogg_decode_headerin(kate_info *ki,kate_comment *kc,ogg_packet *op)
{
  kate_packet kp;
  if (!op) return KATE_E_INVALID_PARAMETER;
  kate_packet_wrap_ogg(&kp,op);
  return kate_decode_headerin(ki,kc,&kp);
}

/**
  Decodes a Kate data packet
  \param k the kate_state structure to decode a packet for
  \param op the ogg_packet to test
  \returns 0 success
  \returns 1 success, and we're at end of stream
  \returns KATE_E_* error
 */
int kate_ogg_decode_packetin(kate_state *k,ogg_packet *op)
{
  kate_packet kp;
  if (!op) return KATE_E_INVALID_PARAMETER;
  kate_packet_wrap_ogg(&kp,op);
  return kate_decode_packetin(k,&kp);
}

