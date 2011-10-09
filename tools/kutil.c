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
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <ogg/ogg.h>
#include "kate/kate.h"
#include "kutil.h"

static int time_total_milliseconds(kate_float t)
{
    return (int)(t*1000+(kate_float)0.5);
}

int time_hours(kate_float t)
{
  return time_total_milliseconds(t)/(60*60*1000);
}

int time_minutes(kate_float t)
{
  return time_total_milliseconds(t)/(60*1000)%60;
}

int time_seconds(kate_float t)
{
  return time_total_milliseconds(t)/1000%60;
}

int time_milliseconds(kate_float t)
{
  return time_total_milliseconds(t)%1000;
}

kate_float time_float_seconds(kate_float t)
{
  return time_milliseconds(t)/(kate_float)1000+time_seconds(t);
}

void granule_to_hmsms(const kate_info *ki,ogg_int64_t granpos,int *h,int *m,int *s,int *ms)
{
  *h=(granpos*ki->gps_denominator)/ki->gps_numerator/(60*60);
  granpos-=(*h*ki->gps_numerator+ki->gps_denominator/2)/ki->gps_denominator*60*60;
  *m=(granpos*ki->gps_denominator)/ki->gps_numerator/60;
  granpos-=(*m*ki->gps_numerator+ki->gps_denominator/2)/ki->gps_denominator*60;
  *s=(granpos*ki->gps_denominator)/ki->gps_numerator;
  granpos-=(*s*ki->gps_numerator+ki->gps_denominator/2)/ki->gps_denominator;
  *ms=(1000*granpos*ki->gps_denominator)/ki->gps_numerator;
}


const char *eat_arg(int argc,char **argv,int *n)
{
  if (*n==argc-1) {
    fprintf(stderr,"%s needs an argument\n",argv[*n]);
    exit(-1);
  }
  return argv[++*n];
}

