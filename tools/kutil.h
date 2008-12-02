/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#ifndef KATE_kutil_h_GUARD
#define KATE_kutil_h_GUARD

extern int time_hours(kate_float t);
extern int time_minutes(kate_float t);
extern kate_float time_float_seconds(kate_float t);
extern int time_seconds(kate_float t);
extern int time_milliseconds(kate_float t);

extern const char *eat_arg(int argc,char **argv,int *n);

#endif

