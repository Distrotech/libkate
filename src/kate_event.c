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

static int kate_event_init(kate_event *ev,const kate_info *ki)
{
  if (!ev || !ki) return KATE_E_INVALID_PARAMETER;

  ev->start_time=-1;
  ev->duration=-1;
  ev->backlink=-1;
  ev->id=-1;

  ev->text_encoding=ki->text_encoding;
  ev->text_directionality=ki->text_directionality;
  ev->text_markup_type=ki->text_markup_type;
  ev->language=NULL;
  ev->text=NULL;
  ev->len=0;

  ev->nmotions=0;
  ev->motions=NULL;

  ev->region=NULL;
  ev->style=NULL;
  ev->secondary_style=NULL;
  ev->font_mapping=NULL;
  ev->palette=NULL;
  ev->bitmap=NULL;

  ev->nbitmaps=0;
  ev->bitmaps=NULL;

  ev->meta=NULL;

  /* internal */
  ev->ki=ki;
  ev->trackers=0;

  return 0;
}

kate_event *kate_event_create(const kate_info *ki)
{
  kate_event *ev;

  if (!ki) return NULL;

  ev=(kate_event*)kate_malloc(sizeof(kate_event));
  if (!ev) return NULL;
  if (kate_event_init(ev,ki)<0) {
    kate_free(ev);
    return NULL;
  }

  return ev;
}

int kate_event_destroy(kate_event *ev)
{
  size_t n;

  if (!ev) return KATE_E_INVALID_PARAMETER;
  if (!ev->ki) return KATE_E_INIT;
  if (ev->trackers) return KATE_E_INIT;

  if (ev->language) kate_free(ev->language);
  kate_free(ev->text);
  if (ev->motions) {
    kate_motion_destroy(ev->ki,ev->motions,NULL,ev->nmotions,0);
  }

  if (ev->region && kate_find_region(ev->ki,ev->region)<0) {
    kate_free(ev->region);
  }
  if (ev->style && kate_find_style(ev->ki,ev->style)<0) {
    if (ev->style->font) kate_free(ev->style->font);
    kate_free(ev->style);
  }
  if (ev->secondary_style && kate_find_style(ev->ki,ev->secondary_style)<0) {
    kate_free(ev->secondary_style);
  }
  if (ev->font_mapping && kate_find_font_mapping(ev->ki,ev->font_mapping)<0) {
    kate_free(ev->font_mapping);
  }
  if (ev->palette && kate_find_palette(ev->ki,ev->palette)<0) {
    kate_free(ev->palette->colors);
    kate_free(ev->palette);
  }

  if (ev->bitmaps) {
    for (n=0;n<ev->nbitmaps;++n) {
      if (ev->bitmaps[n] && kate_find_bitmap(ev->ki,ev->bitmaps[n])<0) {
        kate_free(ev->bitmaps[n]->pixels);
        kate_free(ev->bitmaps[n]);
      }
    }
    kate_free(ev->bitmaps);
  }
  if (ev->bitmap && kate_find_bitmap(ev->ki,ev->bitmap)<0) {
    kate_free(ev->bitmap->pixels);
    kate_free(ev->bitmap);
  }

  if (ev->meta) kate_meta_destroy(ev->meta);

  kate_free(ev);

  return 0;
}

int kate_event_track(kate_event *ev)
{
  if (!ev) return KATE_E_INVALID_PARAMETER;

  ++ev->trackers;

  return 0;
}

int kate_event_release(kate_event *ev)
{
  if (!ev) return KATE_E_INVALID_PARAMETER;
  if (ev->trackers==0) return KATE_E_INIT;

  if (--ev->trackers==0) kate_event_destroy(ev);

  return 0;
}

