/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#define KATE_INTERNAL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kate/kate.h>
#include "kate_internal.h"
#include "kate_decode_state.h"
#include "kate_encode_state.h"

#define MKVER1(x) #x
#define MKVER2(x,y) MKVER1(x) "." MKVER1(y)
#define MKVER3(x,y,z) MKVER1(x)"."MKVER1(y)"."MKVER1(z)

/**
  \ingroup version
  Returns the version number of this version of libkate, in 8.8.8 major.minor.patch format
  \returns the version number of libkate
  */
int kate_get_version(void)
{
  return (KATE_VERSION_MAJOR<<16)|(KATE_VERSION_MINOR<<8)|KATE_VERSION_PATCH;
}

/**
  \ingroup version
  Returns a human readable string representing the version number of this version of libkate
  \returns the version number of libkate as a string
  */
const char *kate_get_version_string(void)
{
  return "libkate " MKVER3(KATE_VERSION_MAJOR,KATE_VERSION_MINOR,KATE_VERSION_PATCH) " (Tiger)";
}

/**
  \ingroup version
  Returns the highest bitstream version number that this version of libkate supports, in 8.8 major.minor format
  \returns the bitstream version number
  */
int kate_get_bitstream_version(void)
{
  return (KATE_BITSTREAM_VERSION_MAJOR<<8)|(KATE_BITSTREAM_VERSION_MINOR);
}

/**
  \ingroup version
  Returns a string representing the highest bitstream version number that this version of libkate supports
  \returns the bitstream version number as a string
  */
const char *kate_get_bitstream_version_string(void)
{
  return "kate bitstream " MKVER2(KATE_BITSTREAM_VERSION_MAJOR,KATE_BITSTREAM_VERSION_MINOR);
}

/**
  Destroys a kate_state structure.
  The kate_state structure should have been initialized with kate_decode_init or kate_encode_init.
  \param k the kate_state structure to clear
  \returns 0 success
  \returns KATE_E_* error
 */
int kate_clear(kate_state *k)
{
  if (!k) return KATE_E_INVALID_PARAMETER;

  if (k->kds) {
    kate_decode_state_destroy(k->kds);
    k->kds=NULL;
  }
  if (k->kes) {
    kate_encode_state_destroy(k->kes);
    k->kes=NULL;
  }

  return 0;
}

static inline int kate_ascii_tolower(int c)
{
  if (c>='A' && c<='Z') return c|32;
  return c;
}

int kate_ascii_strncasecmp(const char *s0,const char *s1,size_t n)
{
  while (n--) {
    int c0=kate_ascii_tolower(*s0++);
    int c1=kate_ascii_tolower(*s1++);
    if (c0!=c1) return c0-c1;
    if (!c0) return 0;
  }
  return 0;
}

