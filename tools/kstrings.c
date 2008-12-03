/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <errno.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <ogg/ogg.h>
#include "kate/oggkate.h"
#include "kate_internal.h"
#include "kutil.h"

const char *halign2text(kate_float d)
{
  static char tmp[32];
  if (d==0.0) return "hcenter";
  if (d==1.0) return "hright";
  if (d==-1.0) return "hleft";
  snprintf(tmp,sizeof(tmp),"halign %.16g",d);
  tmp[sizeof(tmp)-1]=0;
  return tmp;
}

const char *valign2text(kate_float d)
{
  static char tmp[32];
  if (d==0.0) return "vcenter";
  if (d==-1.0) return "vtop";
  if (d==1.0) return "vbottom";
  snprintf(tmp,sizeof(tmp),"valign %.16g",d);
  tmp[sizeof(tmp)-1]=0;
  return tmp;
}

const char *metric2text(kate_space_metric d)
{
  switch (d) {
    case kate_pixel: return "pixel";
    case kate_percentage: return "percent";
    case kate_millionths: return "millionths";
    default: return "invalid";
  }
}

const char *metric2suffix(kate_space_metric d)
{
  switch (d) {
    case kate_pixel: return "";
    case kate_percentage: return "%";
    case kate_millionths: return "m";
    default: return "invalid";
  }
}

const char *curve2text(kate_curve_type d)
{
  switch (d) {
    case kate_curve_none: return "none";
    case kate_curve_static: return "static";
    case kate_curve_linear: return "linear";
    case kate_curve_catmull_rom_spline: return "catmull_rom";
    case kate_curve_bezier_cubic_spline: return "bezier_cubic";
    case kate_curve_bspline: return "bspline";
  }
  return "invalid";
}

const char *semantics2text(kate_motion_semantics d)
{
  static char tmp[32];
  switch (d) {
    case kate_motion_semantics_time: return "time";
    case kate_motion_semantics_region_position: return "region position";
    case kate_motion_semantics_region_size: return "region size";
    case kate_motion_semantics_text_alignment_ext: return "external text alignment";
    case kate_motion_semantics_text_alignment_int: return "internal text alignment";
    case kate_motion_semantics_text_position: return "text position";
    case kate_motion_semantics_text_size: return "text size";
    case kate_motion_semantics_marker1_position: return "marker 1 position";
    case kate_motion_semantics_marker2_position: return "marker 2 position";
    case kate_motion_semantics_marker3_position: return "marker 3 position";
    case kate_motion_semantics_marker4_position: return "marker 4 position";
    case kate_motion_semantics_glyph_pointer_1: return "glyph pointer 1";
    case kate_motion_semantics_glyph_pointer_2: return "glyph pointer 2";
    case kate_motion_semantics_glyph_pointer_3: return "glyph pointer 3";
    case kate_motion_semantics_glyph_pointer_4: return "glyph pointer 4";
    case kate_motion_semantics_text_color_rg: return "text color rg";
    case kate_motion_semantics_text_color_ba: return "text color ba";
    case kate_motion_semantics_background_color_rg: return "background color rg";
    case kate_motion_semantics_background_color_ba: return "background color ba";
    case kate_motion_semantics_draw_color_rg: return "draw color rg";
    case kate_motion_semantics_draw_color_ba: return "draw color ba";
    case kate_motion_semantics_style_morph: return "style morph";
    case kate_motion_semantics_text_path: return "text path";
    case kate_motion_semantics_text_path_section: return "text path section";
    case kate_motion_semantics_draw: return "draw";
    case kate_motion_semantics_text_visible_section: return "visible section";
    case kate_motion_semantics_z: return "z";
    case kate_motion_semantics_horizontal_margins: return "horizontal margins";
    case kate_motion_semantics_vertical_margins: return "vertical margins";
    case kate_motion_semantics_bitmap_position: return "bitmap position";
    case kate_motion_semantics_bitmap_size: return "bitmap size";
    case kate_motion_semantics_marker1_bitmap: return "marker 1 bitmap";
    case kate_motion_semantics_marker2_bitmap: return "marker 2 bitmap";
    case kate_motion_semantics_marker3_bitmap: return "marker 3 bitmap";
    case kate_motion_semantics_marker4_bitmap: return "marker 4 bitmap";
    case kate_motion_semantics_glyph_pointer_1_bitmap: return "glyph pointer 1 bitmap";
    case kate_motion_semantics_glyph_pointer_2_bitmap: return "glyph pointer 2 bitmap";
    case kate_motion_semantics_glyph_pointer_3_bitmap: return "glyph pointer 3 bitmap";
    case kate_motion_semantics_glyph_pointer_4_bitmap: return "glyph pointer 4 bitmap";
    case kate_motion_semantics_draw_width: return "draw width";
    default: snprintf(tmp,sizeof(tmp),"user %u",d); return tmp;
  }
  return "invalid";
}

const char *mapping2text(kate_motion_mapping d)
{
  static char tmp[32];
  switch (d) {
    case kate_motion_mapping_none: return "none";
    case kate_motion_mapping_frame: return "frame";
    case kate_motion_mapping_region: return "region";
    case kate_motion_mapping_event_duration: return "event duration";
    case kate_motion_mapping_bitmap_size: return "bitmap size";
    default: snprintf(tmp,sizeof(tmp),"user %u",d); return tmp;
  }
  return "invalid";
}

const char *directionality2text(kate_text_directionality d)
{
  switch (d) {
    case kate_l2r_t2b: return "l2r_t2b";
    case kate_r2l_t2b: return "r2l_t2b";
    case kate_t2b_r2l: return "t2b_r2l";
    case kate_t2b_l2r: return "t2b_l2r";
  }
  return "invalid";
}

const char *wrap2text(kate_wrap_mode w)
{
  switch (w) {
    case kate_wrap_word: return "word";
    case kate_wrap_none: return "none";
  }
  return "invalid";
}

const char *encoding2text(kate_text_encoding text_encoding)
{
  switch (text_encoding) {
    case kate_utf8: return "UTF-8";
    default: return "unknown";
  }
}

