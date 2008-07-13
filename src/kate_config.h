/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */

#ifndef _KATE_CONFIG_H_
#define _KATE_CONFIG_H_

#if (defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L) || defined __USE_ISOC99
#define KATE_USE_C99
#endif

#ifndef KATE_USE_C99
#define inline
#define const
#endif

#ifndef __GNUC__
#define __attribute__(x)
#endif

#endif

