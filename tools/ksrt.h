/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#ifndef KATE_ksrt_h_GUARD
#define KATE_ksrt_h_GUARD

extern void write_srt_event(FILE *fout,void *data,const kate_event *ev,ogg_int64_t granpos,int event_index);

#endif

