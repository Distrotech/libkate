/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#ifndef _KATE_INTERNAL_H_
#define _KATE_INTERNAL_H_

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

#define kate_internal __attribute__((visibility("internal")))

/* granule operations */
extern kate_int64_t kate_time_granule(const kate_info *ki,kate_float base,kate_float offset) kate_internal;
extern int kate_granule_split_time(const kate_info *ki,kate_int64_t granulepos,kate_float *base,kate_float *offset);

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

extern int kate_is_valid_code_point(int c) kate_internal __attribute__((const));

extern int kate_font_check_ranges(const kate_font_mapping *kfm) kate_internal;

#endif

