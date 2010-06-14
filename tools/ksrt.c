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
#include <stdlib.h>
#include <ogg/ogg.h>
#include "kate/kate.h"
#include "kutil.h"
#include "ksrt.h"

void *new_srt_data(void)
{
  struct srt_data *srt_data=kate_malloc(sizeof(struct srt_data));
  if (srt_data) {
    srt_data->event_index=0;
  }
  return srt_data;
}

void free_srt_data(void *data)
{
  kate_free(data);
}

static ogg_int64_t granule_to_milliseconds(const kate_info *ki,ogg_int64_t granpos)
{
  return (1000*granpos*ki->gps_denominator+ki->gps_numerator/2)/ki->gps_numerator;
}

static void milliseconds_to_time(ogg_int64_t ms,int *hours,int *minutes,int *seconds,int *milliseconds)
{
  *hours=ms/(60*60*1000);
  *minutes=ms/(60*1000)%60;
  *seconds=ms/1000%60;
  *milliseconds=ms%1000;
}

void write_srt_event(FILE *fout,void *data,const kate_event *ev,ogg_int64_t granpos)
{
  int h0,m0,s0,ms0;
  int h1,m1,s1,ms1;
  struct srt_data *srt_data=(struct srt_data*)data;

  milliseconds_to_time(granule_to_milliseconds(ev->ki,ev->start),&h0,&m0,&s0,&ms0);
  milliseconds_to_time(granule_to_milliseconds(ev->ki,ev->start+ev->duration),&h1,&m1,&s1,&ms1);

  (void)granpos;
  (void)data;
  fprintf(fout,"%d\n",++srt_data->event_index);
  fprintf(fout,"%02d:%02d:%02d,%03d --> %02d:%02d:%02d,%03d\n",h0,m0,s0,ms0,h1,m1,s1,ms1);
  fprintf(fout,"%s\n",ev->text);
  fprintf(fout,"\n");
  fflush(fout);
}

