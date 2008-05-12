/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#ifndef _KATE_CODEC_H_
#define _KATE_CODEC_H_

/** \file kate.h
  The libkate public API.
  */

#include <stddef.h>
#include <stdint.h>
#include <ogg/ogg.h>

#ifdef KATE_INTERNAL
#define kate_const
#else
#define kate_const const
#endif

typedef int32_t kate_int32_t;
typedef int64_t kate_int64_t;
typedef float kate_float;

/** \name API version */
/** @{ */
#define KATE_VERSION_MAJOR 0             /**< major version number of the libkate API */
#define KATE_VERSION_MINOR 1             /**< minor version number of the libkate API */
#define KATE_VERSION_PATCH 2             /**< patch version number of the libkate API */
/** @} */

/** \name Bitstream version */
/** @{ */
#define KATE_BITSTREAM_VERSION_MAJOR 0   /**< major version number of the highest bitstream version this version of libkate supports */
#define KATE_BITSTREAM_VERSION_MINOR 2   /**< minor version number of the highest bitstream version this version of libkate supports */
/** @} */

#ifndef kate_malloc
#define kate_malloc malloc
#endif
#ifndef kate_realloc
#define kate_realloc realloc
#endif
#ifndef kate_free
#define kate_free free
#endif

/** defines the character encoding used by text */
typedef enum {
  kate_utf8                      /**< utf-8 variable length byte encoding, see RFC 3629 */
} kate_text_encoding;

/** defines how to interpret spatial dimension values */
typedef enum {
  kate_pixel,                    /**< dimensions are in pixels */
  kate_percentage                /**< dimensions are in percentage of total size */
} kate_space_metric;

/** defines an area where to draw */
typedef struct kate_region {
  kate_space_metric metric;      /**< how to interpret the xywh values */
  int x;                         /**< origin of the region */
  int y;                         /**< origin of the region */
  int w;                         /**< size of the region */
  int h;                         /**< size of the region */
  int style;                     /**< default style to apply to text in this region */
  uint32_t clip:1;               /**< text/images will be clipped to the region */
  uint32_t pad0:31;
  uintptr_t pad1[6];
} kate_region;

/** defines an RGBA color */
typedef struct kate_color {
  unsigned char r;               /**< red component, 0 (black) to 255 (full intensity) */
  unsigned char g;               /**< green component, 0 (black) to 255 (full intensity) */
  unsigned char b;               /**< blue component, 0 (black) to 255 (full intensity) */
  unsigned char a;               /**< alpha component, 0 (fully transparent) to 255 (fully opaque) */
} kate_color;

/** defines a style to display text */
typedef struct kate_style {
  kate_float halign;                 /**< horizontal alignment, -1 for left, +1 for right, other values inter/extrapolate */
  kate_float valign;                 /**< horizontal alignment, -1 for top, +1 for bottom, other values inter/extrapolate */

  kate_color text_color;             /**< RGBA color of the text */
  kate_color background_color;       /**< background RGBA color of the whole region, regardless of what extent any text has */
  kate_color draw_color;             /**< RGBA color for drawn shapes */

  kate_space_metric font_metric;     /**< whether font size are in pixels or percentage */
  kate_float font_width;             /**< horizontal size of the glyphs */
  kate_float font_height;            /**< vertical size of the glyphs */

  kate_space_metric margin_metric;   /**< how to interpret margin values */
  kate_float left_margin;            /**< size of left margin */
  kate_float top_margin;             /**< size of top margin */
  kate_float right_margin;           /**< size of right margin */
  kate_float bottom_margin;          /**< size of bottom margin */

  uint32_t bold:1;                   /**< display text in bold */
  uint32_t italics:1;                /**< display text in italics */
  uint32_t underline:1;              /**< display underlined text */
  uint32_t strike:1;                 /**< display striked text */
  uint32_t justify:1;                /**< display justified text */
  uint32_t pad0:27;

  kate_const char *font;             /**< name of the font to use */

  uintptr_t pad1[9];
} kate_style;

/** defines a type of curve */
typedef enum kate_curve_type {
  kate_curve_none,                   /**< no curve */
  kate_curve_static,                 /**< a single point */
  kate_curve_linear,                 /**< linear interpolation of line segments */
  kate_curve_catmull_rom_spline,     /**< Catmull-Rom spline, goes through each point (even the first and last) */
  kate_curve_bezier_cubic_spline,    /**< Bezier cubic spline, goes through the first and last points, but not others */
  kate_curve_bspline,                /**< Cubic uniform B-spline with 3-multiplicity end knots (goes through each point) */
} kate_curve_type;

/** defines a curve */
typedef struct kate_curve {
  kate_curve_type type;              /**< type of the curve */
  size_t npts;                       /**< number of points in this curve */
  kate_float *pts;                   /**< point coordinates for this curve (2D: npts*2 values) */
  uintptr_t pad[5];
} kate_curve;

/** defines a way to transform a curve point */
typedef enum kate_motion_mapping {
  /* these can be used for a x/y position mapping */
  kate_motion_mapping_none,                  /**< motion maps to itself */
  kate_motion_mapping_frame,                 /**< 0x0 at top left of frame, 1x1 at bottom right of frame */
  kate_motion_mapping_window,                /**< 0x0 at top left of window, 1x1 at bottom right of window */
  kate_motion_mapping_region,                /**< 0x0 at top left of region, 1x1 at bottom right of region */
  kate_motion_mapping_event_duration,        /**< 0-1 map to 0 to the duration of the event (to be used with time) */
  kate_motion_mapping_bitmap_size,           /**< 0x0 at top left of bitmap, 1x1 at bottom right of bitmap */

#if 0
  text is useful, find a way to readd it easily
  kate_motion_mapping_text,                  /**< 0x0 at top left of text, 1x1 at bottom right of text */
#endif

  /* more mapping may be added in future versions */

  kate_motion_mapping_user=128               /**< 128 to 255 for user specific mappings */
} kate_motion_mapping;

/** defines what uses a motion can have */
typedef enum kate_motion_semantics {
  kate_motion_semantics_time,                    /**< controls the flow of time - 1D */
  kate_motion_semantics_z,                       /**< controls the "depth" of the plane - 1D */
  kate_motion_semantics_region_position,         /**< controls the region position */
  kate_motion_semantics_region_size,             /**< controls the region size */
  kate_motion_semantics_text_alignment_int,      /**< controls internal text alignment */
  kate_motion_semantics_text_alignment_ext,      /**< controls external text alignment */
  kate_motion_semantics_text_position,           /**< controls the text position */
  kate_motion_semantics_text_size,               /**< controls the text size */
  kate_motion_semantics_marker1_position,        /**< controls the position of a point */
  kate_motion_semantics_marker2_position,        /**< controls the position of a point */
  kate_motion_semantics_marker3_position,        /**< controls the position of a point */
  kate_motion_semantics_marker4_position,        /**< controls the position of a point */
  kate_motion_semantics_glyph_pointer_1,         /**< controls a pointer to a particular glyph in the text */
  kate_motion_semantics_glyph_pointer_2,         /**< controls a pointer to a particular glyph in the text */
  kate_motion_semantics_glyph_pointer_3,         /**< controls a pointer to a particular glyph in the text */
  kate_motion_semantics_glyph_pointer_4,         /**< controls a pointer to a particular glyph in the text */
  kate_motion_semantics_text_color_rg,           /**< controls the red and green components of the text color */
  kate_motion_semantics_text_color_ba,           /**< controls the blue and alpha components of the text color */
  kate_motion_semantics_background_color_rg,     /**< controls the red and green components of the background color */
  kate_motion_semantics_background_color_ba,     /**< controls the blue and alpha components of the background color */
  kate_motion_semantics_draw_color_rg,           /**< controls the red and green components of the draw color */
  kate_motion_semantics_draw_color_ba,           /**< controls the blue and alpha components of the draw color */
  kate_motion_semantics_style_morph,             /**< controls morphing between style and secondary style */
  kate_motion_semantics_text_path,               /**< controls the path on which text is drawn */
  kate_motion_semantics_text_path_section,       /**< controls the section of the path on which text is drawn */
  kate_motion_semantics_draw,                    /**< controls drawing */
  kate_motion_semantics_text_visible_section,    /**< controls the section of the text which is visible */
  kate_motion_semantics_horizontal_margins,      /**< controls the size of the left and right margins */
  kate_motion_semantics_vertical_margins,        /**< controls the size of the top and bottom margins */
  kate_motion_semantics_bitmap_position,         /**< controls the position of the background image */
  kate_motion_semantics_bitmap_size,             /**< controls the size of the background image */

  /* more semantics may be added in future versions */

  kate_motion_semantics_user = 128               /**< 128 to 255 for user specific semantics */
} kate_motion_semantics;

/** defines a motion - well, try to find a better explanation */
typedef struct kate_motion {
  size_t ncurves;                                /**< number of curves in this motion */
  kate_curve **curves;                           /**< the list of curves in this motion */
  kate_float *durations;                         /**< the durations of each curve */
  kate_motion_mapping x_mapping;                 /**< how to remap the x coordinate of the points */
  kate_motion_mapping y_mapping;                 /**< how to remap the y coordinate of the points */
  kate_motion_semantics semantics;               /**< what use this motion has */
  uint32_t periodic:1;                           /**< if true, repeats periodically (loops) */
  uint32_t pad0:31;
  uintptr_t pad1[5];
} kate_motion;

/** defines the direction in which glyphs within a text are drawn */
typedef enum kate_text_directionality {
  kate_l2r_t2b,                                  /**< left to right, top to bottom: eg, English */
  kate_r2l_t2b,                                  /**< right to left, top to bottom: eg, Arabic */
  kate_t2b_r2l,                                  /**< top to bottom, right to left: eg, Japanese */
  kate_t2b_l2r                                   /**< top to bottom, left to right: eg, Sometimes Japanese */
} kate_text_directionality;

/** defines colors to correspond to a bitmap's pixels */
typedef struct kate_palette {
  size_t ncolors;                                /**< number of colors in this palette */
  kate_color *colors;                            /**< the list of colors in this palette */
  uintptr_t pad[2];
} kate_palette;

/** defines a particular type of bitmap */
typedef enum kate_bitmap_type {
  kate_bitmap_type_paletted,                     /**< paletted bitmap */
  kate_bitmap_type_png                           /**< a PNG bitmap */
} kate_bitmap_type;

/** defines a paletted image */
typedef struct kate_bitmap {
  size_t width;                                  /**< width in pixels */
  size_t height;                                 /**< height in pixels */
  unsigned char bpp;                             /**< bits per pixel, from 1 to 8, or 0 for a raw PNG bitmap */
  kate_bitmap_type type;                         /**< the type of this bitmap */
  unsigned char pad0[2];
  int palette;                                   /**< index of the default palette to use */
  unsigned char *pixels;                         /**< pixels, rows first, one byte per pixel regardless of bpp */
  size_t size;                                   /**< for raw bitmaps, number of bytes in pixels */
  uintptr_t pad1[2];
} kate_bitmap;

/** defines a set of images to map to a range of Unicode code points */
typedef struct kate_font_range {
  int first_code_point;                          /**< first code point in this range */
  int last_code_point;                           /**< last code point in this range */
  int first_bitmap;                              /**< index of the bitmap the first code point maps to */
  uintptr_t pad[5];
} kate_font_range;

/** defines a set of ranges to define a font mapping */
typedef struct kate_font_mapping {
  size_t nranges;                                /**< the number of ranges in this mapping */
  kate_font_range **ranges;                      /**< the list of ranges in this mapping */
  uintptr_t pad[6];
} kate_font_mapping;

/**
  Information about a Kate bitstream
  On encoding, this information will be filled by the encoder.
  On decoding, it will be extracted from the stream headers.
  */
typedef struct kate_info {
  unsigned char bitstream_version_major;         /**< the version of the bitstream being read or written */
  unsigned char bitstream_version_minor;         /**< the version of the bitstream being read or written */
  unsigned char pad0[2];

  kate_text_encoding text_encoding;              /**< the default text encoding (utf-8 only for now) */
  kate_text_directionality text_directionality;  /**< the default text directionality (left to right, etc) */

  unsigned char num_headers;                     /**< number of header packets in the bitstream */
  unsigned char granule_shift;                   /**< how many low granpos bits are used for the offset */
  unsigned char pad1[2];

  kate_int32_t gps_numerator;                    /**< granules per second numerator */
  kate_int32_t gps_denominator;                  /**< granules per second denominator */

  kate_const char *language;                     /**< based on RFC 3066, 15 character + terminating zero max */

  kate_const char *category;                     /**< freeform for now, 15 characters + terminating zero max */

  size_t nregions;                               /**< the number of predefined regions */
  kate_const kate_region *kate_const *regions;   /**< the list of predefined regions */

  size_t nstyles;                                /**< the number of predefined styles */
  kate_const kate_style *kate_const *styles;     /**< the list of predefined styles */

  size_t ncurves;                                /**< the number of predefined curves */
  kate_const kate_curve *kate_const *curves;     /**< the list of predefined curves */

  size_t nmotions;                               /**< the number of predefined motions */
  kate_const kate_motion *kate_const *motions;   /**< the list of predefined motions */

  size_t npalettes;                              /**< the number of predefined palettes */
  kate_const kate_palette *kate_const *palettes; /**< the list of predefined palettes */

  size_t nbitmaps;                               /**< the number of predefined bitmaps */
  kate_const kate_bitmap *kate_const *bitmaps;   /**< the list of predefined bitmaps */

  size_t nfont_ranges;                                       /**< the number of predefined font ranges */
  kate_const kate_font_range *kate_const *font_ranges;       /**< the list of predefined font ranges */

  size_t nfont_mappings;                                     /**< the number of predefined font mappings */
  kate_const kate_font_mapping *kate_const *font_mappings;   /**< the list of predefined font mappings */

  uintptr_t pad2[14];

  /* internal */
  int remove_markup;
  int no_limits;
  int probe;

  uintptr_t pad3[13];
} kate_info;

struct kate_encode_state; /* internal */
struct kate_decode_state; /* internal */

/** top level information about a Kate bitstream */
typedef struct kate_state {
  kate_const kate_info *ki;                      /**< the kate_info associated with this state */

  /* internal */
  kate_const struct kate_encode_state *kes;
  kate_const struct kate_decode_state *kds;

  uintptr_t pad[5];
} kate_state;

/** Vorbis comments - this is the same as Vorbis and Theora comments */
typedef struct kate_comment {
  char **user_comments;                         /**< the list of comments, in the form "tag=value" */
  int *comment_lengths;                         /**< the lengths of the comment strings in bytes */
  int comments;                                 /**< the number of comments in the list */
  char *vendor;                                 /**< vendor string, null terminated */
} kate_comment;

/**
  This is an event passed to the user.
  A kate_tracker may be used to track animation changes to this event.
  */
typedef struct kate_event {
  kate_int64_t start;                           /**< the time at offset rate at which this event starts */
  kate_int64_t duration;                        /**< the duration in granules */
  kate_int64_t backlink;                        /**< the relative offset in granules since the start of the earliest still active event (positive or zero) */

  kate_float start_time;                        /**< the time at which this event starts */
  kate_float end_time;                          /**< the time at which this event ends */

  kate_int32_t id;                              /**< unique id to identify this event */

  kate_text_encoding text_encoding;             /**< character encoding for the text in this event */
  kate_text_directionality text_directionality; /**< directionality of the text in this event */
  kate_const char *language;                    /**< language of the text in this event (may be NULL if no override) */
  kate_const char *text;                        /**< the event text (may be NULL if none) */
  size_t len;                                   /**< length in bytes of the text */
  size_t len0;                                  /**< length in bytes of the text, including terminating zero(s) */

  size_t nmotions;                              /**< number of attached motions (may be zero) */
  kate_const kate_motion *kate_const *motions;  /**< the list of attached motions (may be NULL if none) */

  kate_const kate_region *region;               /**< region to display in (may be NULL for no particular region) */
  kate_const kate_style *style;                 /**< style to display text (may be NULL for no particular style) */
  kate_const kate_style *secondary_style;       /**< secondary style to display text (may be NULL for no particular style) */
  kate_const kate_font_mapping *font_mapping;   /**< font mapping to use for the text (may be NULL for no particular mapping) */
  kate_const kate_palette *palette;             /**< palette to use as background (may be NULL for none) */
  kate_const kate_bitmap *bitmap;               /**< bitmap to use as background (may be NULL for none) */

  uintptr_t pad0[9];

  /* internal */
  const kate_info *ki;
  size_t trackers;

  uintptr_t pad1[10];
} kate_event;

/** this keeps track of changes during an event's lifetime */
typedef struct kate_tracker {
  const kate_info *ki;                          /**< the kate_info associated with this tracker */
  kate_const kate_event *event;                 /**< the event being tracked */
  kate_float t;                                 /**< the current time at which the tracker interpolates */

  struct {
    uint32_t region:1;                          /**< if set, the tracker has region information */
    uint32_t text_alignment_int:1;              /**< if set, the tracker has internal text alignment information */
    uint32_t text_alignment_ext:1;              /**< if set, the tracker has external text alignment information */
    uint32_t text_pos:1;                        /**< if set, the tracker has text position information */
    uint32_t text_size:1;                       /**< if set, the tracker has text size information */
    uint32_t marker_pos:4;                      /**< if set, the tracker has marker position information */
    uint32_t text_color:1;                      /**< if set, the tracker has text color information */
    uint32_t background_color:1;                /**< if set, the tracker has background color information */
    uint32_t draw_color:1;                      /**< if set, the tracker has draw color information */
    uint32_t glyph_pointer:4;                   /**< if set, the tracker has glyph pointer information */
    uint32_t path:1;                            /**< if set, the tracker has text path information */
    uint32_t draw:1;                            /**< if set, the tracker has draw information */
    uint32_t visible_section:1;                 /**< if set, the tracker has visible section information */
    uint32_t z:1;                               /**< if set, the tracker has z (depth) information */
    uint32_t hmargins:1;                        /**< if set, the tracker has horizontal margin information */
    uint32_t vmargins:1;                        /**< if set, the tracker has vertical margin information */
    uint32_t bitmap_pos:1;                      /**< if set, the tracker has bitmap position information */
    uint32_t bitmap_size:1;                     /**< if set, the tracker has bitmap size information */
    /* 24 bits */
    uint32_t pad0:8;
    uint32_t pad1:32;
    /* 64 bits */
  } has;                                        /**< bitfield describing what information the tracker has */

  int window_w;                                 /**< the window width */
  int window_h;                                 /**< the window height */
  int frame_x;                                  /**< the video frame origin in the window */
  int frame_y;                                  /**< the video frame origin in the window */
  int frame_w;                                  /**< the video frame width */
  int frame_h;                                  /**< the video frame height */

  /* has.region */
  kate_float region_x;                          /**< the region horizontal position in pixels */
  kate_float region_y;                          /**< the region vertical position in pixels */
  kate_float region_w;                          /**< the region width in pixels */
  kate_float region_h;                          /**< the region height in pixels */

  /* has.text_alignment (int/ext) */
  kate_float text_halign;                       /**< the horizontal text alignment (-1 for left, 1 for right, etc) */
  kate_float text_valign;                       /**< the vertical text alignment (-1 for top, 1 for bottom, etc) */

  /* has.text_pos */
  kate_float text_x;                            /**< the horizontal text position */
  kate_float text_y;                            /**< the vertical text position */

  /* has.text_size */
  kate_float text_size_x;                       /**< the horizontal text size (eg, width) */
  kate_float text_size_y;                       /**< the vertical text size (eg, height) */

  /* has.marker_pos&(1<<n) */
  kate_float marker_x[4];                       /**< the horizontal position of each marker */
  kate_float marker_y[4];                       /**< the vertical position of each marker */

  /* has.text_color */
  kate_color text_color;                        /**< the text color */

  /* has.background_color */
  kate_color background_color;                  /**< the background color */

  /* has.draw_color */
  kate_color draw_color;                        /**< the draw color */

  /* has.glyph_pointer&(1<<n) */
  kate_float glyph_pointer[4];                  /**< the glyph index of each glyph pointer */
  kate_float glyph_height[4];                   /**< the height associated with each glyph pointer in pixels */

  /* has.path */
  kate_float path_start;                        /**< the starting point of the current path part */
  kate_float path_end;                          /**< the end point of the current path part */

  /* has.draw */
  kate_float draw_x;                            /**< the current horizontal position of the drawn shape */
  kate_float draw_y;                            /**< the current vertical position of the drawn shape */

  /* has.visible_section */
  kate_float visible_x;                         /**< the glyph index of the first visible glyph */
  kate_float visible_y;                         /**< the glyph index of the last visible glyph */

  /* has.z */
  kate_float z;                                 /**< the depth of this event's text */

  /* has.hmargins */
  kate_float left_margin;                       /**< the size of the left margin */
  kate_float right_margin;                      /**< the size of the right margin */

  /* has.vmargins */
  kate_float top_margin;                        /**< the size of the top margin */
  kate_float bottom_margin;                     /**< the size of the bottom margin */

  /* has.bitmap_pos */
  kate_float bitmap_x;                          /**< the horizontal bitmap position */
  kate_float bitmap_y;                          /**< the vertical bitmap position */

  /* has.bitmap_size */
  kate_float bitmap_size_x;                     /**< the horizontal bitmap size (eg, width) */
  kate_float bitmap_size_y;                     /**< the vertical bitmap size (eg, height) */

  /* internal */
  size_t nglyphs;

  uintptr_t pad[28];

} kate_tracker;

/** a kate packet raw data */
typedef struct kate_packet {
  size_t nbytes;             /**< the number of bytes in this packet */
  void *data;                /**< pointer to the data in this packet */
} kate_packet;

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup version Version information */
extern int kate_get_version(void);
extern const char *kate_get_version_string(void);
extern int kate_get_bitstream_version(void);
extern const char *kate_get_bitstream_version_string(void);

/** \defgroup info kate_info access */
extern int kate_info_init(kate_info *ki);
extern int kate_info_set_granule_encoding(kate_info *ki,kate_float resolution,kate_float max_length,kate_float max_event_lifetime);
extern int kate_info_set_language(kate_info *ki,const char *language);
extern int kate_info_set_text_directionality(kate_info *ki,kate_text_directionality text_directionality);
extern int kate_info_set_category(kate_info *ki,const char *category);
extern int kate_info_add_region(kate_info *ki,kate_region *kr);
extern int kate_info_add_style(kate_info *ki,kate_style *ks);
extern int kate_info_add_curve(kate_info *ki,kate_curve *kc);
extern int kate_info_add_motion(kate_info *ki,kate_motion *km);
extern int kate_info_add_palette(kate_info *ki,kate_palette *kp);
extern int kate_info_add_bitmap(kate_info *ki,kate_bitmap *kb);
extern int kate_info_add_font_range(kate_info *ki,kate_font_range *kfr);
extern int kate_info_add_font_mapping(kate_info *ki,kate_font_mapping *kfm);
extern int kate_info_matches_language(const kate_info *ki,const char *language);
extern int kate_info_remove_markup(kate_info *ki,int flag);
extern int kate_info_no_limits(kate_info *ki,int flag);
extern int kate_info_clear(kate_info *ki);

/** \defgroup granule Granule calculations */
extern int kate_granule_shift(const kate_info *ki);
extern kate_float kate_granule_time(const kate_info *ki,kate_int64_t granulepos);
extern kate_int64_t kate_duration_granule(const kate_info *ki,kate_float duration);
extern kate_float kate_granule_duration(const kate_info *ki,kate_int64_t duration);

extern int kate_clear(kate_state *k);
extern int kate_motion_get_point(const kate_motion *km,kate_float duration,kate_float t,kate_float *x,kate_float *y);

/** \defgroup text Text manipulation */
extern int kate_text_get_character(kate_text_encoding text_encoding,const char ** const text,size_t *len0);
extern int kate_text_set_character(kate_text_encoding text_encoding,int c,char ** const text,size_t *len0);
extern int kate_text_remove_markup(kate_text_encoding text_encoding,char *text,size_t *len0);
extern int kate_text_validate(kate_text_encoding text_encoding,const char *text,size_t len0);

/** \defgroup comments kate_comment access */
extern int kate_comment_init(kate_comment *kc);
extern int kate_comment_clear(kate_comment *kc);
extern int kate_comment_add(kate_comment *kc,const char *comment);
extern int kate_comment_add_length(kate_comment *kc,const char *comment,size_t len);
extern int kate_comment_add_tag(kate_comment *kc,const char *name,const char *value);
extern const char *kate_comment_query(const kate_comment *kc,const char *tag,int count);
extern int kate_comment_query_count(const kate_comment *kc,const char *tag);

/** \defgroup encoding Encoding */
extern int kate_encode_init(kate_state *k,kate_info *ki);
extern int kate_encode_headers(kate_state *k,kate_comment *kc,kate_packet *kp);
extern int kate_encode_text(kate_state *k,kate_float start_time,kate_float stop_time,const char *text,size_t sz,kate_packet *kp); /* text is not null terminated */
extern int kate_encode_keepalive(kate_state *k,kate_float t,kate_packet *kp);
extern int kate_encode_finish(kate_state *k,kate_float t,kate_packet *kp); /* t may be negative to use the end granule of the last event */
extern int kate_encode_set_id(kate_state *k,kate_int32_t id);
extern int kate_encode_set_language(kate_state *k,const char *language); /* language can be NULL */
extern int kate_encode_set_text_encoding(kate_state *k,kate_text_encoding text_encoding);
extern int kate_encode_set_text_directionality(kate_state *k,kate_text_directionality text_directionality);
extern int kate_encode_set_region_index(kate_state *k,size_t region);
extern int kate_encode_set_region(kate_state *k,const kate_region *kr);
extern int kate_encode_set_style_index(kate_state *k,size_t style);
extern int kate_encode_set_style(kate_state *k,const kate_style *ks);
extern int kate_encode_set_secondary_style_index(kate_state *k,size_t style);
extern int kate_encode_set_secondary_style(kate_state *k,const kate_style *ks);
extern int kate_encode_set_font_mapping_index(kate_state *k,size_t font_mapping);
extern int kate_encode_add_motion(kate_state *k,kate_motion *km,int destroy);
extern int kate_encode_add_motion_index(kate_state *k,size_t motion);
extern int kate_encode_set_palette_index(kate_state *k,size_t palette);
extern int kate_encode_set_palette(kate_state *k,const kate_palette *kp);
extern int kate_encode_set_bitmap_index(kate_state *k,size_t bitmap);
extern int kate_encode_set_bitmap(kate_state *k,const kate_bitmap *kb);

/** \defgroup decoding Decoding */
extern int kate_decode_is_idheader(const kate_packet *kp);
extern int kate_decode_init(kate_state *k,kate_info *ki);
extern int kate_decode_headerin(kate_info *ki,kate_comment *kc,kate_packet *kp);
extern int kate_decode_packetin(kate_state *k,kate_packet *kp);
extern int kate_decode_eventout(kate_state *k,kate_const kate_event **ev); /* event can be NULL */

/** \defgroup tracker Tracker */
extern int kate_tracker_init(kate_tracker *kin,const kate_info *ki,kate_const kate_event *ev);
extern int kate_tracker_clear(kate_tracker *kin);
extern int kate_tracker_update(kate_tracker *kin,kate_float t,int window_w,int window_h,int frame_x,int frame_y,int frame_w,int frame_h);
extern int kate_tracker_morph_styles(kate_style *style,kate_float t,const kate_style *from,const kate_style *to);
extern int kate_tracker_get_text_path_position(kate_tracker *kin,size_t glyph,int *x,int *y);
extern int kate_tracker_update_property_at_duration(const kate_tracker *kin,kate_float duration,kate_float t,kate_motion_semantics semantics,kate_float *x,kate_float *y);

/** \defgroup font Font */
extern int kate_font_get_index_from_code_point(const kate_font_mapping *kfm,int c);

/** \defgroup high High level API */
extern int kate_high_decode_init(kate_state *k);
extern int kate_high_decode_packetin(kate_state *k,kate_packet *kp,kate_const kate_event **ev);
extern int kate_high_decode_clear(kate_state *k);

/** \defgroup packet kate_packet */
extern int kate_packet_wrap(kate_packet *kp,size_t nbytes,const void *data);
extern int kate_packet_init(kate_packet *kp,size_t nbytes,const void *data);
extern int kate_packet_clear(kate_packet *kp);

#ifdef __cplusplus
}
#endif

/** \name Error codes */
/** @{ */
#define KATE_E_NOT_FOUND (-1)            /**< whatever was requested was not found */
#define KATE_E_INVALID_PARAMETER (-2)    /**< a bogus parameter was passed (usually NULL) */
#define KATE_E_OUT_OF_MEMORY (-3)        /**< we're running out of cheese, bring some more */
#define KATE_E_BAD_GRANULE (-4)          /**< decreasing granule */
#define KATE_E_INIT (-5)                 /**< initializing twice, using an uninitialized state, etc */
#define KATE_E_BAD_PACKET (-6)           /**< packet contains invalid data */
#define KATE_E_TEXT (-7)                 /**< invalid/truncated character/sequence, etc */
#define KATE_E_LIMIT (-8)                /**< a limit was exceeded (eg, string too long, pixel value above bpp, etc) */
#define KATE_E_VERSION (-9)              /**< we do not understand that bitstream version */
#define KATE_E_NOT_KATE (-10)            /**< the packet is not a Kate packet */
/** @} */

#endif

