/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#ifndef KATE_kread_h_GUARD
#define KATE_kread_h_GUARD

#include <stdio.h>

typedef struct {
  int (*on_page)(kate_uintptr_t data,long offset,ogg_page*);
  int (*on_hole)(kate_uintptr_t data,long offset,long size);
} ogg_parser_funcs;

extern int read_raw_size_and_packet(FILE *f,char **buffer,ogg_int64_t *bytes);
extern FILE *open_and_probe_stream(const char *filename);
extern int parse_ogg_stream(FILE *f,const char *pre_buffer,size_t pre_bytes,ogg_parser_funcs funcs,kate_uintptr_t data);

#endif

