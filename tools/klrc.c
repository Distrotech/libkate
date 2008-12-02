/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <ogg/ogg.h>
#include "kate/kate.h"
#include "kutil.h"
#include "klrc.h"

void write_lrc_event(FILE *fout,void *data,const kate_event *ev,ogg_int64_t granpos,int event_index)
{
  float t0=ev->start_time;
  float t1=ev->end_time;
  struct lrc_data *lrc_data=(struct lrc_data*)data;

  (void)granpos;
  (void)data;
  if (event_index==0) {
    fprintf(fout,"[%d:%02d.%02d]",
      time_minutes(t0),time_seconds(t0),time_milliseconds(t0)/10
    );
  }
  else if (lrc_data->last_event_end_time<t0) {
    fprintf(fout,"\n");
    fprintf(fout,"[%d:%02d.%02d]",
      time_minutes(t0),time_seconds(t0),time_milliseconds(t0)/10
    );
  }

  fprintf(fout,"%s\n",ev->text);
  fprintf(fout,"[%d:%02d.%02d]",
    time_minutes(t1),time_seconds(t1),time_milliseconds(t1)/10
  );

  lrc_data->last_event_end_time=t1;
}

void write_lrc_end(FILE *fout)
{
  fprintf(fout,"\n");
}

