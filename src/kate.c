/* Copyright (C) 2008, 2009 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#define KATE_INTERNAL
#include "kate_internal.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include "kate/kate.h"
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

static inline int kate_ascii_tolower(int c) __attribute__((const));
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

int kate_ascii_strcasecmp(const char *s0,const char *s1)
{
  return kate_ascii_strncasecmp(s0,s1,(size_t)-1);
}

/**
  \ingroup misc
  Initializes a kate region to sensible defaults
  \param kr the region to initialize
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_region_init(kate_region *kr)
{
  static const kate_region default_region={
    kate_percentage,
    10,80,80,10,
    -1,
    0,
    0,
    NULL,
    {0,0,0,0,0}
  };

  if (!kr) return KATE_E_INVALID_PARAMETER;
  memcpy(kr,&default_region,sizeof(kate_region));
  return 0;
}

/**
  \ingroup misc
  Initializes a kate style to sensible defaults
  \param ks the style to initialize
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_style_init(kate_style *ks)
{
  static const kate_style default_style={
    0,0,
    {255,255,255,255},
    {0,0,0,0},
    {255,255,255,255},
    kate_pixel,
    -1,-1,
    kate_pixel,
    0,0,0,0,
    0,
    0,
    0,
    0,
    0,
    kate_wrap_word,
    0,
    NULL,
    NULL,
    {0,0,0,0,0,0,0,0}
  };

  if (!ks) return KATE_E_INVALID_PARAMETER;
  memcpy(ks,&default_style,sizeof(kate_style));
  return 0;
}

/**
  \ingroup misc
  Initializes a kate palette to sensible defaults
  \param kp the palette to initialize
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_palette_init(kate_palette *kp)
{
  static const kate_palette default_palette={
    0,
    NULL,
    NULL,
    {0}
  };

  if (!kp) return KATE_E_INVALID_PARAMETER;
  memcpy(kp,&default_palette,sizeof(kate_palette));
  return 0;
}

/**
  \ingroup misc
  Initializes a kate bitmap to sensible defaults. Obsolete, use kate_bitmap_init_new in new code.
  \param kb the bitmap to initialize
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_bitmap_init(kate_bitmap *kb)
{
  typedef struct kate_bitmap_old_equivalent {
    size_t width;                                  /**< width in pixels */
    size_t height;                                 /**< height in pixels */
    unsigned char bpp;                             /**< bits per pixel, from 1 to 8, or 0 for a raw PNG bitmap */
    kate_bitmap_type type;                         /**< the type of this bitmap */
    unsigned char pad0[1];
    unsigned char internal;
    int palette;                                   /**< index of the default palette to use */
    unsigned char *pixels;                         /**< pixels, rows first, one byte per pixel regardless of bpp */
    size_t size;                                   /**< for raw bitmaps, number of bytes in pixels */
    int x_offset;                                  /**< the horizontal offset to the logical origin of the bitmap */
    int y_offset;                                  /**< the vertical offset to the logical origin of the bitmap */
  } kate_bitmap_old_equivalent;
  static const kate_bitmap_old_equivalent default_bitmap={
    0,0,
    0,
    kate_bitmap_type_png,
    {0},
    0, /* internal flag - meta and later not initialized */
    -1,
    NULL,
    0,
    0,0,
  };

  if (!kb) return KATE_E_INVALID_PARAMETER;
  memcpy(kb,&default_bitmap,sizeof(kate_bitmap_old_equivalent));
  return 0;
}

/**
  \ingroup misc
  Initializes a kate bitmap to sensible defaults
  \param kb the bitmap to initialize
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_bitmap_init_new(kate_bitmap *kb)
{
  static const kate_bitmap default_bitmap={
    0,0,
    0,
    kate_bitmap_type_png,
    {0},
    1, /* internal flag - all initialized */
    -1,
    NULL,
    0,
    0,0,
    NULL,
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0}
  };

  if (!kb) return KATE_E_INVALID_PARAMETER;
  memcpy(kb,&default_bitmap,sizeof(kate_bitmap));
  return 0;
}

/**
  \ingroup misc
  Initializes a kate curve to sensible defaults
  \param kc the curve to initialize
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_curve_init(kate_curve *kc)
{
  static const kate_curve default_curve={
    kate_curve_none,
    0,
    NULL,
    {0,0,0,0,0}
  };

  if (!kc) return KATE_E_INVALID_PARAMETER;
  memcpy(kc,&default_curve,sizeof(kate_curve));
  return 0;
}

/**
  \ingroup misc
  Initializes a kate motion to sensible defaults
  \param km the motion to initialize
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_motion_init(kate_motion *km)
{
  static const kate_motion default_motion={
    0,
    NULL,
    NULL,
    kate_motion_mapping_none,
    kate_motion_mapping_none,
    kate_motion_semantics_time,
    0,
    0,
    NULL,
    {0,0,0,0}
  };

  if (!km) return KATE_E_INVALID_PARAMETER;
  memcpy(km,&default_motion,sizeof(kate_motion));
  return 0;
}

void *kate_checked_malloc(size_t n,size_t sz)
{
  size_t mul;
  if (kate_check_mul_overflow(n,sz,&mul)) return NULL;
  return kate_malloc(mul);
}

void *kate_checked_realloc(void *ptr,size_t n,size_t sz)
{
  size_t mul;
  if (kate_check_mul_overflow(n,sz,&mul)) return NULL;
  return kate_realloc(ptr,mul);
}

