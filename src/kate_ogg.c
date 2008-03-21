/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#define KATE_INTERNAL

#include <stdlib.h>
#include <string.h>
#include <kate/oggkate.h>
#include "kate_encode_state.h"
#include "kate_internal.h"

static void kate_packet_wrap_ogg(kate_packet *kp,const ogg_packet *op)
{
  kate_packet_wrap(kp,op->bytes,op->packet);
}

static void kate_packet_create_ogg(kate_packet *kp,ogg_packet *op,kate_encode_state *kes,int eos)
{
  op->b_o_s=(kes->packetno==0);
  op->e_o_s=eos;
  op->packetno=kes->packetno;
  op->granulepos=kes->granulepos;
  op->bytes=kp->nbytes;
  op->packet=_ogg_malloc(kp->nbytes);
  memcpy(op->packet,kp->data,kp->nbytes);
  kate_free(kp->data);
}

int kate_ogg_encode_headers(kate_state *k,kate_comment *kc,ogg_packet *op)
{
  kate_packet kp;
  int ret=kate_encode_headers(k,kc,&kp);
  if (ret<0) return ret;
  if (ret>0) return ret; /* no more headers, no packet created */
  kate_packet_create_ogg(&kp,op,k->kes,0);
  return 0;
}

int kate_ogg_encode_text(kate_state *k,kate_float start_time,kate_float stop_time,const char *text,size_t sz,ogg_packet *op)
{
  kate_packet kp;
  int ret=kate_encode_text(k,start_time,stop_time,text,sz,&kp);
  if (ret<0) return ret;
  kate_packet_create_ogg(&kp,op,k->kes,0);
  return 0;
}

int kate_ogg_encode_keepalive(kate_state *k,kate_float t,ogg_packet *op)
{
  kate_packet kp;
  int ret=kate_encode_keepalive(k,t,&kp);
  if (ret<0) return ret;
  kate_packet_create_ogg(&kp,op,k->kes,0);
  return 0;
}

int kate_ogg_encode_finish(kate_state *k,kate_float t,ogg_packet *op)
{
  kate_packet kp;
  int ret=kate_encode_finish(k,t,&kp);
  if (ret<0) return ret;
  kate_packet_create_ogg(&kp,op,k->kes,1);
  return 0;
}

int kate_ogg_decode_is_idheader(const ogg_packet *op)
{
  kate_packet kp;
  if (!op) return 0;
  if (op->packetno!=0) return 0;
  kate_packet_wrap_ogg(&kp,op);
  return kate_decode_is_idheader(&kp);
}

int kate_ogg_decode_headerin(kate_info *ki,kate_comment *kc,ogg_packet *op)
{
  kate_packet kp;
  if (!op) return KATE_E_INVALID_PARAMETER;
  if (op->bytes>0 && ((op->packet[0]&~0x80)!=op->packetno)) return KATE_E_BAD_PACKET;
  kate_packet_wrap_ogg(&kp,op);
  return kate_decode_headerin(ki,kc,&kp);
}

int kate_ogg_decode_packetin(kate_state *k,ogg_packet *op)
{
  kate_packet kp;
  if (!op) return KATE_E_INVALID_PARAMETER;
  kate_packet_wrap_ogg(&kp,op);
  return kate_decode_packetin(k,&kp);
}

