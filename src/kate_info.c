/* Copyright (C) 2008 Vincent Penquerc'h.
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

/**
  \ingroup info
  Initializes a kate_info structure.
  When done, it should be clear using kate_info_clear.
  \param ki the kate_info structure to initialize
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_info_init(kate_info *ki)
{
  if (!ki) return KATE_E_INVALID_PARAMETER;

  ki->bitstream_version_major=KATE_BITSTREAM_VERSION_MAJOR;
  ki->bitstream_version_minor=KATE_BITSTREAM_VERSION_MINOR;

  ki->num_headers=0;

  /* default to a sensible granule encoding */
  ki->granule_shift=32;
  ki->gps_numerator=1000;
  ki->gps_denominator=1;

  ki->text_encoding=kate_utf8;
  ki->text_directionality=kate_l2r_t2b;
  ki->text_markup_type=kate_markup_none;

  ki->language=NULL;
  ki->category=NULL;

  ki->nregions=0;
  ki->regions=NULL;
  ki->nstyles=0;
  ki->styles=NULL;
  ki->ncurves=0;
  ki->curves=NULL;
  ki->nmotions=0;
  ki->motions=NULL;
  ki->npalettes=0;
  ki->palettes=NULL;
  ki->nbitmaps=0;
  ki->bitmaps=NULL;
  ki->nfont_ranges=0;
  ki->font_ranges=NULL;
  ki->nfont_mappings=0;
  ki->font_mappings=NULL;

  ki->original_canvas_width=0;
  ki->original_canvas_height=0;

  ki->remove_markup=0;
  ki->no_limits=0;
  ki->probe=0;

  return 0;
}

static void kate_make_fraction(kate_float resolution,kate_int32_t *numerator,kate_int32_t *denominator)
{
  if (resolution<1) {
    *numerator=(kate_int32_t)(1000/resolution+(kate_float)0.5);
    *denominator=1000;
  }
  else {
    *numerator=1000;
    *denominator=(kate_int32_t)(resolution*1000+(kate_float)0.5);
  }
}

/**
  \ingroup info
  Sets up the granule encoding parameters so that events may be timestamped with the given
  resolution, and may not last longer than the given lifetime.
  \param ki the kate_info structure to set the granule encoding parameters in.
  \param resolution the timestamping resolution, in seconds (eg, 0.001 means millisecond resolution)
  \param max_length the maximum time we need to represent, in seconds
  \param max_event_lifetime the maximum time an event may last, in seconds
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_info_set_granule_encoding(kate_info *ki,kate_float resolution,kate_float max_length,kate_float max_event_lifetime)
{
  kate_float offset_granules;
  unsigned char offset_bits,bits;
  kate_float base_min_resolution;

  if (!ki || resolution<=0 || max_event_lifetime<0) return KATE_E_INVALID_PARAMETER;

  offset_granules=max_event_lifetime/resolution;
  offset_bits=0;
  while (offset_granules>=1) {
    ++offset_bits;
    if (offset_bits>=64) return KATE_E_BAD_GRANULE;
    offset_granules/=2;
  }

  base_min_resolution=max_length;
  bits=offset_bits;
  while (bits<62) {
    ++bits;
    base_min_resolution/=2;
  }

  ki->granule_shift=offset_bits;
  kate_make_fraction(resolution,&ki->gps_numerator,&ki->gps_denominator);

  if (base_min_resolution<=resolution) {
    /* we can represent the required encoding */
    return 0;
  }
  else {
    /* we cannot represent the required encoding */
    return KATE_E_BAD_GRANULE;
  }
}

static int kate_replace_string(char **sptr,const char *s)
{
  size_t len;
  char *l=NULL;

  if (!sptr) return KATE_E_INVALID_PARAMETER;

  if (s) {
    len=strlen(s);
    l=(char*)kate_malloc(len+1);
    if (!l) return KATE_E_OUT_OF_MEMORY;
    memcpy(l,s,len+1);
  }

  if (*sptr) kate_free(*sptr);
  *sptr=l;

  return 0;
}

/**
  \ingroup info
  Sets the default language for this bitstream.
  \param ki the kate_info structure for the stream
  \param language the default language to set for this stream
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_info_set_language(kate_info *ki,const char *language)
{
  if (!ki) return KATE_E_INVALID_PARAMETER;
  return kate_replace_string(&ki->language,language);
}

/**
  \ingroup info
  Sets the default text directionality for this bitstream
  \param ki the kate_info structure for the stream
  \param text_directionality the default text directionality to set for this stream
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_info_set_text_directionality(kate_info *ki,kate_text_directionality text_directionality)
{
  if (!ki) return KATE_E_INVALID_PARAMETER;
  ki->text_directionality=text_directionality;
  return 0;
}

/**
  \ingroup info
  Sets the category for this bitstream.
  \param ki the kate_info structure for the stream
  \param category the stream's category
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_info_set_category(kate_info *ki,const char *category)
{
  if (!ki) return KATE_E_INVALID_PARAMETER;
  return kate_replace_string(&ki->category,category);
}

/**
  \ingroup info
  Sets the default text markup type for this bitstream
  \param ki the kate_info structure for the stream
  \param text_markup_type the default text markup type to set for this stream
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_info_set_markup_type(kate_info *ki,kate_markup_type text_markup_type)
{
  if (!ki) return KATE_E_INVALID_PARAMETER;
  ki->text_markup_type=text_markup_type;
  return 0;
}

/**
  \ingroup info
  Sets the size of the canvas this stream is being authored for
  \param ki the kate_info structure for the stream
  \param width the width of the canvas
  \param height the height of the canvas
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_info_set_original_canvas_size(kate_info *ki,size_t width,size_t height)
{
  if (!ki) return KATE_E_INVALID_PARAMETER;
  ki->original_canvas_width=width;
  ki->original_canvas_height=height;
  return 0;
}

static int kate_info_add_item(kate_info *ki,size_t *nitems,void ***items,void *item)
{
  void **newitems;

  if (!ki || !nitems || !items || !item) return KATE_E_INVALID_PARAMETER;

  newitems=(void**)kate_realloc(*items,((*nitems)+1)*sizeof(void*));
  if (!newitems) return KATE_E_OUT_OF_MEMORY;
  *items=newitems;
  newitems[*nitems]=item;
  ++*nitems;

  return 0;
}

/**
  \ingroup info
  Adds a predefined region so it can be referred to by index later.
  \param ki the kate_info structure for the stream
  \param kr the region to add
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_info_add_region(kate_info *ki,kate_region *kr)
{
  return kate_info_add_item(ki,&ki->nregions,(void***)(char*)&ki->regions,kr);
}

/**
  \ingroup info
  Adds a predefined style so it can be referred to by index later.
  Adds a predefined region so it can be referred to by index later.
  \param ki the kate_info structure for the stream
  \param ks the style to add
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_info_add_style(kate_info *ki,kate_style *ks)
{
  return kate_info_add_item(ki,&ki->nstyles,(void***)(char*)&ki->styles,ks);
}

/**
  \ingroup info
  Adds a predefined curve so it can be referred to by index later.
  \param ki the kate_info structure for the stream
  \param kc the curve to add
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_info_add_curve(kate_info *ki,kate_curve *kc)
{
  return kate_info_add_item(ki,&ki->ncurves,(void***)(char*)&ki->curves,kc);
}

/**
  \ingroup info
  Adds a predefined motion so it can be referred to by index later.
  \param ki the kate_info structure for the stream
  \param km the motion to add
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_info_add_motion(kate_info *ki,kate_motion *km)
{
  return kate_info_add_item(ki,&ki->nmotions,(void***)(char*)&ki->motions,km);
}

/**
  \ingroup info
  Adds a predefined palette so it can be referred to by index later.
  \param ki the kate_info structure for the stream
  \param kp the palette to add
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_info_add_palette(kate_info *ki,kate_palette *kp)
{
  return kate_info_add_item(ki,&ki->npalettes,(void***)(char*)&ki->palettes,kp);
}

/**
  \ingroup info
  Adds a predefined bitmap so it can be referred to by index later.
  \param ki the kate_info structure for the stream
  \param kb the bitmap to add
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_info_add_bitmap(kate_info *ki,kate_bitmap *kb)
{
  return kate_info_add_item(ki,&ki->nbitmaps,(void***)(char*)&ki->bitmaps,kb);
}

/**
  \ingroup info
  Adds a predefined font range so it can be referred to by index later.
  \param ki the kate_info structure for the stream
  \param kfr the font range to add
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_info_add_font_range(kate_info *ki,kate_font_range *kfr)
{
  return kate_info_add_item(ki,&ki->nfont_ranges,(void***)(char*)&ki->font_ranges,kfr);
}

/**
  \ingroup info
  Adds a predefined font mapping so it can be referred to by index later.
  \param ki the kate_info structure for the stream
  \param kfm the font mapping to add
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_info_add_font_mapping(kate_info *ki,kate_font_mapping *kfm)
{
  return kate_info_add_item(ki,&ki->nfont_mappings,(void***)(char*)&ki->font_mappings,kfm);
}

/**
  \ingroup info
  Checks whether the given language matches (fully or partially) the language
  described in the kate_info structure.
  \param ki the kate_info structure for the stream
  \param language the language to check against
  \returns 0 success, but the language doesn't match
  \returns 1 success, the language matches perfectly
  \returns 2 success, the language matches partially
  \returns KATE_E_* error
  */
int kate_info_matches_language(const kate_info *ki,const char *language)
{
  char *sep;
  if (!ki) return KATE_E_INVALID_PARAMETER;

  if (!language) return 1; /* if we ask for "any" language, we match */
  if (!ki->language) return 1; /* if the stream has no set language, it matches all */

  if (!kate_ascii_strncasecmp(ki->language,language,0xffffffff)) return 1; /* perfect match */

  /* if we specify a language with no subtag, it matches any subtag of the same language */
  sep=strpbrk(language,"-_");
  if (sep) {
    size_t bytes=sep-language;
    if (!kate_ascii_strncasecmp(ki->language,language,bytes)) return 2; /* partial match */
  }

  return 0; /* doesn't match */
}

/**
  \ingroup info
  Requests whether future events will strip text of markup or not.
  \param ki the kate_info structure for the stream
  \param flag if zero, markup will be kept, else if will be removed
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_info_remove_markup(kate_info *ki,int flag)
{
  if (!ki) return KATE_E_INVALID_PARAMETER;
  ki->remove_markup=flag;
  return 0;
}

/**
  \ingroup info
  Requests whether large quantities of various things should be rejected or not,
  as a simple defense against DOS.
  \param ki the kate_info structure for the stream
  \param flag if zero, values deemed arbitrarily too large will be treated as an error, else they will be accepted
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_info_no_limits(kate_info *ki,int flag)
{
  if (!ki) return KATE_E_INVALID_PARAMETER;
  ki->no_limits=flag;
  return 0;
}

/**
  \ingroup info
  Clears a kate_info structure previously initialized with kate_info_init.
  It may not be used again until kate_info_init is called again on it.
  \param ki the kate_info structure to clear, must have been initialized with kate_info_init
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_info_clear(kate_info *ki)
{
  size_t n,l;

  if (!ki) return KATE_E_INVALID_PARAMETER;

  if (ki->bitmaps) {
    for (n=0;n<ki->nbitmaps;++n) {
      kate_free(ki->bitmaps[n]->pixels);
      kate_free(ki->bitmaps[n]);
    }
    kate_free(ki->bitmaps);
  }
  if (ki->palettes) {
    for (n=0;n<ki->npalettes;++n) {
      kate_free(ki->palettes[n]->colors);
      kate_free(ki->palettes[n]);
    }
    kate_free(ki->palettes);
  }
  if (ki->motions) {
    kate_motion_destroy(ki,ki->motions,NULL,ki->nmotions,1);
  }
  if (ki->curves) {
    for (n=0;n<ki->ncurves;++n) {
      kate_free(ki->curves[n]->pts);
      kate_free(ki->curves[n]);
    }
    kate_free(ki->curves);
  }
  if (ki->regions) {
    for (n=0;n<ki->nregions;++n) kate_free(ki->regions[n]);
    kate_free(ki->regions);
  }
  if (ki->styles) {
    for (n=0;n<ki->nstyles;++n) {
      kate_style *ks=ki->styles[n];
      if (ks->font) kate_free(ks->font);
      kate_free(ks);
    }
    kate_free(ki->styles);
  }
  if (ki->language) kate_free(ki->language);
  if (ki->category) kate_free(ki->category);
  if (ki->font_mappings) {
    for (n=0;n<ki->nfont_mappings;++n) {
      kate_font_mapping *kfm=ki->font_mappings[n];
      if (kfm->ranges) {
        for (l=0;l<kfm->nranges;++l) {
          int idx=kate_find_font_range(ki,kfm->ranges[l]);
          if (idx<0) kate_free(kfm->ranges[l]);
        }
        kate_free(kfm->ranges);
      }
      kate_free(kfm);
    }
    kate_free(ki->font_mappings);
  }
  if (ki->font_ranges) {
    for (n=0;n<ki->nfont_ranges;++n) kate_free(ki->font_ranges[n]);
    kate_free(ki->font_ranges);
  }

  return 0;
}

static int kate_find_item(const void *item,const void **items,size_t nitems)
{
  size_t n;

  if (!item) return KATE_E_INVALID_PARAMETER;

  if (!items) return KATE_E_NOT_FOUND; /* if nothing in the list, it may be NULL */
  for (n=0;n<nitems;++n) if (item==items[n]) return n;
  return KATE_E_NOT_FOUND;
}

int kate_find_region(const kate_info *ki,const kate_region *kr)
{
  return kate_find_item(kr,(const void**)ki->regions,ki->nregions);
}

int kate_find_style(const kate_info *ki,const kate_style *ks)
{
  return kate_find_item(ks,(const void**)ki->styles,ki->nstyles);
}

int kate_find_curve(const kate_info *ki,const kate_curve *kc)
{
  return kate_find_item(kc,(const void**)ki->curves,ki->ncurves);
}

int kate_find_motion(const kate_info *ki,const kate_motion *km)
{
  return kate_find_item(km,(const void**)ki->motions,ki->nmotions);
}

int kate_find_palette(const kate_info *ki,const kate_palette *kp)
{
  return kate_find_item(kp,(const void**)ki->palettes,ki->npalettes);
}

int kate_find_bitmap(const kate_info *ki,const kate_bitmap *kb)
{
  return kate_find_item(kb,(const void**)ki->bitmaps,ki->nbitmaps);
}

int kate_find_font_range(const kate_info *ki,const kate_font_range *kfr)
{
  return kate_find_item(kfr,(const void**)ki->font_ranges,ki->nfont_ranges);
}

int kate_find_font_mapping(const kate_info *ki,const kate_font_mapping *kfm)
{
  return kate_find_item(kfm,(const void**)ki->font_mappings,ki->nfont_mappings);
}

