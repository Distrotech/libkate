/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */

#ifndef _KATE_CONFIG_H_
#define _KATE_CONFIG_H_

#include <stddef.h>
#include <limits.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef KATE_INTERNAL
#define kate_const
#else
#define kate_const const
#endif

#ifndef kate_malloc
#define kate_malloc malloc
#endif
#ifndef kate_realloc
#define kate_realloc realloc
#endif
#ifndef kate_free
#define kate_free free
#endif

#if !defined HAVE_ATTRIBUTE
#define __attribute__(x)
#endif

#if defined HAVE_STDINT_H || defined HAVE_INTTYPES_H
typedef int32_t kate_int32_t;
#elif defined int32_t
typedef int32_t kate_int32_t;
#elif defined INT_MAX && INT_MAX==2147483647
typedef int kate_int32_t;
#elif defined SHRT_MAX && SHRT_MAX==2147483647
typedef short int kate_int32_t;
#elif defined LONG_MAX && LONG_MAX==2147483647
typedef long int kate_int32_t;
#elif defined LLONG_MAX && LLONG_MAX==2147483647
typedef long long int kate_int32_t;
#else
#error No 32 bit signed integer found
#endif

#if defined HAVE_STDINT_H || defined HAVE_INTTYPES_H
typedef int64_t kate_int64_t;
#elif defined int64_t
typedef int64_t kate_int64_t;
#elif defined INT_MAX && INT_MAX>2147483647
typedef int kate_int64_t;
#elif defined SHRT_MAX && SHRT_MAX>2147483647
typedef short int kate_int64_t;
#elif defined LONG_MAX && LONG_MAX>2147483647
typedef long int kate_int64_t;
#elif defined LLONG_MAX && LLONG_MAX>2147483647
typedef long long int kate_int64_t;
#else
#error No 64 bit signed integer found
#endif

#if defined HAVE_STDINT_H || defined HAVE_INTTYPES_H
typedef uintptr_t kate_uintptr_t;
#elif defined uintptr_t
typedef uintptr_t kate_uintptr_t;
#else
#warning No suitable type for holding integer and pointer found, using unsigned long
typedef unsigned long kate_uintptr_t;
#endif

typedef float kate_float;

#endif

