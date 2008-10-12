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

typedef struct kate_tracker_internal {
  size_t nglyphs;
} kate_tracker_internal;

/**
  \ingroup tracker
  Initializes a tracker with the given event.
  When done, it should be cleared using kate_tracker_clear.
  \param kin the tracker to initialize
  \param ki the kate_info structure for the stream
  \param ev the event to track
  */
int kate_tracker_init(kate_tracker *kin,const kate_info *ki,kate_const kate_event *ev)
{
  kate_tracker_internal *internal;
  const char *text;
  size_t rlen0;

  if (!kin || !ki || !ev) return KATE_E_INVALID_PARAMETER;

  internal=(kate_tracker_internal*)kate_malloc(sizeof(kate_tracker_internal));
  if (!internal) return KATE_E_OUT_OF_MEMORY;

  kin->ki=ki;
  kin->event=ev;
  kin->internal=internal;
  kate_event_track(kin->event);

  kin->internal->nglyphs=0;
  text=kin->event->text;
  rlen0=kin->event->len0;
  while (kate_text_get_character(kin->event->text_encoding,&text,&rlen0)>0)
    ++kin->internal->nglyphs;

  return 0;
}

/**
  \ingroup tracker
  Clears a tracker.
  \param kin the tracker to clear, must have been initialized with kate_tracker_init
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_tracker_clear(kate_tracker *kin)
{
  if (!kin) return KATE_E_INVALID_PARAMETER;
  if (!kin->event) return KATE_E_INIT;
  if (!kin->internal) return KATE_E_INIT;

  kate_free(kin->internal);
  kate_event_release(kin->event);

  return 0;
}

#define LERP(t,from,to) (((1-(t))*(from))+((t)*(to)))

/**
  \ingroup tracker
  Morphs between two styles.
  t should be between 0 and 1
  \param style the style that will hold the morphed style
  \param t the amount of morping from the from style (eg, 0 yields from, 1 yields to)
  \param from the style to morph from
  \param to the style to morph to
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_tracker_morph_styles(kate_style *style,kate_float t,const kate_style *from,const kate_style *to)
{
  if (!style || !from || !to) return KATE_E_INVALID_PARAMETER;
  if (t<(kate_float)-0.001 || t>(kate_float)1.001) return KATE_E_INVALID_PARAMETER;

  /* clamp, just in case */
  if (t<0) t=0;
  if (t>1) t=1;

#define MORPH(field) style->field=LERP(t,from->field,to->field)
#define MORPHINT(field) style->field=(int)((kate_float)0.5+LERP(t,from->field,to->field))
  MORPH(halign);
  MORPH(valign);
  MORPHINT(text_color.r);
  MORPHINT(text_color.g);
  MORPHINT(text_color.b);
  MORPHINT(text_color.a);
  MORPHINT(background_color.r);
  MORPHINT(background_color.g);
  MORPHINT(background_color.b);
  MORPHINT(background_color.a);
  MORPHINT(draw_color.r);
  MORPHINT(draw_color.g);
  MORPHINT(draw_color.b);
  MORPHINT(draw_color.a);
  MORPHINT(font_metric); /* doesn't make much sense, caveat streamor */
  MORPH(font_width);
  MORPH(font_height);
  MORPHINT(margin_metric); /* doesn't make much sense, caveat streamor */
  MORPH(left_margin);
  MORPH(top_margin);
  MORPH(right_margin);
  MORPH(bottom_margin);
  MORPHINT(bold);
  MORPHINT(italics);
  MORPHINT(underline);
  MORPHINT(strike);
  MORPHINT(justify);
  MORPHINT(wrap_mode); /* doesn't make much sense, caveat streamor */
  style->font=(t<(kate_float)0.5)?from->font:to->font;
#undef MORPHINT
#undef MORPH

  return 0;
}

/**
  \ingroup tracker
  Remaps a point according to the given mappings
  \param kin the tracker to use for the remapping
  \param x_mapping the mapping for use for the x coordinate
  \param y_mapping the mapping for use for the y coordinate
  \param x a pointer to the x coordinate
  \param y a pointer to the y coordinate
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_tracker_remap(const kate_tracker *kin,kate_motion_mapping x_mapping,kate_motion_mapping y_mapping,kate_float *x,kate_float *y)
{
  if (!kin || !x || !y) return KATE_E_INVALID_PARAMETER;

  switch (x_mapping) {
    case kate_motion_mapping_none:
      /* nothing to do */
      break;
    case kate_motion_mapping_frame:
      *x=kin->frame_x+(*x)*kin->frame_w;
      break;
    case kate_motion_mapping_window:
      *x=(*x)*kin->window_w;
      break;
    case kate_motion_mapping_region:
      *x=kin->region_x+(*x)*kin->region_w;
      break;
    case kate_motion_mapping_event_duration:
      *x=(*x)*(kin->event->end_time-kin->event->start_time);
      break;
    case kate_motion_mapping_bitmap_size:
      if (!kin->event->bitmap) return KATE_E_INVALID_PARAMETER;
      *x=(*x)*kin->event->bitmap->width;
      break;
    default:
      /* unknown mapping */
      return KATE_E_INVALID_PARAMETER;
  }

  switch (y_mapping) {
    case kate_motion_mapping_none:
      /* nothing to do */
      break;
    case kate_motion_mapping_frame:
      *y=kin->frame_y+(*y)*kin->frame_h;
      break;
    case kate_motion_mapping_window:
      *y=(*y)*kin->window_h;
      break;
    case kate_motion_mapping_region:
      *y=kin->region_y+(*y)*kin->region_h;
      break;
    case kate_motion_mapping_event_duration:
      *y=(*y)*(kin->event->end_time-kin->event->start_time);
      break;
    case kate_motion_mapping_bitmap_size:
      if (!kin->event->bitmap) return KATE_E_INVALID_PARAMETER;
      *y=(*y)*kin->event->bitmap->height;
      break;
    default:
      /* unknown mapping */
      return KATE_E_INVALID_PARAMETER;
  }

  return 0;
}

static const kate_motion *kate_tracker_find_motion(const kate_tracker *kin,kate_motion_semantics semantics)
{
  const kate_event *ev;
  const kate_motion *km;
  size_t n;

  if (!kin || !kin->event) return NULL;

  ev=kin->event;
  for (n=0;n<ev->nmotions;++n) {
    km=ev->motions[n];
    if (km->semantics==semantics) return km;
  }

  return NULL;
}

/**
  \ingroup tracker
  Returns the value of a particular property at the given time
  \param kin the tracker to update the property for
  \param duration the duration of the event the motion belongs to
  \param t the time, between 0 and duration, at which to compute the point
  \param semantics the semantics of the motion to use
  \param x a pointer to the x coordinate
  \param y a pointer to the y coordinate
  \returns 0 success
  \returns 1 success, and there is no such motion at this time
  \returns KATE_E_* error
  */
int kate_tracker_update_property_at_duration(const kate_tracker *kin,kate_float duration,kate_float t,kate_motion_semantics semantics,kate_float *x,kate_float *y)
{
  const kate_motion *km;
  int ret;

  if (!kin || !x || !y) return KATE_E_INVALID_PARAMETER;

  km=kate_tracker_find_motion(kin,semantics);
  if (km) {
    ret=kate_motion_get_point(km,duration,t,x,y);
    if (ret<0) return ret;
    if (ret==0) {
      ret=kate_tracker_remap(kin,km->x_mapping,km->y_mapping,x,y);
      if (ret<0) return ret;
      return 0;
    }
    return 1; /* positive means no error, but no curve at that point */
  }
  return 1; /* positive means no error, but nothing found either */
}

static int kate_tracker_update_property_at(const kate_tracker *kin,kate_float t,kate_motion_semantics semantics,kate_float *x,kate_float *y)
{
#if 1
  kate_float duration=kin->event->end_time-kin->event->start_time;
  return kate_tracker_update_property_at_duration(kin,duration,t,semantics,x,y);
#else
  const kate_motion *km;
  int ret;

  if (!kin || !x || !y) return KATE_E_INVALID_PARAMETER;

  km=kate_tracker_find_motion(kin,semantics);
  if (km) {
    /* TODO: could be precalculated */
    kate_float duration=kin->event->end_time-kin->event->start_time;
    ret=kate_motion_get_point(km,duration,t,x,y);
    if (ret<0) return ret;
    if (ret==0) {
      ret=kate_tracker_remap(kin,km->x_mapping,km->y_mapping,x,y);
      if (ret<0) return ret;
      return 0;
    }
    return 1; /* positive means no error, but no curve at that point */
  }
  return 1; /* positive means no error, but nothing found either */
#endif
}

static int kate_tracker_update_property(const kate_tracker *kin,kate_motion_semantics semantics,kate_float *px,kate_float *py)
{
  if (!kin) return KATE_E_INVALID_PARAMETER;
  return kate_tracker_update_property_at(kin,kin->t,semantics,px,py);
}

static unsigned char kate_float_to_color_component(kate_float c)
{
  int v=(int)(c+(kate_float)0.5);
  if (v<0) v=0;
  if (v>255) v=255;
  return v;
}

static const kate_bitmap *kate_tracker_find_bitmap(const kate_tracker *kin,kate_float frame)
{
  size_t idx=(size_t)(frame+(kate_float)0.5);
  if (kin->event->nbitmaps>0) {
    if (idx<kin->event->nbitmaps) {
      return kin->event->bitmaps[idx];
    }
  }
  else {
    if (idx<kin->ki->nbitmaps) {
      return kin->ki->bitmaps[idx];
    }
  }
  return NULL;
}

/**
  \ingroup tracker
  Tracks changes in an event at the given time.
  \param kin the tracker to update
  \param t the time (between 0 and the duration of the motion) to update to
  \param window_w the width of the window
  \param window_h the height of the window
  \param frame_x the horizontal offset of the video frame within the window
  \param frame_y the vertical offset of the video frame within the window
  \param frame_w the width of the video frame
  \param frame_h the height of the video frame
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_tracker_update(kate_tracker *kin,kate_float t,int window_w,int window_h,int frame_x,int frame_y,int frame_w,int frame_h)
{
  const kate_event *ev;
  const kate_region *kr;
  const kate_style *ks;
  const kate_motion *km;
  size_t n;
  int ret;
  kate_float r,g,b,a;
  kate_float dummy;
  kate_float frame;

  if (!kin) return KATE_E_INVALID_PARAMETER;
  if (!kin->event) return KATE_E_INIT;
  if (t<0) return KATE_E_INVALID_PARAMETER;
  if (frame_w<0 || frame_h<0) return KATE_E_INVALID_PARAMETER;

  ev=kin->event;
  kin->t=t;
  memset(&kin->has,0,sizeof(kin->has));

  kin->window_w=window_w;
  kin->window_h=window_h;
  kin->frame_x=frame_x;
  kin->frame_y=frame_y;
  kin->frame_w=frame_w;
  kin->frame_h=frame_h;

  /* find out which region we're in, if any */
  kr=ev->region;

  /* find out which style we have, if any */
  ks=ev->style;
  if (!ks && kr && kr->style>=0) ks=kin->ki->styles[kr->style]; /* regions can have a default style */

  /* start off with the event attributes */
  kin->region_x=kin->region_y=kin->region_w=kin->region_h=0;
  if (kr) {
    switch (kr->metric) {
      case kate_percentage:
        kin->region_x=kr->x*frame_w/(kate_float)100;
        kin->region_y=kr->y*frame_h/(kate_float)100;
        kin->region_w=kr->w*frame_w/(kate_float)100;
        kin->region_h=kr->h*frame_h/(kate_float)100;
        break;
      case kate_millionths: /* use 64 bit arithmetic to avoid overflow */
        kin->region_x=kr->x*(kate_int64_t)frame_w/(kate_float)1000000;
        kin->region_y=kr->y*(kate_int64_t)frame_h/(kate_float)1000000;
        kin->region_w=kr->w*(kate_int64_t)frame_w/(kate_float)1000000;
        kin->region_h=kr->h*(kate_int64_t)frame_h/(kate_float)1000000;
        break;
      case kate_pixel:
        kin->region_x=kr->x;
        kin->region_y=kr->y;
        kin->region_w=kr->w;
        kin->region_h=kr->h;
        break;
      default:
        return KATE_E_INVALID_PARAMETER;
        break;
    }
    /* we already have a valid region */
    kin->has.region=1;
  }

  if (ks) {
    kin->text_color=ks->text_color;
    kin->background_color=ks->background_color;
    kin->draw_color=ks->draw_color;
    /* we already have valid text/background/draw colors */
    kin->has.text_color=1;
    kin->has.background_color=1;
    kin->has.draw_color=1;

    kin->text_halign=ks->halign;
    kin->text_valign=ks->valign;
    /* we already have internal alignment */
    kin->has.text_alignment_int=1;
  }

  /* if we have no motions, go no further */
  if (!kin->event->nmotions) return 0;

  /* TODO: think about the consequences of this wrt continuations */

  /* alter the flow of time */
  ret=kate_tracker_update_property(kin,kate_motion_semantics_time,&t,&dummy);
  if (ret==0) {
    /* t should be left within 0-duration */
    /* clamp, just in case */
    if (t<0) t=0;

    /* jump in time for the remainder of the lookups */
    kin->t=t;
  }

  /* update Z */
  ret=kate_tracker_update_property(kin,kate_motion_semantics_z,&kin->z,&dummy);
  if (ret==0) kin->has.z=1;

  /* update region position */
  if (kr) {
    ret=kate_tracker_update_property(kin,kate_motion_semantics_region_position,&kin->region_x,&kin->region_y);
    if (ret==0) kin->has.region=1;
  }

  /* update region size */
  if (kr) {
    ret=kate_tracker_update_property(kin,kate_motion_semantics_region_size,&kin->region_w,&kin->region_h);
    if (ret==0) kin->has.region=1;
  }

  /* update text visible section */
  if (kr) {
    ret=kate_tracker_update_property(kin,kate_motion_semantics_text_visible_section,&kin->visible_x,&kin->visible_y);
    if (ret==0) kin->has.visible_section=1;
  }

  /* update text path section */
  km=kate_tracker_find_motion(kin,kate_motion_semantics_text_path);
  if (km) {
    kin->has.path=1;
    kin->path_start=0;
    kin->path_end=1;
    ret=kate_tracker_update_property(kin,kate_motion_semantics_text_path_section,&kin->path_start,&kin->path_end);
  }

  /* update style morphing */
  km=kate_tracker_find_motion(kin,kate_motion_semantics_style_morph);
  if (km) {
    kate_float morphing;
    kate_float duration=kin->event->end_time-kin->event->start_time;
    ret=kate_motion_get_point(km,duration,kin->t,&morphing,&dummy);
    if (ret==0) {
      kate_style style;
      ret=kate_tracker_morph_styles(&style,morphing,ev->style,ev->secondary_style);
      if (ret==0) {
        kin->text_halign=style.halign;
        kin->text_valign=style.valign;
        kin->text_color=style.text_color;
        kin->background_color=style.background_color;
        kin->draw_color=style.draw_color;
        kin->left_margin=style.left_margin;
        kin->top_margin=style.top_margin;
        kin->right_margin=style.right_margin;
        kin->bottom_margin=style.bottom_margin;
        /* we get everything that's in a style here */
        /* those can be overridden afterwards by an alignment/color/etc motion */
        kin->has.text_alignment_int=1;
        kin->has.text_color=1;
        kin->has.background_color=1;
        kin->has.draw_color=1;
        kin->has.hmargins=1;
        kin->has.vmargins=1;
      }
    }
  }

  /* update margins */
  ret=kate_tracker_update_property(kin,kate_motion_semantics_horizontal_margins,&kin->left_margin,&kin->right_margin);
  if (ret==0) kin->has.hmargins=1;
  ret=kate_tracker_update_property(kin,kate_motion_semantics_vertical_margins,&kin->top_margin,&kin->bottom_margin);
  if (ret==0) kin->has.vmargins=1;

  /* update text alignment */
  ret=kate_tracker_update_property(kin,kate_motion_semantics_text_alignment_int,&kin->text_halign,&kin->text_valign);
  if (ret==0) kin->has.text_alignment_int=1;
  ret=kate_tracker_update_property(kin,kate_motion_semantics_text_alignment_ext,&kin->text_halign,&kin->text_valign);
  if (ret==0) {
    kin->has.text_alignment_int=0; /* external alignment override internal (the default) */
    kin->has.text_alignment_ext=1;
  }

  /* update bitmap position */
  ret=kate_tracker_update_property(kin,kate_motion_semantics_bitmap_position,&kin->bitmap_x,&kin->bitmap_y);
  if (ret==0) kin->has.bitmap_pos=1;

  /* update bitmap size */
  ret=kate_tracker_update_property(kin,kate_motion_semantics_bitmap_size,&kin->bitmap_size_x,&kin->bitmap_size_y);
  if (ret==0) kin->has.bitmap_size=1;

  /* update text position */
  ret=kate_tracker_update_property(kin,kate_motion_semantics_text_position,&kin->text_x,&kin->text_y);
  if (ret==0) kin->has.text_pos=1;

  /* update text size */
  ret=kate_tracker_update_property(kin,kate_motion_semantics_text_size,&kin->text_size_x,&kin->text_size_y);
  if (ret==0) kin->has.text_size=1;

  /* update glyph pointers */
  for (n=0;n<4;++n) {
    kate_motion_semantics semantics=(kate_motion_semantics)(kate_motion_semantics_glyph_pointer_1+n);
    ret=kate_tracker_update_property(kin,semantics,&kin->glyph_pointer[n],&kin->glyph_height[n]);
    if (ret==0) {
      kin->has.glyph_pointer|=(1<<n);
      /* we don't have to bother looking for a bitmap frame if there's no glyph pointer */
      semantics=(kate_motion_semantics)(kate_motion_semantics_glyph_pointer_1_bitmap+n);
      ret=kate_tracker_update_property(kin,semantics,&frame,&dummy);
      if (ret==0) {
        const kate_bitmap *kb=kate_tracker_find_bitmap(kin,frame);
        if (kb) {
          kin->glyph_pointer_bitmap[n]=kb;
          kin->has.glyph_pointer_bitmap|=(1<<n);
        }
      }
    }
  }

  /* update marker position */
  for (n=0;n<4;++n) {
    kate_motion_semantics semantics=(kate_motion_semantics)(kate_motion_semantics_marker1_position+n);
    ret=kate_tracker_update_property(kin,semantics,&kin->marker_x[n],&kin->marker_y[n]);
    if (ret==0) {
      kin->has.marker_pos|=(1<<n);
      /* we don't have to bother looking for a bitmap frame if there's no marker position */
      semantics=(kate_motion_semantics)(kate_motion_semantics_marker1_bitmap+n);
      ret=kate_tracker_update_property(kin,semantics,&frame,&dummy);
      if (ret==0) {
        const kate_bitmap *kb=kate_tracker_find_bitmap(kin,frame);
        if (kb) {
          kin->marker_bitmap[n]=kb;
          kin->has.marker_bitmap|=(1<<n);
        }
      }
    }
  }

  /* update draw position */
  ret=kate_tracker_update_property(kin,kate_motion_semantics_draw,&kin->draw_x,&kin->draw_y);
  if (ret==0) kin->has.draw=1;

  /* update colors */
  ret=kate_tracker_update_property(kin,kate_motion_semantics_text_color_rg,&r,&g);
  if (ret==0) {
    kin->text_color.r=kate_float_to_color_component(r);
    kin->text_color.g=kate_float_to_color_component(g);
    kin->has.text_color=1;
  }
  ret=kate_tracker_update_property(kin,kate_motion_semantics_text_color_ba,&b,&a);
  if (ret==0) {
    kin->text_color.b=kate_float_to_color_component(b);
    kin->text_color.a=kate_float_to_color_component(a);
    kin->has.text_color=1;
  }

  ret=kate_tracker_update_property(kin,kate_motion_semantics_background_color_rg,&r,&g);
  if (ret==0) {
    kin->background_color.r=kate_float_to_color_component(r);
    kin->background_color.g=kate_float_to_color_component(g);
    kin->has.background_color=1;
  }
  ret=kate_tracker_update_property(kin,kate_motion_semantics_background_color_ba,&b,&a);
  if (ret==0) {
    kin->background_color.b=kate_float_to_color_component(b);
    kin->background_color.a=kate_float_to_color_component(a);
    kin->has.background_color=1;
  }

  ret=kate_tracker_update_property(kin,kate_motion_semantics_draw_color_rg,&r,&g);
  if (ret==0) {
    kin->draw_color.r=kate_float_to_color_component(r);
    kin->draw_color.g=kate_float_to_color_component(g);
    kin->has.draw_color=1;
  }
  ret=kate_tracker_update_property(kin,kate_motion_semantics_draw_color_ba,&b,&a);
  if (ret==0) {
    kin->draw_color.b=kate_float_to_color_component(b);
    kin->draw_color.a=kate_float_to_color_component(a);
    kin->has.draw_color=1;
  }

  /* update draw width */
  ret=kate_tracker_update_property(kin,kate_motion_semantics_draw_width,&kin->draw_width,&dummy);
  if (ret==0) kin->has.draw_width=1;

  return 0;
}

/**
  \ingroup tracker
  Finds the position of a particular glyph along a path.
  \param kin the tracker to use
  \param glyph the index of the glyph to get the position of
  \param x will return the first coordinate of the position
  \param y will return the second coordinate of the position
  */
int kate_tracker_get_text_path_position(kate_tracker *kin,size_t glyph,int *x,int *y)
{
  kate_float t;
  kate_float dx,dy;
  int ret;

  if (!kin || !x || !y) return KATE_E_INVALID_PARAMETER;
  if (glyph>=kin->internal->nglyphs) return KATE_E_INVALID_PARAMETER;
  if (!kin->has.path) return KATE_E_INVALID_PARAMETER;

  t=0;
  if (kin->internal->nglyphs>1) {
    t=kin->path_start+glyph*(kin->path_end-kin->path_start)/(kin->internal->nglyphs-1);
  }
  ret=kate_tracker_update_property_at_duration(kin,1,t,kate_motion_semantics_text_path,&dx,&dy);
  if (ret==0) {
    *x=(int)(dx+(kate_float)0.5);
    *y=(int)(dy+(kate_float)0.5);
  }

  return ret;
}

