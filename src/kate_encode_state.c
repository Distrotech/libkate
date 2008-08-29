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
#include "kate/kate.h"
#include "kate_encode_state.h"

static void kate_encode_state_init_helper(kate_encode_state *kes)
{
  kes->id=-1;

  kes->motions=NULL;
  kes->destroy_motions=NULL;
  kes->motion_indices=NULL;
  kes->nmotions=0;

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

kate_encode_state *kate_encode_state_create(void)
{
  kate_encode_state *kes=(kate_encode_state*)kate_malloc(sizeof(kate_encode_state));
  if (kes) {
    kate_pack_writeinit(&kes->kpb);

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

int kate_encode_state_clear_overrides(kate_encode_state *kes,const kate_info *ki)
{
  if (!kes || !ki) return KATE_E_INVALID_PARAMETER;

  if (kes->motions) {
    kate_motion_destroy(ki,kes->motions,kes->destroy_motions,kes->nmotions,0);
  }
  if (kes->destroy_motions) {
    kate_free(kes->destroy_motions);
  }
  if (kes->motion_indices) {
    kate_free(kes->motion_indices);
  }

  if (kes->overrides.language) {
    kate_free(kes->overrides.language);
  }

  kes->overrides.text_encoding=ki->text_encoding;
  kes->overrides.text_directionality=ki->text_directionality;
  kes->overrides.text_markup_type=ki->text_markup_type;

  kate_encode_state_init_helper(kes);

  return 0;
}

static int kate_encode_state_add_motion_or_index(kate_encode_state *kes,kate_motion *km,size_t motion,int destroy)
{
  kate_motion **motions;
  int *destroy_motions;
  size_t *motion_indices;

  if (!kes) return KATE_E_INVALID_PARAMETER;

  motions=(kate_motion**)kate_realloc(kes->motions,(kes->nmotions+1)*sizeof(kate_motion*));
  if (!motions) return KATE_E_OUT_OF_MEMORY;
  kes->motions=motions;

  destroy_motions=(int*)kate_realloc(kes->destroy_motions,(kes->nmotions+1)*sizeof(int));
  if (!destroy_motions) return KATE_E_OUT_OF_MEMORY;
  kes->destroy_motions=destroy_motions;

  motion_indices=(size_t*)kate_realloc(kes->motion_indices,(kes->nmotions+1)*sizeof(size_t));
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

int kate_encode_state_destroy(kate_encode_state *kes)
{
  if (!kes) return KATE_E_INVALID_PARAMETER;

  kate_pack_writeclear(&kes->kpb);
  if (kes->timings) kate_free(kes->timings);
  if (kes->motions) kate_free(kes->motions);
  if (kes->destroy_motions) kate_free(kes->destroy_motions);
  if (kes->motion_indices) kate_free(kes->motion_indices);
  if (kes->overrides.language) kate_free(kes->overrides.language);
  kate_free(kes);

  return 0;
}

int kate_encode_state_add_event(kate_encode_state *kes,kate_float start,kate_float end)
{
  kate_event_timing *new_timings;

  if (!kes) return KATE_E_INVALID_PARAMETER;
  if (start<0 || end<0 || end<start) return KATE_E_INVALID_PARAMETER;

  new_timings=(kate_event_timing*)kate_realloc(kes->timings,(kes->ntimings+1)*sizeof(kate_event_timing));
  if (!new_timings) return KATE_E_OUT_OF_MEMORY;
  kes->timings=new_timings;

  kes->timings[kes->ntimings].start=start;
  kes->timings[kes->ntimings].end=end;
  ++kes->ntimings;

  return 0;
}

int kate_encode_state_get_earliest_event(kate_encode_state *kes,kate_float *start,kate_float *end)
{
  size_t n;

  if (!kes || !start) return KATE_E_INVALID_PARAMETER; /* end may be NULL */
  if (!kes->ntimings) return KATE_E_NOT_FOUND;

  for (n=0;n<kes->ntimings;++n) {
    if (n==0 || kes->timings[n].start<*start) {
      *start=kes->timings[n].start;
      if (end) *end=kes->timings[n].end;
    }
  }

  return 0;
}

int kate_encode_state_get_latest_event(kate_encode_state *kes,kate_float *start,kate_float *end)
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

int kate_encode_state_trim_events(kate_encode_state *kes,kate_float t)
{
  size_t n;

  if (!kes) return KATE_E_INVALID_PARAMETER;

  for (n=0;n<kes->ntimings;++n) {
    if (kes->timings[n].end<=t) {
      kes->timings[n--]=kes->timings[--kes->ntimings];
    }
  }

  return 0;
}

