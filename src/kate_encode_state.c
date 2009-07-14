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
#include <string.h>
#include "kate/kate.h"
#include "kate_meta.h"
#include "kate_encode_state.h"

static void kate_encode_state_init_helper(kate_encode_state *kes)
{
  kes->motions=NULL;
  kes->destroy_motions=NULL;
  kes->motion_indices=NULL;
  kes->nmotions=0;

  kes->bitmaps=NULL;
  kes->bitmap_indices=NULL;
  kes->nbitmaps=0;

  kes->meta=NULL;

  kes->overrides.region_index=-1;
  kes->overrides.region=NULL;
  kes->overrides.style_index=-1;
  kes->overrides.style=NULL;
  kes->overrides.secondary_style_index=-1;
  kes->overrides.secondary_style=NULL;

  kes->overrides.palette_index=-1;
  kes->overrides.palette=NULL;
  kes->overrides.bitmap_index=-1;
  kes->overrides.bitmap=NULL;

  kes->overrides.language=NULL;

  kes->overrides.font_mapping_index=-1;
}

kate_encode_state *kate_encode_state_create(const kate_info *ki)
{
  kate_encode_state *kes=(kate_encode_state*)kate_malloc(sizeof(kate_encode_state));
  if (kes) {
    kate_pack_writeinit(&kes->kpb);

    kes->ki=ki;

    kes->id=0;

    kes->granulepos=0;
    kes->packetno=-1;

    kes->furthest_granule=0;

    kes->eos=0;

    kes->ntimings=0;
    kes->timings=NULL;

    kate_encode_state_init_helper(kes);
  }
  return kes;
}

int kate_encode_state_clear_overrides(kate_encode_state *kes)
{
  if (!kes) return KATE_E_INVALID_PARAMETER;
  if (!kes->ki) return KATE_E_INIT;

  if (kes->motions) {
    kate_motion_destroy(kes->ki,kes->motions,kes->destroy_motions,kes->nmotions,0);
  }
  if (kes->destroy_motions) {
    kate_free(kes->destroy_motions);
  }
  if (kes->motion_indices) {
    kate_free(kes->motion_indices);
  }

  if (kes->meta) {
    kate_meta_destroy(kes->meta);
  }

  if (kes->bitmaps) {
    kate_free(kes->bitmaps);
  }
  if (kes->bitmap_indices) {
    kate_free(kes->bitmap_indices);
  }

  if (kes->overrides.language) {
    kate_free(kes->overrides.language);
  }

  kes->overrides.text_encoding=kes->ki->text_encoding;
  kes->overrides.text_directionality=kes->ki->text_directionality;
  kes->overrides.text_markup_type=kes->ki->text_markup_type;

  kate_encode_state_init_helper(kes);

  return 0;
}

static int kate_encode_state_add_motion_or_index(kate_encode_state *kes,kate_motion *km,size_t motion,int destroy)
{
  kate_motion **motions;
  int *destroy_motions;
  size_t *motion_indices;
  kate_motion_semantics semantics;
  size_t n;
  int ret;

  if (!kes) return KATE_E_INVALID_PARAMETER;
  if (!kes->ki) return KATE_E_INIT;
  if (!km && motion>=kes->ki->nmotions) return KATE_E_INVALID_PARAMETER;

  ret=kate_check_add_overflow(kes->nmotions,1,NULL);
  if (ret<0) return ret;

  /* check if we have that semantic already */
  semantics=km?km->semantics:kes->ki->motions[motion]->semantics;
  for (n=0;n<kes->nmotions;++n) {
    const kate_motion *km2=kes->motions[n];
    if (!km2) km2=kes->ki->motions[kes->motion_indices[n]];
    if (km2->semantics==semantics) return KATE_E_LIMIT;
  }

  motions=(kate_motion**)kate_checked_realloc(kes->motions,(kes->nmotions+1),sizeof(kate_motion*));
  if (!motions) return KATE_E_OUT_OF_MEMORY;
  kes->motions=motions;

  destroy_motions=(int*)kate_checked_realloc(kes->destroy_motions,(kes->nmotions+1),sizeof(int));
  if (!destroy_motions) return KATE_E_OUT_OF_MEMORY;
  kes->destroy_motions=destroy_motions;

  motion_indices=(size_t*)kate_checked_realloc(kes->motion_indices,(kes->nmotions+1),sizeof(size_t));
  if (!motion_indices) return KATE_E_OUT_OF_MEMORY;
  kes->motion_indices=motion_indices;

  kes->motions[kes->nmotions]=km;
  kes->destroy_motions[kes->nmotions]=destroy;
  kes->motion_indices[kes->nmotions]=motion;
  ++kes->nmotions;

  return 0;
}

int kate_encode_state_add_motion(kate_encode_state *kes,kate_motion *km,int destroy)
{
  if (!kes || !km) return KATE_E_INVALID_PARAMETER;
  return kate_encode_state_add_motion_or_index(kes,km,0,destroy);
}

int kate_encode_state_add_motion_index(kate_encode_state *kes,size_t motion)
{
  if (!kes) return KATE_E_INVALID_PARAMETER;
  return kate_encode_state_add_motion_or_index(kes,NULL,motion,0);
}

static int kate_encode_state_add_bitmap_or_index(kate_encode_state *kes,const kate_bitmap *kb,size_t bitmap)
{
  const kate_bitmap **bitmaps;
  size_t *bitmap_indices;
  int ret;

  if (!kes) return KATE_E_INVALID_PARAMETER;
  if (!kes->ki) return KATE_E_INIT;
  if (!kb && bitmap>=kes->ki->nbitmaps) return KATE_E_INVALID_PARAMETER;

  ret=kate_check_add_overflow(kes->nbitmaps,1,NULL);
  if (ret<0) return ret;

  bitmaps=(const kate_bitmap**)kate_checked_realloc(kes->bitmaps,(kes->nbitmaps+1),sizeof(const kate_bitmap*));
  if (!bitmaps) return KATE_E_OUT_OF_MEMORY;
  kes->bitmaps=bitmaps;

  bitmap_indices=(size_t*)kate_checked_realloc(kes->bitmap_indices,(kes->nbitmaps+1),sizeof(size_t));
  if (!bitmap_indices) return KATE_E_OUT_OF_MEMORY;
  kes->bitmap_indices=bitmap_indices;

  kes->bitmaps[kes->nbitmaps]=kb;
  kes->bitmap_indices[kes->nbitmaps]=bitmap;
  ++kes->nbitmaps;

  return 0;
}

int kate_encode_state_add_bitmap(kate_encode_state *kes,const kate_bitmap *kb)
{
  if (!kes || !kb) return KATE_E_INVALID_PARAMETER;
  return kate_encode_state_add_bitmap_or_index(kes,kb,0);
}

int kate_encode_state_add_bitmap_index(kate_encode_state *kes,size_t bitmap)
{
  if (!kes) return KATE_E_INVALID_PARAMETER;
  return kate_encode_state_add_bitmap_or_index(kes,NULL,bitmap);
}

int kate_encode_state_merge_meta(kate_encode_state *kes,kate_meta *km)
{
  int ret;

  if (!kes || !km) return KATE_E_INVALID_PARAMETER;

  if (!kes->meta) {
    ret=kate_meta_create(&kes->meta);
    if (ret<0) return ret;
  }
  return kate_meta_merge(kes->meta,km);
}

int kate_encode_state_add_meta(kate_encode_state *kes,const kate_meta *km)
{
  kate_meta *tmp;
  int ret;

  if (!kes || !km) return KATE_E_INVALID_PARAMETER;

  ret=kate_meta_create_copy(&tmp,km);
  if (ret<0) return ret;
  ret=kate_encode_state_merge_meta(kes,tmp);
  if (ret<0) {
    kate_meta_destroy(tmp);
  }
  return ret;
}

int kate_encode_state_destroy(kate_encode_state *kes)
{
  size_t n;

  if (!kes) return KATE_E_INVALID_PARAMETER;

  kate_pack_writeclear(&kes->kpb);
  if (kes->timings) {
    for (n=0;n<kes->ntimings;++n) {
      if (kes->timings[n].original_data) kate_free(kes->timings[n].original_data);
      if (kes->timings[n].repeat_data) kate_free(kes->timings[n].repeat_data);
    }
    kate_free(kes->timings);
  }
  if (kes->motions) kate_free(kes->motions);
  if (kes->destroy_motions) kate_free(kes->destroy_motions);
  if (kes->motion_indices) kate_free(kes->motion_indices);
  if (kes->bitmaps) kate_free(kes->bitmaps);
  if (kes->bitmap_indices) kate_free(kes->bitmap_indices);
  if (kes->meta) kate_meta_destroy(kes->meta);
  if (kes->overrides.language) kate_free(kes->overrides.language);
  kate_free(kes);

  return 0;
}

int kate_encode_state_add_event(kate_encode_state *kes,kate_int64_t start,kate_int64_t end)
{
  kate_event_timing *new_timings;
  int ret;

  if (!kes) return KATE_E_INVALID_PARAMETER;
  if (start<0 || end<0 || end<start) return KATE_E_INVALID_PARAMETER;

  ret=kate_check_add_overflow(kes->ntimings,1,NULL);
  if (ret<0) return ret;

  new_timings=(kate_event_timing*)kate_checked_realloc(kes->timings,(kes->ntimings+1),sizeof(kate_event_timing));
  if (!new_timings) return KATE_E_OUT_OF_MEMORY;
  kes->timings=new_timings;

  kes->timings[kes->ntimings].start=start;
  kes->timings[kes->ntimings].end=end;
  kes->timings[kes->ntimings].id=kes->id;
  kes->timings[kes->ntimings].repeat=start;
  kes->timings[kes->ntimings].original_size=0;
  kes->timings[kes->ntimings].original_data=NULL;
  kes->timings[kes->ntimings].repeat_size=0;
  kes->timings[kes->ntimings].repeat_data=NULL;
  ++kes->ntimings;

  return 0;
}

int kate_encode_state_get_earliest_event(kate_encode_state *kes,kate_int64_t *start,kate_int64_t *end)
{
  size_t n;

  if (!kes || !start) return KATE_E_INVALID_PARAMETER; /* end may be NULL */
  if (!kes->ntimings) return KATE_E_NOT_FOUND;

  for (n=0;n<kes->ntimings;++n) {
    /* repeat will be either start, or later if a repeat has been emitted */
    if (n==0 || kes->timings[n].repeat<*start) {
      *start=kes->timings[n].repeat;
      if (end) *end=kes->timings[n].end;
    }
  }

  return 0;
}

int kate_encode_state_get_latest_event(kate_encode_state *kes,kate_int64_t *start,kate_int64_t *end)
{
  size_t n;

  if (!kes || !end) return KATE_E_INVALID_PARAMETER; /* start may be NULL */
  if (!kes->ntimings) return KATE_E_NOT_FOUND;

  for (n=0;n<kes->ntimings;++n) {
    if (n==0 || kes->timings[n].end>*end) {
      if (start) *start=kes->timings[n].start;
      *end=kes->timings[n].end;
    }
  }

  return 0;
}

int kate_encode_state_trim_events(kate_encode_state *kes,kate_int64_t t)
{
  size_t n;

  if (!kes) return KATE_E_INVALID_PARAMETER;

  for (n=0;n<kes->ntimings;++n) {
    if (kes->timings[n].end<=t) {
      if (kes->timings[n].original_data) kate_free(kes->timings[n].original_data);
      if (kes->timings[n].repeat_data) kate_free(kes->timings[n].repeat_data);
      kes->timings[n--]=kes->timings[--kes->ntimings];
    }
  }

  return 0;
}

int kate_encode_state_save_event_buffer(kate_encode_state *kes,size_t size,const void *data)
{
  kate_event_timing *ket;

  if (!kes) return KATE_E_INVALID_PARAMETER;
  if (!data || size==0) return KATE_E_INVALID_PARAMETER;
  if (kes->ntimings==0) return KATE_E_INIT;

  /* store in the last added slot */
  ket=&kes->timings[kes->ntimings-1];

  /* initialized already ? */
  if (ket->original_data || ket->original_size) return KATE_E_INIT;
  if (ket->repeat_data || ket->repeat_size) return KATE_E_INIT;

  /* increment id */
  ++kes->id;
  if (kes->id<0) return KATE_E_LIMIT; /* 2 billion or so events limit */

  /* store packet data */
  ket->original_data=kate_malloc(size);
  if (!ket->original_data) return KATE_E_OUT_OF_MEMORY;
  memcpy(ket->original_data,data,size);
  ket->original_size=size;

  return 0;
}

int kate_encode_state_get_repeat(kate_encode_state *kes,kate_int64_t t,kate_int64_t threshold,kate_packet *kp)
{
  size_t n;
  kate_event_timing *ket;
  unsigned char packet_type;

  if (!kes) return KATE_E_INVALID_PARAMETER; /* kp may be NULL */

  for (n=0;n<kes->ntimings;++n) {
    ket=kes->timings+n;
    /* old enough ? */
    if (threshold==0) {
      if (ket->repeat>=t-threshold) continue;
    }
    else {
      if (ket->repeat>t-threshold) continue;
    }
    /* has data ? */
    if (!ket->original_data) continue;
    /* is a text packet ? */
    if (ket->original_size<1+8*3) continue;
    packet_type=((const unsigned char*)ket->original_data)[0];
    if (packet_type!=0x00) continue;

    /* we found an older one */
    ket->repeat = t;

    /* create a repeat packet if there is none yet */
    if (!ket->repeat_data) {
      /* we're in luck, it's the same size */
      ket->repeat_data=kate_malloc(ket->original_size);
      if (!ket->repeat_data) return KATE_E_OUT_OF_MEMORY;
      memcpy(ket->repeat_data,ket->original_data,ket->original_size);
      ket->repeat_size=ket->original_size;
      /* set packet type to repeat */
      ((unsigned char*)ket->repeat_data)[0]=0x02;
    }

    kate_packet_init(kp,ket->repeat_size,ket->repeat_data);

    return 1;
  }

  return 0;
}

