/* Copyright (C) 2008, 2009 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#ifndef KATE_kate_internal_h_GUARD
#define KATE_kate_internal_h_GUARD

#define KATE_LIMIT_COMMENTS 4096
#define KATE_LIMIT_COMMENT_LENGTH 4096
#define KATE_LIMIT_REGIONS 4096
#define KATE_LIMIT_STYLES 4096
#define KATE_LIMIT_CURVE_POINTS 4096
#define KATE_LIMIT_CURVES 4096
#define KATE_LIMIT_MOTION_CURVES 4096
#define KATE_LIMIT_MOTIONS 4096
#define KATE_LIMIT_PALETTES 4096
#define KATE_LIMIT_BITMAPS 4096
#define KATE_LIMIT_BITMAP_SIZE 4096
#define KATE_LIMIT_BITMAP_RAW_SIZE  4194304
#define KATE_LIMIT_FONT_RANGES 4096
#define KATE_LIMIT_FONT_MAPPINGS 4096
#define KATE_LIMIT_FONT_MAPPING_RANGES 4096
#define KATE_LIMIT_TEXT_MOTIONS 4096
#define KATE_LIMIT_LANGUAGE_LENGTH 1024
#define KATE_LIMIT_TEXT_LENGTH 1048576

#include "config.h"

#include "kate/kate.h"

#if !defined __GNUC__ || (((__GNUC__*0x100)+(__GNUC_MINOR__+0))<0x0303)
#define __attribute__(x)
#endif

#ifdef __ELF__
#define kate_internal __attribute__((visibility("internal")))
#else
#define kate_internal
#endif

/* from http://www.fefe.de/intof.html */
#define KATE_TYPE_HALF_MAX_SIGNED(type) ((type)1 << (sizeof(type)*8-2))
#define KATE_TYPE_MAX_SIGNED(type) (KATE_TYPE_HALF_MAX_SIGNED(type) - 1 + KATE_TYPE_HALF_MAX_SIGNED(type))
#define KATE_TYPE_MIN_SIGNED(type) (-1 - KATE_TYPE_MAX_SIGNED(type))
#define KATE_TYPE_MIN(type) ((type)-1 < 1?KATE_TYPE_MIN_SIGNED(type):(type)0)
#define KATE_TYPE_MAX(type) ((type)~KATE_TYPE_MIN(type))

static inline int kate_check_add_overflow(size_t x,size_t y,size_t *res) __attribute__((pure));
static inline int kate_check_add_overflow(size_t x,size_t y,size_t *res)
{
  if (KATE_TYPE_MAX(size_t)-(y)>=x) {
    if (res) *res=x+y;
    return 0;
  }
  return KATE_E_LIMIT;
}

static inline int kate_check_mul_overflow_generic(size_t x,size_t y,size_t *res) __attribute__((pure));
static inline int kate_check_mul_overflow_generic(size_t x,size_t y,size_t *res)
{
  kate_uint64_t r,mask;
  if ((x&y)>>(sizeof(size_t)*4)) return KATE_E_LIMIT;
  mask=KATE_TYPE_MAX(size_t)>>(sizeof(size_t)*4);
  r=(x>>(sizeof(size_t)*4))*(y&mask)+(y>>(sizeof(size_t)*4))*(x&mask);
  if (r>>(sizeof(size_t)*4)) return KATE_E_LIMIT;
  r<<=(sizeof(size_t)*4);
  return kate_check_add_overflow(r,(x&mask)*(y&mask),res);
}

static inline int kate_check_mul_overflow(size_t x,size_t y,size_t *res) __attribute__((pure));
static inline int kate_check_mul_overflow(size_t x,size_t y,size_t *res)
{
  if (sizeof(size_t)>4) {
    return kate_check_mul_overflow_generic(x,y,res);
  }
  else {
    kate_uint64_t r=((kate_uint64_t)x)*y;
    if ((r>>(sizeof(size_t)*4))>>(sizeof(size_t)*4)) return KATE_E_LIMIT;
    if (res) *res=(size_t)r;
    return 0;
  }
}

/* granule operations */
extern kate_int64_t kate_time_granule(const kate_info *ki,kate_float base,kate_float offset) kate_internal;

/* motions */
extern int kate_motion_destroy(const kate_info *ki,kate_motion **motions,const int *destroy,size_t nmotions,int force) kate_internal;

/* event tracking */
extern kate_event *kate_event_create(const kate_info *ki) kate_internal;
extern int kate_event_track(kate_event *ev) kate_internal;
extern int kate_event_release(kate_event *ev) kate_internal;
extern int kate_event_destroy(kate_event *ev) kate_internal;

/* checks for header references */
extern int kate_find_region(const kate_info *ki,const kate_region *kr);
extern int kate_find_style(const kate_info *ki,const kate_style *ks);
extern int kate_find_curve(const kate_info *ki,const kate_curve *kc);
extern int kate_find_motion(const kate_info *ki,const kate_motion *km);
extern int kate_find_palette(const kate_info *ki,const kate_palette *kp);
extern int kate_find_bitmap(const kate_info *ki,const kate_bitmap *kb);
extern int kate_find_font_range(const kate_info *ki,const kate_font_range *kfr);
extern int kate_find_font_mapping(const kate_info *ki,const kate_font_mapping *kfm);

/* utility */
extern int kate_ascii_strncasecmp(const char *s0,const char *s1,size_t n) kate_internal __attribute__((pure));
extern int kate_ascii_strcasecmp(const char *s0,const char *s1) kate_internal __attribute__((pure));

extern int kate_is_valid_code_point(int c) kate_internal __attribute__((const));

extern int kate_font_check_ranges(const kate_font_mapping *kfm);

extern void *kate_checked_malloc(size_t n,size_t sz) kate_internal __attribute__((malloc));
extern void *kate_checked_realloc(void *ptr,size_t n,size_t sz) kate_internal;

#endif

