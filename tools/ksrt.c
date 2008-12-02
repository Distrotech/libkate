/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <ogg/ogg.h>
#include "kate/oggkate.h"
#include "kate_internal.h"
#include "kutil.h"
#include "ksrt.h"

void write_srt_event(FILE *fout,void *data,const kate_event *ev,ogg_int64_t granpos,int event_index)
{
  float t0=ev->start_time;
  float t1=ev->end_time;

  (void)granpos;
  (void)data;
  fprintf(fout,"%d\n",event_index+1);
  fprintf(fout,"%02d:%02d:%02d,%03d --> %02d:%02d:%02d,%03d\n",
    time_hours(t0),time_minutes(t0),time_seconds(t0),time_milliseconds(t0),
    time_hours(t1),time_minutes(t1),time_seconds(t1),time_milliseconds(t1)
  );
  fprintf(fout,"%s\n",ev->text);
  fprintf(fout,"\n");
}

