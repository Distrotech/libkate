/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#ifndef KATE_kstream_h_GUARD
#define KATE_kstream_h_GUARD

#include <ogg/ogg.h>
#include "kate/kate.h"

enum { kstream_header_info, kstream_data };

typedef struct {
  ogg_stream_state os;
  kate_state k;
  kate_info ki;
  kate_comment kc;
  int init;
  char *filename;
  FILE *fout;
  unsigned int stream_index;
  int event_index;
  int ret;
} kate_stream;

extern int kstream_init(kate_stream *ks,ogg_page *og,int stream_index);
extern int kstream_clear(kate_stream *ks);
extern char *get_filename(const char *basename,const kate_stream *ks,const kate_stream *streams,size_t nstreams);

#endif

