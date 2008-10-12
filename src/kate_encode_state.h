/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#ifndef KATE_kate_encode_state_h_GUARD
#define KATE_kate_encode_state_h_GUARD

#include "kate_internal.h"
#include "kate_bitwise.h"
#include "kate/kate.h"

typedef struct kate_event_timing {
  kate_float start;
  kate_float end;
} kate_event_timing;

typedef struct kate_encode_state {
  kate_pack_buffer kpb;

  const kate_info *ki;

  kate_int32_t id;

  kate_int64_t granulepos;
  kate_int64_t packetno;

  kate_int64_t furthest_granule;

  size_t nmotions;
  kate_motion **motions;
  int *destroy_motions;
  size_t *motion_indices;

  size_t nbitmaps;
  const kate_bitmap **bitmaps;
  size_t *bitmap_indices;

  int eos;

  size_t ntimings;
  kate_event_timing *timings;

  struct {
    char *language;
    kate_text_encoding text_encoding;
    kate_text_directionality text_directionality;
    kate_markup_type text_markup_type;
    int region_index;
    const kate_region *region;
    int style_index;
    const kate_style *style;
    int secondary_style_index;
    const kate_style *secondary_style;
    int font_mapping_index;

    const kate_palette *palette;
    int palette_index;
    const kate_bitmap *bitmap;
    int bitmap_index;
  } overrides;
} kate_encode_state;

extern kate_encode_state *kate_encode_state_create(const kate_info *ki) kate_internal;
extern int kate_encode_state_clear_overrides(kate_encode_state *kes) kate_internal;
extern int kate_encode_state_add_motion(kate_encode_state *kes,kate_motion *km,int destroy) kate_internal;
extern int kate_encode_state_add_motion_index(kate_encode_state *kes,size_t motion) kate_internal;
extern int kate_encode_state_add_bitmap(kate_encode_state *kes,const kate_bitmap *kb) kate_internal;
extern int kate_encode_state_add_bitmap_index(kate_encode_state *kes,size_t bitmap) kate_internal;
extern int kate_encode_state_destroy(kate_encode_state *kes) kate_internal;

extern int kate_encode_state_add_event(kate_encode_state *kes,kate_float start,kate_float end) kate_internal;
extern int kate_encode_state_get_earliest_event(kate_encode_state *kes,kate_float *start,kate_float *end) kate_internal;
extern int kate_encode_state_get_latest_event(kate_encode_state *kes,kate_float *start,kate_float *end) kate_internal;
extern int kate_encode_state_trim_events(kate_encode_state *kes,kate_float t) kate_internal;

#endif
