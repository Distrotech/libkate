/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#ifndef KATE_kkate_h_GUARD
#define KATE_kkate_h_GUARD

extern int write_bitmaps;

extern void write_kate_start(FILE *f);
extern void write_kate_end(FILE *f);
extern void write_kate_headers(FILE *f,const kate_info *ki,const kate_comment *kc);
extern void write_kate_event(FILE *fout,void *data,const kate_event *ev,ogg_int64_t granpos,int event_index);

#endif

