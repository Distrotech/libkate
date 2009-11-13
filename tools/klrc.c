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

static ogg_int64_t granule_to_centiseconds(const kate_info *ki,ogg_int64_t granpos)
{
  return (100*granpos*ki->gps_denominator+ki->gps_numerator/2)/ki->gps_numerator;
}

static void centiseconds_to_time(ogg_int64_t cs,int *minutes,int *seconds,int *centiseconds)
{
  *minutes=cs/(60*100);
  *seconds=cs/100%60;
  *centiseconds=cs%100;
}

void write_lrc_event(FILE *fout,void *data,const kate_event *ev,ogg_int64_t granpos,int event_index)
{
  float t0=ev->start_time;
  float t1=ev->end_time;
  struct lrc_data *lrc_data=(struct lrc_data*)data;
  int m0,s0,cs0;
  int m1,s1,cs1;

  centiseconds_to_time(granule_to_centiseconds(ev->ki,ev->start),&m0,&s0,&cs0);
  centiseconds_to_time(granule_to_centiseconds(ev->ki,ev->start+ev->duration),&m1,&s1,&cs1);

  (void)granpos;
  (void)data;
  if (event_index==0) {
    fprintf(fout,"[%d:%02d.%02d]",m0,s0,cs0);
  }
  else if (lrc_data->last_event_end_time<t0) {
    fprintf(fout,"\n");
    fprintf(fout,"[%d:%02d.%02d]",m0,s0,cs0);
  }

  fprintf(fout,"%s\n",ev->text);
  fprintf(fout,"[%d:%02d.%02d]",m1,s1,cs1);

  lrc_data->last_event_end_time=t1;
}

void write_lrc_end(FILE *fout)
{
  fprintf(fout,"\n");
}

