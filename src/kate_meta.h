/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#ifndef KATE_kate_meta_h_GUARD
#define KATE_kate_meta_h_GUARD

#include "kate_internal.h"
#include "kate/kate.h"

typedef struct kate_meta_leaf {
  char *tag;
  char *value;
  size_t len;
} kate_meta_leaf;

struct kate_meta {
  size_t nmeta;
  struct kate_meta_leaf *meta;
};

extern int kate_meta_create_copy(kate_meta **km,const kate_meta *km2) kate_internal;

#endif

