/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#ifndef KATE_kstream_h_GUARD
#define KATE_kstream_h_GUARD

#include <stdio.h>
#include <ogg/ogg.h>
#include "kate/kate.h"

enum { kstream_uninitialized, kstream_header_info, kstream_data, kstream_eos };

typedef struct kate_stream {
  ogg_stream_state os;
  kate_state k;
  kate_info ki;
  kate_comment kc;
  int init;
  char *filename;
  FILE *fout;
  unsigned int stream_index;
  int ret;
  void *data;
} kate_stream;

typedef struct {
  size_t n_kate_streams;
  kate_stream **kate_streams;
} kate_stream_set;

extern int kstream_init(kate_stream *ks,ogg_page *og,int stream_index);
extern int kstream_clear(kate_stream *ks);
extern char *get_filename(const char *basename,const kate_stream *ks,const kate_stream_set *streams);

extern void init_kate_stream_set(kate_stream_set *streams);
extern kate_stream *add_kate_stream(kate_stream_set *streams,ogg_page *og,int stream_index);
extern kate_stream *find_kate_stream_for_page(kate_stream_set *streams,ogg_page *og);
extern void remove_kate_stream(const kate_stream *ks,kate_stream_set *streams);
extern void clear_and_remove_kate_stream(kate_stream *ks,kate_stream_set *streams);
extern void clear_kate_stream_set(kate_stream_set *streams);
extern kate_stream *find_kate_stream_for_file(kate_stream_set *streams,FILE *f);

#endif

