/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


%{

#define KATE_INTERNAL
#include "kate_internal.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <ctype.h>
#include "kate/oggkate.h"
#ifdef HAVE_PNG
#include "kpng.h"
#endif
#include "katedesc.h"
#include "kate_parser.h"

#define CHECK_KATE_API_ERROR(statement) \
  do {                                  \
    int ret=(statement);                \
    if (ret<0) {                        \
      yyerror("error in #statement");   \
      exit(-1);                         \
    }                                   \
  } while(0)

int nerrors=0;
int nwarnings=0;

static char *temp_macro_name=NULL;
static kate_float timebase = (kate_float)0;

typedef kate_style kd_style;
typedef kate_region kd_region;

typedef struct kd_curve {
  size_t idx;
  kate_curve *curve;
} kd_curve;

typedef struct kd_motion {
  size_t idx;
  kate_motion *motion;
} kd_motion;

typedef struct kd_palette {
  kate_palette *palette;
} kd_palette;

typedef struct kd_bitmap {
  kate_bitmap *bitmap;
} kd_bitmap;

static kate_style kstyle;
static kate_region kregion;
static kd_palette kpalette;
static kd_bitmap kbitmap;
static kd_curve kcurve;
static kate_font_range *krange=NULL;
static kate_font_mapping *kmapping=NULL;

static size_t nkmotions=0;
static kd_motion *kmotions=NULL;
static kate_motion *kmotion=NULL;

static char **style_names=NULL;
static char **region_names=NULL;
static char **palette_names=NULL;
static char **bitmap_names=NULL;
static char **curve_names=NULL;
static char **motion_names=NULL;
static char **font_range_names=NULL;
static char **font_mapping_names=NULL;

static size_t n_local_bitmaps=0;
static char **local_bitmap_names=NULL;
static kate_bitmap **local_bitmaps=NULL;

static int open_ended_curve=0;
static int n_curve_pts=-1;
static int n_palette_colors=-1;
static int n_bitmap_pixels=-1;
static size_t n_bytes_in_stream=0;
static char *byte_stream=NULL;
static size_t byte_stream_size=0;

static kate_float karaoke_base_height=(kate_float)0;
static kate_float karaoke_top_height=(kate_float)0;

typedef struct kd_event {
  kate_float t0;
  kate_float t1;
  kate_float duration;
  char *text;
  kate_markup_type text_markup_type;
  int region_index;
  const kate_region *region;
  int style_index;
  const kate_style *style;
  int secondary_style_index;
  const kate_style *secondary_style;
  int palette_index;
  const kate_palette *palette;
  int bitmap_index;
  const kate_bitmap *bitmap;
} kd_event;
static kd_event kevent;

static void katedesc_trace(const char *msg,...)
{
  va_list ap;
  va_start(ap,msg);
  vfprintf(stderr,msg,ap);
  va_end(ap);
}
/* #define KDTRACE(msg,args...) katedesc_trace(msg, ##args) */
#define KDTRACE katedesc_trace

int yyerror(const char *s)
{
  (void)s;
  KDTRACE("Error line %d: %s (token %s)\n",nlines,s,katedesc_text);
  nerrors++;
  return 1;
}

int yyerrorf(const char *msg,...)
{
  static char buffer[4096];
  va_list ap;
  va_start(ap,msg);
  vsnprintf(buffer,sizeof(buffer),msg,ap);
  buffer[sizeof(buffer)-1]=0;
  va_end(ap);
  return yyerror(buffer);
}

int yywarning(const char *s)
{
  (void)s;
  KDTRACE("Warning line %d: %s (token %s)\n",nlines,s,katedesc_text);
  nwarnings++;
  return 1;
}

static void add_meta(kate_meta **km,const char *tag,const char *value)
{
  if (!*km) {
    CHECK_KATE_API_ERROR(kate_meta_create(km));
  }
  CHECK_KATE_API_ERROR(kate_meta_add_string(*km,tag,value));
}

static void add_meta_byte_stream(kate_meta **km,const char *tag)
{
  if (!*km) {
    CHECK_KATE_API_ERROR(kate_meta_create(km));
  }
  CHECK_KATE_API_ERROR(kate_meta_add(*km,tag,byte_stream,byte_stream_size));

  kate_free(byte_stream);
  byte_stream=NULL;
  n_bytes_in_stream=0;
  byte_stream_size=0;
}

static char *catstrings(char *s1,const char *s2)
{
  size_t len;
  char *s;

  if (!s2) { yyerror("internal error: no string to append"); exit(-1); }
  len=(s1?strlen(s1):0)+strlen(s2)+1;
  s=(char*)kate_realloc(s1,len);
  if (!s) { yyerror("out of memory"); exit(-1); }
  if (s1) strcat(s,s2); else strcpy(s,s2);

  return s;
}

static char *dupstring(const char *s)
{
  size_t len;
  char *news;

  if (!s) { yyerror("internal error: no string"); exit(-1); }

  len=strlen(s);
  news=(char*)kate_malloc(len+1);
  if (!news) { yyerror("out of memory"); exit(-1); }
  memcpy(news,s,len+1);
  return news;
}

static int find_item(const char *name,size_t nnames,char **names)
{
  size_t n;
  for (n=0;n<nnames;++n) {
    if (names[n] && !strcmp(names[n],name)) return n;
  }
  yyerrorf("Named item not found: %s",name);
  return 0;
}

static void init_palette(void)
{
  kpalette.palette=(kate_palette*)kate_malloc(sizeof(kate_palette));
  if (!kpalette.palette) { yyerror("out of memory"); exit(-1); }
  if (kate_palette_init(kpalette.palette)<0) {
    yyerror("palette init failed");
    exit(-1);
  }
}

static void generate_full_filename(char *full_filename,size_t size,const char *filename)
{
  if (filename[0]=='/' || filename[0]=='\\') {
    strcpy(full_filename,filename);
  }
  else {
    snprintf(full_filename,size,"%s%s",base_path,filename);
    full_filename[size-1]=0;
  }
}

static void load_palette(const char *filename)
{
#ifdef HAVE_PNG
  int ncolors;
  kate_color *palette=NULL;
  static char full_filename[4096];

  generate_full_filename(full_filename,sizeof(full_filename),filename);
  if (kd_read_png8(full_filename,NULL,NULL,NULL,&palette,&ncolors,NULL)) {
    yyerrorf("failed to load %s",filename);
    return;
  }

  kpalette.palette->ncolors=ncolors;
  kpalette.palette->colors=palette;
#else
  yyerrorf("PNG support not compiled in: cannot load %s",filename);
  exit(-1);
#endif
}

static void check_palette(const kate_palette *kp)
{
  if (!kp) { yyerror("internal error: no palette"); exit(-1); }
}

static void add_palette(kate_info *ki,const char *name,kate_palette *kp)
{
  int ret;

  check_palette(kp);

  ret=kate_info_add_palette(ki,kp);
  if (ret<0) {
    yyerrorf("Failed to register palette: %d",ret);
  }
  else {
    palette_names=(char**)kate_realloc(palette_names,ki->npalettes*sizeof(char*));
    if (!palette_names) {
      yyerror("Out of memory");
      exit(-1);
    }
    palette_names[ki->npalettes-1]=name?dupstring(name):NULL;
  }
}

static int find_palette(const kate_info *ki,const char *name)
{
  return find_item(name,ki->npalettes,palette_names);
}

static void init_bitmap(void)
{
  kbitmap.bitmap=(kate_bitmap*)kate_malloc(sizeof(kate_bitmap));
  if (!kbitmap.bitmap) { yyerror("out of memory"); exit(-1); }
  if (kate_bitmap_init_new(kbitmap.bitmap)<0) {
    yyerror("bitmap init failed");
    exit(-1);
  }
}

static int compute_bitmap_x_offset(kate_float percent)
{
  if (kbitmap.bitmap->width) {
    return (int)(kbitmap.bitmap->width*percent/100+0.5);
  }
  else {
    yyerror("Bitmap width must be known before specifying offset as a percentage");
    return 0;
  }
}

static int compute_bitmap_y_offset(kate_float percent)
{
  if (kbitmap.bitmap->height) {
    return (int)(kbitmap.bitmap->height*percent/100+0.5);
  }
  else {
    yyerror("Bitmap height must be known before specifying offset as a percentage");
    return 0;
  }
}

static void load_bitmap(const char *filename,int paletted)
{
#ifdef HAVE_PNG
  int w,h;
  unsigned char *pixels=NULL;

  if (paletted) {
    int bpp;
    static char full_filename[4096];

    generate_full_filename(full_filename,sizeof(full_filename),filename);
    if (kd_read_png8(full_filename,&w,&h,&bpp,NULL,NULL,&pixels)) {
      yyerrorf("failed to load %s",filename);
      return;
    }

    kbitmap.bitmap->type=kate_bitmap_type_paletted;
    kbitmap.bitmap->width=w;
    kbitmap.bitmap->height=h;
    kbitmap.bitmap->bpp=bpp;
    kbitmap.bitmap->pixels=pixels;
    kbitmap.bitmap->size=0;
  }
  else {
    size_t size;
    static char full_filename[4096];

    generate_full_filename(full_filename,sizeof(full_filename),filename);
    if (kd_read_png(full_filename,&w,&h,&pixels,&size)) {
      yyerrorf("failed to load %s",filename);
      return;
    }

    kbitmap.bitmap->type=kate_bitmap_type_png;
    kbitmap.bitmap->width=w;
    kbitmap.bitmap->height=h;
    kbitmap.bitmap->bpp=0;
    kbitmap.bitmap->pixels=pixels;
    kbitmap.bitmap->size=size;
  }

  kbitmap.bitmap->palette=-1;
#else
  (void)paletted;
  yyerrorf("PNG support not compiled in: cannot load %s",filename);
  exit(-1);
#endif
}

static void check_bitmap(const kate_bitmap *kb)
{
  if (!kb) { yyerror("internal error: no bitmap"); exit(-1); }
}

static void add_bitmap(kate_info *ki,const char *name,kate_bitmap *kb)
{
  int ret;

  check_bitmap(kb);

  ret=kate_info_add_bitmap(ki,kb);
  if (ret<0) {
    yyerrorf("Failed to register bitmap: %d",ret);
  }
  else {
    bitmap_names=(char**)kate_realloc(bitmap_names,ki->nbitmaps*sizeof(char*));
    if (!bitmap_names) {
      yyerror("Out of memory");
      exit(-1);
    }
    bitmap_names[ki->nbitmaps-1]=name?dupstring(name):NULL;
  }
}

static void add_local_bitmap(kate_state *k,const char *name,kate_bitmap *kb)
{
  int ret;

  check_bitmap(kb);

  ret=kate_encode_add_bitmap(k,kb);
  if (ret<0) {
    yyerrorf("Failed to register bitmap: %d",ret);
  }
  else {
    ++n_local_bitmaps;
    local_bitmap_names=(char**)kate_realloc(local_bitmap_names,n_local_bitmaps*sizeof(char*));
    if (!local_bitmap_names) {
      yyerror("Out of memory");
      exit(-1);
    }
    local_bitmap_names[n_local_bitmaps-1]=name?dupstring(name):NULL;
    local_bitmaps=(kate_bitmap**)kate_realloc(local_bitmaps,n_local_bitmaps*sizeof(kate_bitmap*));
    if (!local_bitmaps) {
      yyerror("Out of memory");
      exit(-1);
    }
    local_bitmaps[n_local_bitmaps-1]=kb;
  }
}

static void add_local_bitmap_index(kate_state *k,const char *name,size_t idx)
{
  int ret;

  ret=kate_encode_add_bitmap_index(k,idx);
  if (ret<0) {
    yyerrorf("Failed to register bitmap: %d",ret);
  }
  else {
    ++n_local_bitmaps;
    local_bitmap_names=(char**)kate_realloc(local_bitmap_names,n_local_bitmaps*sizeof(char*));
    if (!local_bitmap_names) {
      yyerror("Out of memory");
      exit(-1);
    }
    local_bitmap_names[n_local_bitmaps-1]=name?dupstring(name):NULL;
    local_bitmaps=(kate_bitmap**)kate_realloc(local_bitmaps,n_local_bitmaps*sizeof(kate_bitmap*));
    if (!local_bitmaps) {
      yyerror("Out of memory");
      exit(-1);
    }
    local_bitmaps[n_local_bitmaps-1]=NULL;
  }
}

static int find_bitmap(const kate_info *ki,const char *name)
{
  int ret=find_item(name,ki->nbitmaps,bitmap_names);
  if (ret>=0) return ret;
  ret=find_item(name,n_local_bitmaps,local_bitmap_names);
  if (ret>=0) return ret+ki->nbitmaps;
  return ret;
}

static void check_style(const kate_style *ks)
{
  if (!ks) { yyerror("internal error: no style"); exit(-1); }
}

static void add_style(kate_info *ki,const char *name,const kate_style *ks)
{
  int ret;
  kate_style *ks2;

  check_style(ks);

  ks2=(kate_style*)kate_malloc(sizeof(*ks2));
  if (!ks2) {
    yyerrorf("Out of memory");
    exit(-1);
  }
  memcpy(ks2,ks,sizeof(*ks2));

  ret=kate_info_add_style(ki,ks2);
  if (ret<0) {
    yyerrorf("Failed to register style: %d",ret);
  }
  else {
    style_names=(char**)kate_realloc(style_names,ki->nstyles*sizeof(char*));
    if (!style_names) {
      yyerror("Out of memory");
      exit(-1);
    }
    style_names[ki->nstyles-1]=name?dupstring(name):NULL;
  }
}

static int find_style(const kate_info *ki,const char *name)
{
  return find_item(name,ki->nstyles,style_names);
}

static void check_region(const kate_region *kr)
{
  if (!kr) { yyerror("internal error: no region"); exit(-1); }
  if (kr->w<0) yyerrorf("Region width (%d) must be non negative",kr->w);
  if (kr->h<0) yyerrorf("Region height (%d) must be non negative",kr->h);
}

static void add_region(kate_info *ki,const char *name,const kate_region *kr)
{
  int ret;
  kate_region *kr2;

  check_region(kr);

  kr2=(kate_region*)kate_malloc(sizeof(*kr2));
  if (!kr2) {
    yyerrorf("Out of memory");
    exit(-1);
  }
  memcpy(kr2,kr,sizeof(*kr2));

  ret=kate_info_add_region(ki,kr2);
  if (ret<0) {
    yyerrorf("Failed to register region: %d",ret);
  }
  else {
    region_names=(char**)kate_realloc(region_names,ki->nregions*sizeof(char*));
    if (!region_names) {
      yyerror("Out of memory");
      exit(-1);
    }
    region_names[ki->nregions-1]=name?dupstring(name):NULL;
  }
}

static int find_region(const kate_info *ki,const char *name)
{
  return find_item(name,ki->nregions,region_names);
}

static void check_curve(const kate_curve *kc)
{
  size_t minpts=0,maxpts=INT_MAX;
  if (!kc) { yyerror("internal error: no curve"); exit(-1); }
  switch (kc->type) {
    case kate_curve_none: minpts=0; maxpts=0; break;
    case kate_curve_static: minpts=1; maxpts=1; break;
    case kate_curve_linear: minpts=2; maxpts=INT_MAX; break;
    case kate_curve_catmull_rom_spline: minpts=2; maxpts=INT_MAX; break;
    case kate_curve_bezier_cubic_spline: minpts=3; maxpts=INT_MAX; break;
    case kate_curve_bspline: minpts=2; maxpts=INT_MAX; break;
  }
  if (kc->npts<minpts) yyerrorf("Curve does not have enough points for this type (has %d, min pts %d)",kc->npts,minpts);
  if (kc->npts>maxpts) yyerrorf("Curve has too many points for this type (has %d, max pts %d)",kc->npts,maxpts);
  if (kc->type==kate_curve_bezier_cubic_spline) {
    if ((kc->npts-1)%3) yyerrorf("Cubic Bezier splines should have 1+3n points");
  }
}

static void add_curve(kate_info *ki,const char *name,kate_curve *kc)
{
  int ret;

  check_curve(kc);

  ret=kate_info_add_curve(ki,kc);
  if (ret<0) {
    yyerrorf("Failed to register curve: %d",ret);
  }
  else {
    curve_names=(char**)kate_realloc(curve_names,ki->ncurves*sizeof(char*));
    if (!curve_names) {
      yyerror("Out of memory");
      exit(-1);
    }
    curve_names[ki->ncurves-1]=name?dupstring(name):NULL;
  }
}

static int find_curve(const kate_info *ki,const char *name)
{
  return find_item(name,ki->ncurves,curve_names);
}

static void clear_motions(void)
{
  kate_free(kmotions);
  kmotions=NULL;
  kmotion=NULL;
  nkmotions=0;
}

static void check_motion(kate_motion *kmotion)
{
  if (!kmotion) { yyerror("internal error: no motion"); exit(-1); }

  if (kmotion->ncurves==0) { yyerror("motion must have at least one curve"); return; }
  if (kmotion->semantics==(kate_motion_semantics)-1) { yyerror("motion semantics is not defined"); return; }
}

static void add_motion(kate_info *ki,const char *name,kate_motion *km)
{
  int ret;

  check_motion(km);

  ret=kate_info_add_motion(ki,km);
  if (ret<0) {
    yyerrorf("Failed to register motion: %d",ret);
  }
  else {
    motion_names=(char**)kate_realloc(motion_names,ki->nmotions*sizeof(char*));
    if (!motion_names) {
      yyerror("Out of memory");
      exit(-1);
    }
    motion_names[ki->nmotions-1]=name?dupstring(name):NULL;
  }

  clear_motions();
}

static int find_motion(const kate_info *ki,const char *name)
{
  return find_item(name,ki->nmotions,motion_names);
}

static void init_style(kd_style *style)
{
  int ret=kate_style_init(style);
  if (ret<0) {
    yyerrorf("Error initializing style: %d\n",ret);
    exit(1);
  }
}

static void set_font_width(kd_style *style,kate_float s,kate_space_metric metric)
{
  if (style->font_width>=0) {
    yyerror("Font width already set");
  }
  if (style->font_height>=0 && style->font_metric!=metric) {
    yyerror("All font size metrics must be the same");
  }
  style->font_width=s;
  style->font_metric=metric;
}

static void set_font_height(kd_style *style,kate_float s,kate_space_metric metric)
{
  if (style->font_height>=0) {
    yyerror("Font height already set");
  }
  if (style->font_width>=0 && style->font_metric!=metric) {
    yyerror("All font size metrics must be the same");
  }
  style->font_height=s;
  style->font_metric=metric;
}

static void set_font(kd_style *style,const char *font)
{
  size_t len;
  if (!font) {
    yyerror("Internal error: no font");
    exit(-1);
  }
  len=strlen(font);
  if (style->font) {
    yyerror("Font already set");
  }
  style->font=(char*)kate_malloc(len+1);
  if (!style->font) {
    yyerror("out of memory");
    exit(-1);
  }
  strcpy(style->font,font);
}

static void set_font_size(kd_style *style,kate_float s,kate_space_metric metric)
{
  if (style->font_height>=0 || style->font_width>=0) {
    yyerror("Font width and/or height already set");
  }
  style->font_width=s;
  style->font_height=s;
  style->font_metric=metric;
}

static void set_margin(kd_style *style,kate_float *margin,kate_float m,kate_space_metric metric)
{
  int metric_already_set=0;
  if (&style->left_margin!=margin && style->left_margin!=0) metric_already_set=1;
  if (&style->top_margin!=margin && style->top_margin!=0) metric_already_set=1;
  if (&style->right_margin!=margin && style->right_margin!=0) metric_already_set=1;
  if (&style->bottom_margin!=margin && style->bottom_margin!=0) metric_already_set=1;
  if (metric_already_set && metric!=style->margin_metric) {
    yyerror("Metric must be the same for all margins");
    return;
  }
  *margin=m;
  style->margin_metric=metric;
}

static void set_margins(kd_style *style,
                        kate_float left,kate_space_metric left_metric,
                        kate_float top,kate_space_metric top_metric,
                        kate_float right,kate_space_metric right_metric,
                        kate_float bottom,kate_space_metric bottom_metric)
{
  if (left_metric!=right_metric || left_metric!=top_metric || left_metric!=bottom_metric) {
    yyerror("Metric must be the same for all margins");
    return;
  }
  style->left_margin=left;
  style->top_margin=top;
  style->right_margin=right;
  style->bottom_margin=bottom;
  style->margin_metric=left_metric;
}

static void init_style_from(int idx)
{
  const kate_style *from=ki.styles[idx];
  init_style(&kstyle);
  memcpy(&kstyle,from,sizeof(kate_style));
}

static void init_region(kd_region *region)
{
  int ret=kate_region_init(region);
  if (ret<0) {
    yyerrorf("Error initializing region: %d\n",ret);
    exit(1);
  }
}

static void init_region_from(int idx)
{
  const kate_region *from=ki.regions[idx];
  init_region(&kregion);
  memcpy(&kregion,from,sizeof(kate_region));
}

static void reference_curve_from(int idx)
{
  kcurve.idx=idx;
  kcurve.curve=NULL;
}

static void init_curve(void)
{
  kcurve.idx=0;
  kcurve.curve=(kate_curve*)kate_malloc(sizeof(kate_curve));
  if (!kcurve.curve) { yyerror("out of memory"); exit(-1); }
  if (kate_curve_init(kcurve.curve)<0) {
    yyerror("error initializing curve");
    exit(-1);
  }
}

static void init_curve_from(int idx)
{
  const kate_curve *from=ki.curves[idx];
  if (!from || !from->pts) {
    yyerror("invalid curve to init from");
    exit(-1);
  }
  init_curve();
  memcpy(kcurve.curve,from,sizeof(kate_curve));
  kcurve.curve->pts=(kate_float*)kate_malloc(kcurve.curve->npts*2*sizeof(kate_float));
  if (!kcurve.curve->pts) {
    yyerror("out of memory");
    exit(-1);
  }
  memcpy(kcurve.curve->pts,from->pts,kcurve.curve->npts*2*sizeof(kate_float));
}

static void init_curve_points(int npts)
{
  if (!kcurve.curve) { yyerror("internal error: curve not initialized"); exit(-1); }
  if (n_curve_pts<0) { katedesc_error("Curve type must be specified before the points"); exit(-1); }
  if (npts<=0) { katedesc_error("Number of points cannot be negative or zero"); exit(-1); }
  kcurve.curve->npts=npts;
  kcurve.curve->pts=(kate_float*)kate_malloc(npts*2*sizeof(kate_float));
  if (!kcurve.curve->pts) {
    yyerror("Out of memory");
    exit(-1);
  }
  open_ended_curve=0;
}

static void init_open_ended_curve_points(void)
{
  if (!kcurve.curve) { yyerror("internal error: curve not initialized"); exit(-1); }
  if (n_curve_pts<0) katedesc_error("Curve type must be specified before the points");
  kcurve.curve->npts=0;
  kcurve.curve->pts=NULL;
  open_ended_curve=1;
}

static void add_open_ended_curve_point(kate_float pt)
{
  if (!kcurve.curve) { yyerror("internal error: curve not initialized"); exit(-1); }
  ++n_curve_pts;
  kcurve.curve->npts=(n_curve_pts+1)/2;
  kcurve.curve->pts=(kate_float*)kate_realloc(kcurve.curve->pts,kcurve.curve->npts*2*sizeof(kate_float));
  if (!kcurve.curve->pts) {
    yyerror("Out of memory");
    exit(-1);
  }
  kcurve.curve->pts[n_curve_pts-1]=pt;
}

static void init_palette_colors(int ncolors)
{
  if (!kpalette.palette) { yyerror("internal error: palette not initialized"); exit(-1); }
  kpalette.palette->ncolors=ncolors;
  kpalette.palette->colors=(kate_color*)kate_malloc(ncolors*sizeof(kate_color));
  if (!kpalette.palette->colors) {
    yyerror("Out of memory");
    exit(-1);
  }
  n_palette_colors=0;
}

static void init_bitmap_pixels(int width,int height,int bpp)
{
  if (!kbitmap.bitmap) { yyerror("internal error: bitmap not initialized"); exit(-1); }
  if (width<=0 || height<=0) yyerror("Bitmap dimensions must be positive");
  if (bpp<=0) yyerror("Bitmap bit depth must be positive");
  if (bpp>8) yyerrorf("Bitmap bit depth must not be more than 8 (is %d)",bpp);
  kbitmap.bitmap->type=kate_bitmap_type_paletted;
  kbitmap.bitmap->width=width;
  kbitmap.bitmap->height=height;
  kbitmap.bitmap->bpp=bpp;
  kbitmap.bitmap->pixels=(unsigned char*)kate_malloc(width*height*sizeof(unsigned char));
  if (!kbitmap.bitmap->pixels) {
    yyerror("Out of memory");
    exit(-1);
  }
  kbitmap.bitmap->size=0;
  n_bitmap_pixels=0;
}

static void init_png_bitmap_pixels(int width,int height,int size)
{
  if (!kbitmap.bitmap) { yyerror("internal error: bitmap not initialized"); exit(-1); }
  if (width<=0 || height<=0) yyerror("Bitmap dimensions must be positive");
  kbitmap.bitmap->type=kate_bitmap_type_png;
  kbitmap.bitmap->width=width;
  kbitmap.bitmap->height=height;
  kbitmap.bitmap->bpp=0;
  kbitmap.bitmap->pixels=(unsigned char*)kate_malloc(size);
  if (!kbitmap.bitmap->pixels) {
    yyerror("Out of memory");
    exit(-1);
  }
  kbitmap.bitmap->size=size;
  n_bitmap_pixels=0;
}

static void init_byte_stream(int nbytes)
{
  if (nbytes<0) { yyerror("internal error: negative number of bytes"); exit(-1); }
  byte_stream=(char*)kate_malloc(nbytes);
  if (!byte_stream) {
    yyerror("Out of memory");
    exit(-1);
  }
  n_bytes_in_stream=0;
  byte_stream_size=nbytes;
}

static void set_color(kate_color *kc,uint32_t c)
{
  int r=(c>>24)&0xff;
  int g=(c>>16)&0xff;
  int b=(c>>8)&0xff;
  int a=(c>>0)&0xff;
  if (!kc) {
    yyerror("Internal error: null color");
    exit(-1);
  }
  if (r<0 || r>255) yyerrorf("red component (%d) must be between 0 and 255",r);
  if (g<0 || g>255) yyerrorf("green component (%d) must be between 0 and 255",g);
  if (b<0 || b>255) yyerrorf("blue component (%d) must be between 0 and 255",b);
  if (a<0 || a>255) yyerrorf("alpha component (%d) must be between 0 and 255",a);
  kc->r=r;
  kc->g=g;
  kc->b=b;
  kc->a=a;
}

static void init_event(kd_event *ev)
{
  if (!ev) {
    yyerror("Internal error: null event");
    exit(-1);
  }
  ev->t0=ev->t1=ev->duration=(kate_float)-1.0;
  ev->text=NULL;
  ev->text_markup_type=kate_markup_none;
  ev->region_index=ev->style_index=ev->secondary_style_index=-1;
  ev->region=NULL;
  ev->style=NULL;
  ev->secondary_style=NULL;
  ev->palette_index=ev->bitmap_index=-1;
  ev->palette=NULL;
  ev->bitmap=NULL;
}

static void kd_encode_set_id(kate_state *kstate,unsigned int id)
{
  if (id&0x80000000) yyerrorf("ID %d (hex %x) out of range, must fit on 31 bits",id,id);

  /* unused at the moment - will need to map an autogenerated id to this user id */
  (void)kstate;
  (void)id;
}

static int add_entity(const char *entity,char **text,size_t *wlen0)
{
  int count=0;

  if (!entity || !text || !wlen0) {
    yyerror("internal error: add_entity passed null parameter");
    exit(-1);
  }

  /* write out the entity */
  while (*entity) {
    int ret=kate_text_set_character(kate_utf8,*entity,text,wlen0);
    if (ret<0) return ret;
    count+=ret;
    ++entity;
  }
  return count;
}

static char *expand_numeric_entities(const char *text)
{
  enum {
    s_text,
    s_amp,
    s_code,
    s_named
  } state=s_text;
  int c,code=0,code_from_numeric,base=-1;
  size_t len=strlen(text),len0=len+1,rlen0=len0,wlen0=len0,allocated=len0;
  /* we might need to replace "&#00;" with "&amp;" - we can't use more characters, so we needn't allocate more */
  /* this might change if we even can replace a numeric entity with a named one that is longer as a string */
  char *newtext=(char*)kate_malloc(allocated),*newtextptr=newtext;

  if (!newtext) { yyerror("out of memory"); exit(-1); }

  while (1) {
    int ret=kate_text_get_character(kate_utf8,&text,&rlen0);
    if (ret<0) {
      yyerrorf("failed to read character: %d",ret);
      kate_free(newtext);
      return NULL;
    }
    c=ret;

    code_from_numeric=0;
    switch (state) {
      case s_text:
        if (c=='&') {
          state=s_amp;
          code=0;
        }
        break;
      case s_amp:
        if (c=='#') {
          state=s_code;
          base=-1; /* unknown yet */
        }
        else {
          state=s_named;

          /* we've encountered a named entity, and we've discarded the & already,
             so we need to add it now before the newly read character */
          ret=kate_text_set_character(kate_utf8,'&',&newtextptr,&wlen0);
          if (ret<0) {
            yyerrorf("failed to write character: %d",ret);
            kate_free(newtext);
            return NULL;
          }
        }
        break;
      case s_code:
        if (c==';') {
          if (base<0) {
            /* no code given */
            yyerrorf("no code given in numeric entity");
            c=0;
            code_from_numeric=1;
            state=s_text;
          }
          else {
            c=code; /* this will be written below */
            code_from_numeric=1;
            state=s_text;
          }
        }
        else {
          /* if first character, determine if this is decimal or hexadecimal */
          if (base<0) {
            if (c=='x' || c=='X') {
              base=16;
              break; /* the code starts next character */
            }
            else {
              base=10;
              /* fall through */
            }
          }

          code*=base;
          if (isdigit(c)) code+=(c-'0');
          else if (base==16 && isxdigit(c)) code+=(tolower(c)-'a'+10);
          else yyerrorf("invalid character in numeric entity (only numeric entities are supported), got %d",c);
        }
        break;
      case s_named:
        if (c==';') {
          state=s_text;
        }
        break;
    }

    if (state==s_text || state==s_named) {
      /* we don't want to expand characters in "<&>" as they would then be wrongly interpreted,
         so we insert here as entities - note that we don't want to expand them and do another
         pass to reencode them, as we then might pick up others that *were* in the text verbatim */
      if (code_from_numeric && (c&~0xff)==0 && strchr("<&>",c)) {
        switch (c) {
          case '<': ret=add_entity("&lt;",&newtextptr,&wlen0); break;
          case '&': ret=add_entity("&amp;",&newtextptr,&wlen0); break;
          case '>': ret=add_entity("&gt;",&newtextptr,&wlen0); break;
        }
      }
      else {
        ret=kate_text_set_character(kate_utf8,c,&newtextptr,&wlen0);
      }
      if (ret<0) {
        yyerrorf("failed to write character: %d",ret);
        kate_free(newtext);
        return NULL;
      }
    }
    if (c==0) break;
  }
  return newtext;
}

static char *getrawline(const char **text)
{
  size_t rlen0;
  int newline,in_newline=0;
  const char *start_of_line;
  int c;

  if (!text || !*text) {
    yyerror("error: getrawline passed invalid text pointer");
    exit(-1);
  }

  rlen0=strlen(*text)+1;
  start_of_line=*text;

  while (1) {
    const char *ptr=*text;
    int ret=kate_text_get_character(kate_utf8,text,&rlen0);
    if (ret<0) {
      yyerrorf("failed to read character: %d",ret);
      return NULL;
    }
    c=ret;
    if (c==0) {
      /* end of the string, return everything */
      size_t len=strlen(start_of_line);
      char *line=(char*)kate_malloc(len+1);
      memcpy(line,start_of_line,len+1);
      *text=ptr; /* do not push past the start of the new line */
      return line;
    }
    newline=(c=='\r' || c=='\n');
    if (!newline && in_newline) {
      /* we are at the start of a new line */
      char *line=(char*)kate_malloc(ptr-start_of_line+1);
      memcpy(line,start_of_line,ptr-start_of_line);
      line[ptr-start_of_line]=0;
      *text=ptr; /* do not push past the start of the new line */
      return line;
    }
    in_newline=newline;
  }
}

static void trimend(char *line,size_t rlen0,int *eol)
{
  int ret;
  int c;
  int ws;
  char *text=line;

  if (!line) return;
  
  ret=kate_text_get_character(kate_utf8,(const char**)&text,&rlen0);
  if (ret<0) {
    yyerrorf("failed to read character: %d",ret);
    return;
  }
  c=ret;
  if (c==0) {
    *eol=1;
    return;
  }

  trimend(text,rlen0,eol);

  ws=((c<=0xff) && (strchr(" \t\n\r",c)!=NULL));
  if (*eol && ws) *line=0;
  if (!ws) *eol=0;
}

static char *trimline(const char *line)
{
  char *trimmed;
  size_t rlen0;
  int c;
  int eol=0;

  if (!line) {
    yyerror("error: trimline passed null line");
    exit(-1);
  }

  rlen0=strlen(line)+1;

  /* first seek to the first non whitespace character in the line */
  while (1) {
    const char *ptr=line;
    int ret=kate_text_get_character(kate_utf8,&ptr,&rlen0);
    if (ret<0) {
      yyerrorf("failed to read character: %d",ret);
      return NULL;
    }
    c=ret;
    if (!c || (c!=' ' && c!='\t')) {
      /* we found a non whitespace, or an end of line, stop */
      break;
    }
    /* we can advance the start of the line */
    line=ptr;
  }

  rlen0=strlen(line)+1;
  trimmed=(char*)kate_malloc(rlen0);
  memcpy(trimmed,line,rlen0);
  trimend(trimmed,strlen(trimmed)+1,&eol);
  return trimmed;
}

static char *trimtext(const char *text)
{
  char *newtext=(char*)kate_malloc(1);
  *newtext=0;
  while (text && *text) {
    char *line=getrawline(&text);
    char *trimmed=trimline(line);
    kate_free(line);
    if (*trimmed && strcmp(trimmed,"\n")) {
      /* ignore empty lines */
      if (newtext && *newtext) {
        /* add a newline between lines (eg, not before the first line) */
        newtext=catstrings(newtext,"\n");
      }
      newtext=catstrings(newtext,trimmed);
    }
    kate_free(trimmed);
  }
  return newtext;
}

static char *trimtext_pre(const char *text)
{
  /* in pre, we just kill a new line at start and one at the end, if any */
  size_t len;
  char *newtext;

  if (!text) {
    yyerror("error: trimtext_pre passed null text");
    exit(-1);
  }

  len=strlen(text);
  newtext=(char*)kate_malloc(len+1);

  /* start */
  if (*text=='\n') {
    memcpy(newtext,text+1,len);
    --len;
  }
  else {
    memcpy(newtext,text,len+1);
  }

  /* end */
  if (len>0 && newtext[len-1]=='\n') {
    newtext[len-1]=0;
  }

  return newtext;
}

static void backslash_n_to_newline(char *text)
{
  char *ptr=text;
  while (ptr && (ptr=strstr(ptr,"\\n"))) {
    *ptr='\n';
    memmove(ptr+1,ptr+2,strlen(ptr+2)+1);
  }
}

static void set_event_text(kd_event *ev,const char *text,int pre,int markup)
{
  char *newtext,*expanded;
  size_t len;

  if (ev->text) {
    yyerrorf("text already set (to %s, trying to set to %s)",ev->text,text);
    return;
  }
  if (!text) {
    yyerror("null text");
    return;
  }

  len=strlen(text);
  newtext=(char*)kate_malloc(len+1);
  memcpy(newtext,text,len+1);
  backslash_n_to_newline(newtext);

  expanded=expand_numeric_entities(newtext);
  kate_free(newtext);
  newtext=expanded;

  if (!pre) {
    char *trimmed_newtext=trimtext(newtext);
    kate_free(newtext);
    newtext=trimmed_newtext;
  }
  else {
    char *trimmed_newtext=trimtext_pre(newtext);
    kate_free(newtext);
    newtext=trimmed_newtext;
  }

  if (markup) {
    ev->text_markup_type=kate_markup_simple;
  }
  else {
    ev->text_markup_type=kate_markup_none;
  }

  ev->text=newtext;
}

static void set_event_text_from(kd_event *ev,const char *source,int pre,int markup)
{
  FILE *f;
  char *text=NULL;
  char s[4096],*sret;

  f=fopen(source,"rt");
  if (!f) {
    yyerrorf("Failed to open file %s\n",source);
    exit(-1);
  }
  sret=fgets(s,sizeof(s),f);
  if (!sret) {
    yyerrorf("Failed to read from file %s\n",source);
    exit(-1);
  }
  while (!feof(f)) {
    /* This implicitely forbids embedded zeros - could this be a problem ? */
    text=catstrings(text,s);
    sret=fgets(s,sizeof(s),f);
    if (!sret) {
      yyerrorf("Failed to read from file %s\n",source);
      exit(-1);
    }
  }
  fclose(f);

  if (text) {
    set_event_text(ev,text,pre,markup);
    kate_free(text);
  }
}

static void set_event_t0_t1(kd_event *ev,kate_float t0,kate_float t1)
{
  if (ev->t0>=0) { yyerror("start time already set"); return; }
  if (ev->t1>=0) { yyerror("end time already set"); return; }
  ev->t0=t0;
  ev->t1=t1;
}

static void set_event_t0(kd_event *ev,kate_float v)
{
  if (ev->t0>=0) { yyerror("start time already set"); return; }
  ev->t0=v;
}

static void set_event_t1(kd_event *ev,kate_float v)
{
  if (ev->t1>=0) { yyerror("end time already set"); return; }
  ev->t1=v;
}

static void set_event_duration(kd_event *ev,kate_float v)
{
  if (ev->duration>=0) { yyerror("duration already set"); return; }
  ev->duration=v;
}

static void set_event_region_index(kd_event *ev,int r)
{
  int ret;
  if (ev->region_index>=0 || ev->region) { yyerror("region already set"); return; }
  ev->region_index=r;
  ret=kate_encode_set_region_index(&k,r);
  if (ret<0) yyerrorf("failed to set region index: %d",ret);
}

static void set_event_region(kd_event *ev,kate_region *kr)
{
  int ret;
  check_region(kr);
  if (ev->region_index>=0 || ev->region) { yyerror("region already set"); return; }
  ev->region=kr;
  ret=kate_encode_set_region(&k,kr);
  if (ret<0) yyerrorf("failed to set region: %d",ret);
}

static void set_event_style_index(kd_event *ev,int s)
{
  int ret;
  if (ev->style_index>=0 || ev->style) { yyerror("style already set"); return; }
  ev->style_index=s;
  ret=kate_encode_set_style_index(&k,s);
  if (ret<0) yyerrorf("failed to set style index: %d",ret);
}

static void set_event_secondary_style_index(kd_event *ev,int s)
{
  int ret;
  if (ev->secondary_style_index>=0 || ev->secondary_style) { yyerror("secondary style already set"); return; }
  ev->secondary_style_index=s;
  ret=kate_encode_set_secondary_style_index(&k,s);
  if (ret<0) yyerrorf("failed to set secondary style index: %d",ret);
}

static void set_event_style(kd_event *ev,kate_style *ks)
{
  int ret;
  check_style(ks);
  if (ev->style_index>=0 || ev->style) { yyerror("style already set"); return; }
  ev->style=ks;
  ret=kate_encode_set_style(&k,ks);
  if (ret<0) yyerrorf("failed to set style: %d",ret);
}

static void set_event_secondary_style(kd_event *ev,kate_style *ks)
{
  int ret;
  check_style(ks);
  if (ev->secondary_style_index>=0 || ev->secondary_style) { yyerror("secondary style already set"); return; }
  ev->secondary_style=ks;
  ret=kate_encode_set_secondary_style(&k,ks);
  if (ret<0) yyerrorf("failed to set secondary style: %d",ret);
}

static void set_event_palette_index(kd_event *ev,int p)
{
  int ret;
  if (ev->palette_index>=0 || ev->palette) { yyerror("palette already set"); return; }
  ev->palette_index=p;
  ret=kate_encode_set_palette_index(&k,p);
  if (ret<0) yyerrorf("failed to set palette index: %d",ret);
}

static void set_event_palette(kd_event *ev,kate_palette *kp)
{
  int ret;
  check_palette(kp);
  if (ev->palette_index>=0 || ev->palette) { yyerror("palette already set"); return; }
  ev->palette=kp;
  ret=kate_encode_set_palette(&k,kp);
  if (ret<0) yyerrorf("failed to set palette: %d",ret);
}

static void set_event_bitmap_index(kd_event *ev,int b)
{
  int ret;
  if (ev->bitmap_index>=0 || ev->bitmap) { yyerror("bitmap already set"); return; }
  ev->bitmap_index=b;
  ret=kate_encode_set_bitmap_index(&k,b);
  if (ret<0) yyerrorf("failed to set bitmap index: %d",ret);
}

static void set_event_bitmap(kd_event *ev,kate_bitmap *kb)
{
  int ret;
  check_bitmap(kb);
  if (ev->bitmap_index>=0 || ev->bitmap) { yyerror("bitmap already set"); return; }
  ev->bitmap=kb;
  ret=kate_encode_set_bitmap(&k,kb);
  if (ret<0) yyerrorf("failed to set bitmap: %d",ret);
}

static void kd_add_event_meta(const char *tag,const char *value)
{
  kate_meta *meta;
  int ret;

  ret=kate_meta_create(&meta);
  if (ret>=0) {
    ret=kate_meta_add_string(meta,tag,value);
    if (ret>=0) {
      ret=kate_encode_merge_meta(&k,meta);
      if (ret<0) {
        kate_meta_destroy(meta);
      }
    }
    else {
      kate_meta_destroy(meta);
    }
  }
  if (ret<0) yyerrorf("failed to add metadata: %d",ret);
}

static void kd_add_event_meta_byte_stream(const char *tag)
{
  kate_meta *meta;
  int ret;

  ret=kate_meta_create(&meta);
  if (ret>=0) {
    ret=kate_meta_add(meta,tag,byte_stream,byte_stream_size);
    if (ret>=0) {
      ret=kate_encode_merge_meta(&k,meta);
      if (ret<0) {
        kate_meta_destroy(meta);
      }
    }
    else {
      kate_meta_destroy(meta);
    }
  }
  if (ret<0) yyerrorf("failed to add metadata: %d",ret);

  kate_free(byte_stream);
  byte_stream=NULL;
  n_bytes_in_stream=0;
  byte_stream_size=0;
}

static kd_event *check_event(kd_event *ev)
{
  /* we can set:
     start and end
     start and duration
     duration and end
     */
  int sets=0;
  if (ev->t0>=(kate_float)0.0) ++sets;
  if (ev->t1>=(kate_float)0.0) ++sets;
  if (ev->duration>=(kate_float)0.0) ++sets;
  if (sets<2) { yyerror("start/end times underspecified"); return NULL; }
  if (sets>2) { yyerror("start/end times overspecified"); return NULL; }

  if (ev->t0<(kate_float)0.0) ev->t0=ev->t1-ev->duration;
  if (ev->t1<(kate_float)0.0) ev->t1=ev->t0+ev->duration;

  return ev;
}

static void init_motion(void)
{
  kmotions=(kd_motion*)kate_realloc(kmotions,(nkmotions+1)*sizeof(kd_motion));
  if (!kmotions) { yyerror("out of memory"); exit(-1); }
  ++nkmotions;
  kmotions[nkmotions-1].idx=0;
  kmotions[nkmotions-1].motion=kate_malloc(sizeof(kate_motion));
  kmotion=kmotions[nkmotions-1].motion;
  if (!kmotion) { yyerror("out of memory"); exit(-1); }

  if (kate_motion_init(kmotion)<0) {
    yyerror("failed to init motion");
    exit(-1);
  }
  kmotion->semantics=(kate_motion_semantics)-1;
}

static void add_curve_to_motion(kate_motion *kmotion,kate_float duration)
{
  kmotion->ncurves++;
  kmotion->curves=(kate_curve**)kate_realloc(kmotion->curves,kmotion->ncurves*sizeof(kate_curve*));
  if (!kmotion->curves) { yyerror("out of memory"); exit(-1); }
  kmotion->durations=(kate_float*)kate_realloc(kmotion->durations,kmotion->ncurves*sizeof(kate_float));
  if (!kmotion->durations) { yyerror("out of memory"); exit(-1); }

  if (kcurve.curve) {
    check_curve(kcurve.curve);
    kmotion->curves[kmotion->ncurves-1]=kcurve.curve;
    kcurve.curve=NULL;
  }
  else {
    if (kcurve.idx>=ki.ncurves) {
      yyerror("Internal error: curve index out of range");
      exit(-1);
    }
    kmotion->curves[kmotion->ncurves-1]=ki.curves[kcurve.idx];
  }

  kmotion->durations[kmotion->ncurves-1]=duration;
}

static size_t get_num_glyphs(const char *text)
{
  size_t len0;
  size_t nglyphs=0;
  int intag=0,c;

  if (!text) { yyerror("Internal error: get_num_glyphs got NULL text"); exit(-1); }

  len0=strlen(text)+1;
  while ((c=kate_text_get_character(kate_utf8,&text,&len0))>0) {
    if (c=='<') intag++;
    if (!intag) ++nglyphs;
    if (c=='>') intag--;
  }
  return nglyphs;
}

static void init_simple_glyph_pointer_motion(void)
{
  init_motion();
  kmotion->semantics=kate_motion_semantics_glyph_pointer_1;
  karaoke_base_height=(kate_float)0;
  karaoke_top_height=(kate_float)0;
}

static int get_glyph_pointer_offset(unsigned int pointer_id)
{
  if (pointer_id<1 || pointer_id>4) {
    yyerrorf("Only glyph pointers 1-4 are available (trying to set %d)",pointer_id);
    exit(-1);
  }
  return (kate_motion_semantics)(kate_motion_semantics_glyph_pointer_1+pointer_id-1);
}

static kate_float get_last_glyph_x(const kate_motion *km)
{
  const kate_curve *kc;
  if (!km) { yyerror("internal error: no motion"); exit(-1); }
  if (km->ncurves==0) return (kate_float)-0.5; /* by default, center of the glyph before the first one (eg, marks nothing yet) */
  kc=km->curves[km->ncurves-1];
  if (kc->npts<1) yyerror("internal error: no points in last curve");
  return kc->pts[kc->npts*2-2]; /* -1 would be y */
}

static kate_float compute_karaoke_height(float y)
{
  /* turn height (bottom to top) to screen coordinates (top to bottom): negate */
  return -(karaoke_base_height*(1-y)+karaoke_top_height*y);
}

static void add_glyph_pause(kate_float dt,kate_float y)
{
  init_curve();
  kcurve.curve->type=kate_curve_static;
  kcurve.curve->npts=1;
  kcurve.curve->pts=(kate_float*)kate_malloc(kcurve.curve->npts*2*sizeof(kate_float));
  if (!kcurve.curve->pts) { yyerror("out of memory"); exit(-1); }
  kcurve.curve->pts[0]=get_last_glyph_x(kmotion);
  kcurve.curve->pts[1]=compute_karaoke_height(y);
  add_curve_to_motion(kmotion,dt);
}

static void add_glyph_transition(unsigned int glyph,kate_float dt,kate_float ystart,kate_float ytop,int absolute,kate_float pause_fraction)
{
  /* get the last glyph position and the new one */
  kate_float x0=get_last_glyph_x(kmotion);
  kate_float x1=glyph+(kate_float)0.5;
  size_t n;

  /* convert absolute to relative */
  if (absolute) {
    for (n=0;n<kmotion->ncurves;++n) dt-=kmotion->durations[n];
  }

  if (dt<(kate_float)0.0) {
    yyerrorf("duration (%f) must not be negative\n",dt);
    exit(-1);
  }

  /* add a pause before the next jump */
  if (pause_fraction>(kate_float)0.0) {
    kate_float delay=dt*pause_fraction;
    add_glyph_pause(delay,ystart);
    dt-=delay;
    if (dt<(kate_float)0.0) dt=(kate_float)0.0;
    x0=get_last_glyph_x(kmotion);
  }

  init_curve();

  if (dt==(kate_float)0.0) {
    /* if zero duration, just add static point at the end point */
    kcurve.curve->type=kate_curve_static;
    kcurve.curve->npts=1;
    kcurve.curve->pts=(kate_float*)kate_malloc(kcurve.curve->npts*2*sizeof(kate_float));
    if (!kcurve.curve->pts) { yyerror("out of memory"); exit(-1); }
    /* directly at the end position */
    kcurve.curve->pts[0]=x1;
    kcurve.curve->pts[1]=compute_karaoke_height(ystart);
  }
  else {
    kcurve.curve->type=kate_curve_catmull_rom_spline;
    kcurve.curve->npts=3+2; /* the two end points are duplicated */
    kcurve.curve->pts=(kate_float*)kate_malloc(kcurve.curve->npts*2*sizeof(kate_float));
    if (!kcurve.curve->pts) { yyerror("out of memory"); exit(-1); }

    /* start position */
    kcurve.curve->pts[0]=kcurve.curve->pts[2]=x0;
    kcurve.curve->pts[1]=kcurve.curve->pts[3]=compute_karaoke_height(ystart);

    /* the interpolated points */
    for (n=2;n<kcurve.curve->npts-2;++n) {
      kate_float t=(n-1)/(kate_float)(kcurve.curve->npts-2-1);
      kcurve.curve->pts[n*2]=kcurve.curve->pts[n*2+2]=x1*t+x0*((kate_float)1.0-t);
      kcurve.curve->pts[n*2+1]=kcurve.curve->pts[n*2+3]=compute_karaoke_height(ytop);
    }

    /* end position */
    kcurve.curve->pts[kcurve.curve->npts*2-4]=kcurve.curve->pts[kcurve.curve->npts*2-2]=x1;
    kcurve.curve->pts[kcurve.curve->npts*2-3]=kcurve.curve->pts[kcurve.curve->npts*2-1]=compute_karaoke_height(ystart);
  }

  add_curve_to_motion(kmotion,dt);
}

static void add_glyph_transition_to_text(const char *text,kate_float dt,kate_float ystart,kate_float ytop,int absolute,kate_float pause_fraction)
{
  char *newtext;

  if (!text) {
    yyerror("null text");
    return;
  }

  if (kevent.text) {
    char *text2=(char*)kate_realloc(kevent.text,strlen(kevent.text)+strlen(text)+1);
    if (!text2) {
      yyerror("out of memory");
      return;
    }
    strcat(text2,text);
    kevent.text=text2;
  }
  else {
    char *text2=(char*)kate_malloc(strlen(text)+1);
    if (!text2) {
      yyerror("out of memory");
      return;
    }
    strcpy(text2,text);
    kevent.text=text2;
  }

  backslash_n_to_newline(kevent.text);
  newtext=expand_numeric_entities(kevent.text);
  if (!newtext) return;
  kate_free(kevent.text);
  kevent.text=newtext;

  add_glyph_transition(get_num_glyphs(kevent.text)-1,dt,ystart,ytop,absolute,pause_fraction);
}

static void set_style_morph(kd_event *ev,int from,int to)
{
  int ret;

  if (!ev) {
    yyerror("internal error: no event");
    exit(-1);
  }
  if (from<0 || to<0) {
    yyerror("error: style index cannot be negative");
    exit(-1);
  }

  if (ev->style_index>=0 || ev->style) { yyerror("style already set"); return; }
  ev->style_index=from;
  ret=kate_encode_set_style_index(&k,from);
  if (ret<0) yyerrorf("failed to set style index: %d",ret);

  if (ev->secondary_style_index>=0 || ev->secondary_style) { yyerror("secondary style already set"); return; }
  ev->secondary_style_index=to;
  ret=kate_encode_set_secondary_style_index(&k,to);
  if (ret<0) yyerrorf("failed to set secondary_style index: %d",ret);
}

static void clear_local_bitmaps(void)
{
  size_t n;

  if (local_bitmap_names) {
    for (n=0;n<n_local_bitmaps;++n) {
      if (local_bitmap_names[n]) kate_free(local_bitmap_names[n]);
      if (local_bitmaps[n]) {
        if(local_bitmaps[n]->meta) kate_meta_destroy(local_bitmaps[n]->meta);
        kate_free(local_bitmaps[n]->pixels);
        kate_free(local_bitmaps[n]);
      }
    }
    kate_free(local_bitmap_names);
    local_bitmap_names=NULL;
    kate_free(local_bitmaps);
    local_bitmaps=NULL;
  }
  n_local_bitmaps=0;
}

static void clear_event(kd_event *ev)
{
  if (!ev) {
    yyerror("internal error: no event");
    exit(-1);
  }
  if (ev->text) {
    kate_free(ev->text);
    ev->text=NULL;
  }
  clear_motions();
  clear_local_bitmaps();
}

static void kd_finalize_simple_timed_glyph_motion(kate_motion *kmotion)
{
  const kd_event *ev=&kevent;
  kate_float duration=ev->duration;
  kate_float t0=ev->t0,t1=ev->t1;
  kate_float duration_so_far;
  size_t n;
  int sets;

  if (!kmotion) {
    yyerror("internal error: kd_finalize_simple_timed_glyph_motion passed NULL motion");
    exit(-1);
  }

  /* for this helper motion, we require the timing of the event to be known in advance */
  sets=0;
  if (t0>=(kate_float)0.0) ++sets;
  if (t1>=(kate_float)0.0) ++sets;
  if (duration>=(kate_float)0.0) ++sets;
  if (sets<2) { yyerror("start/end times must be specified before timed glyph marker motion setup"); return; }
  if (sets>2) { yyerror("start/end times overspecified"); return; }

  if (t0<(kate_float)0.0) t0=t1-duration;
  if (t1<(kate_float)0.0) t1=t0+duration;
  duration=t1-t0;

  /* add a pause to take us to the end time */
  duration_so_far=(kate_float)0.0;
  for (n=0;n<kmotion->ncurves;++n) duration_so_far+=kmotion->durations[n];
  if (duration_so_far>duration) {
    yyerrorf("Simple timed glyph motion lasts longer than its event (motion %f, event %f)",duration_so_far,duration);
    exit(-1);
  }
  add_glyph_pause(duration-duration_so_far,(kate_float)1.0);
}

static void set_motion_mapping(kate_motion *kmotion,kate_motion_mapping x_mapping,kate_motion_mapping y_mapping)
{
  if (!kmotion) {
    yyerror("internal error: set_motion_mapping passed NULL motion");
    exit(-1);
  }

  kmotion->x_mapping=x_mapping;
  kmotion->y_mapping=y_mapping;
}

static void set_motion_semantics(kate_motion *kmotion,kate_motion_semantics semantics)
{
  if (!kmotion) {
    yyerror("internal error: set_motion_semantics passed NULL motion");
    exit(-1);
  }

  if (kmotion->semantics!=(kate_motion_semantics)-1) { yyerror("semantics is already defined"); return; }
  kmotion->semantics=semantics;
}

static kate_motion_semantics kd_get_marker_position_semantics(int n)
{
  switch (n) {
    case 1: return kate_motion_semantics_marker1_position;
    case 2: return kate_motion_semantics_marker2_position;
    case 3: return kate_motion_semantics_marker3_position;
    case 4: return kate_motion_semantics_marker4_position;
    default: yyerrorf("Invalid marker number: %d (only 1-4 are supported)",n); exit(-1);
  }
  return kate_motion_semantics_marker4_position;
}

static kate_motion_semantics kd_get_marker_bitmap_semantics(int n)
{
  switch (n) {
    case 1: return kate_motion_semantics_marker1_bitmap;
    case 2: return kate_motion_semantics_marker2_bitmap;
    case 3: return kate_motion_semantics_marker3_bitmap;
    case 4: return kate_motion_semantics_marker4_bitmap;
    default: yyerrorf("Invalid marker number: %d (only 1-4 are supported)",n); exit(-1);
  }
  return kate_motion_semantics_marker4_bitmap;
}

static kate_motion_semantics kd_get_glyph_pointer_semantics(int n)
{
  switch (n) {
    case 1: return kate_motion_semantics_glyph_pointer_1;
    case 2: return kate_motion_semantics_glyph_pointer_2;
    case 3: return kate_motion_semantics_glyph_pointer_3;
    case 4: return kate_motion_semantics_glyph_pointer_4;
    default: yyerrorf("Invalid glyph pointer number: %d (only 1-4 are supported)",n); exit(-1);
  }
  return kate_motion_semantics_glyph_pointer_4;
}

static kate_motion_semantics kd_get_glyph_pointer_bitmap_semantics(int n)
{
  switch (n) {
    case 1: return kate_motion_semantics_glyph_pointer_1_bitmap;
    case 2: return kate_motion_semantics_glyph_pointer_2_bitmap;
    case 3: return kate_motion_semantics_glyph_pointer_3_bitmap;
    case 4: return kate_motion_semantics_glyph_pointer_4_bitmap;
    default: yyerrorf("Invalid glyph pointer number: %d (only 1-4 are supported)",n); exit(-1);
  }
  return kate_motion_semantics_glyph_pointer_4_bitmap;
}

static void kd_add_event_motion(kate_motion *kmotion)
{
  int ret;

  if (!kmotion) {
    yyerror("internal error: kd_add_event_motion passed NULL motion");
    exit(-1);
  }

  check_motion(kmotion);
  ret=kate_encode_add_motion(&k,kmotion,1);
  if (ret<0) {
    yyerrorf("failed to add motion: %d",ret);
    exit(-1);
  }
  clear_motions();
}

static void kd_add_event_motion_index(size_t idx)
{
  int ret;

  if (idx>=ki.nmotions) { yyerrorf("Motion index %u out of range (%u motions available)",idx,ki.nmotions); exit(-1); }

  ret=kate_encode_add_motion_index(&k,idx);
  if (ret<0) {
    yyerrorf("failed to add motion: %d",ret);
    exit(-1);
  }
  clear_motions();
}

static void init_font_range(void)
{
  krange=(kate_font_range*)kate_malloc(sizeof(kate_font_range));
  if (!krange) { yyerror("out of memory"); exit(-1); }
  krange->first_code_point=-1;
  krange->last_code_point=-1;
  krange->first_bitmap=-1;
}

static void set_font_range_code_point_string(int *cp,const char *s)
{
  size_t len0;
  int ret,c;

  if (!cp) { yyerror("internal error: no code point pointer"); exit(-1); }
  if (!s) { yyerror("internal error: no string"); exit(-1); }
  len0=strlen(s)+1;
  ret=kate_text_get_character(kate_utf8,&s,&len0);
  if (ret<0) {
    yyerrorf("failed to get character from string: %d",ret);
    return;
  }
  c=ret;
  ret=kate_text_get_character(kate_utf8,&s,&len0);
  if (ret<0) {
    yyerrorf("failed to get character from string: %d",ret);
    return;
  }
  if (ret) {
    yyerror("code point string should contain only one character");
    return;
  }
  *cp=c;
}

static void set_font_range_first_code_point_string(const char *s)
{
  if (!s) { yyerror("internal error: no string"); exit(-1); }
  if (!krange) { yyerror("internal error: no font range"); exit(-1); }
  set_font_range_code_point_string(&krange->first_code_point,s);
}

static void set_font_range_last_code_point_string(const char *s)
{
  if (!s) { yyerror("internal error: no string"); exit(-1); }
  if (!krange) { yyerror("internal error: no font range"); exit(-1); }
  set_font_range_code_point_string(&krange->last_code_point,s);
}

static void set_font_range_first_code_point(int idx)
{
  if (!krange) { yyerror("internal error: no font range"); exit(-1); }
  krange->first_code_point=idx;
}

static void set_font_range_last_code_point(int idx)
{
  if (!krange) { yyerror("internal error: no font range"); exit(-1); }
  krange->last_code_point=idx;
}

static void set_font_range_first_bitmap(int idx)
{
  if (!krange) { yyerror("internal error: no font range"); exit(-1); }
  krange->first_bitmap=idx;
}

static void add_font_range(kate_info *ki,const char *name,kate_font_range *kfr)
{
  int ret;
  if (!ki || !kfr) { yyerror("internal error: no kate_info or kate_font_range"); exit(-1); }
  if (!krange) { yyerror("internal error: no font range"); exit(-1); }
  if (krange->first_code_point<0) yyerror("first code point not set");
  if (krange->last_code_point<0) yyerror("last code point not set");
  if (krange->last_code_point<krange->first_code_point) yyerror("last code point cannnot be less than first code point");
  if (krange->first_bitmap<0) yyerror("bitmap index not set");
  ret=kate_info_add_font_range(ki,kfr);
  if (ret<0) {
    yyerrorf("failed to add font range: %d",ret);
  }
  else {
    font_range_names=(char**)kate_realloc(font_range_names,ki->nfont_ranges*sizeof(char*));
    if (!font_range_names) {
      yyerror("Out of memory");
      exit(-1);
    }
    font_range_names[ki->nfont_ranges-1]=name?dupstring(name):NULL;
  }
}

static int find_font_range(const kate_info *ki,const char *name)
{
  return find_item(name,ki->nfont_ranges,font_range_names);
}

static void init_font_mapping(void)
{
  kmapping=(kate_font_mapping*)kate_malloc(sizeof(kate_font_mapping));
  if (!kmapping) { yyerror("out of memory"); exit(-1); }
  kmapping->nranges=0;
  kmapping->ranges=NULL;
}

static int check_font_overlap(const kate_font_range *kfr0,const kate_font_range *kfr1)
{
  if (!kfr0 || !kfr1) return KATE_E_INVALID_PARAMETER;

  if (kfr0->last_code_point<kfr1->first_code_point) return 0;
  if (kfr1->last_code_point<kfr0->first_code_point) return 0;

  return KATE_E_INIT;
}

static int check_font_ranges(const kate_font_mapping *kfm)
{
  size_t n,l;

  if (!kfm) return KATE_E_INVALID_PARAMETER;

  for (n=0;n<kfm->nranges;++n) {
    const kate_font_range *kfr=kfm->ranges[n];
    if (!kfr) return KATE_E_INIT;
    if (kfr->last_code_point<kfr->first_code_point) return KATE_E_INIT;
    for (l=n+1;l<kfm->nranges;++l) {
      int ret=check_font_overlap(kfr,kfm->ranges[l]);
      if (ret<0) return ret;
    }
  }

  return 0;
}

static void add_font_range_to_mapping(void)
{
  int ret;

  if (!krange) { yyerror("internal error: no font range"); exit(-1); }
  if (!kmapping) { yyerror("internal error: no font mapping"); exit(-1); }

  kmapping->ranges=(kate_font_range**)kate_realloc(kmapping->ranges,(kmapping->nranges+1)*sizeof(kate_font_range*));
  if (!kmapping->ranges) {
    yyerror("error: out of memory");
    exit(-1);
  }
  kmapping->ranges[kmapping->nranges]=krange;
  ++kmapping->nranges;

  ret=check_font_ranges(kmapping);
  if (ret<0) yyerror("font mapping ranges overlap");

  krange=NULL;
}

static void add_font_mapping(kate_info *ki,const char *name,kate_font_mapping *kfm)
{
  int ret;
  if (!ki) { yyerror("internal error: no kate_info"); exit(-1); }
  if (!kfm) { yyerror("internal error: no font mapping"); exit(-1); }
  if (kfm->nranges==0) yyerror("font mapping has no ranges");
  ret=kate_info_add_font_mapping(ki,kfm);
  if (ret<0) {
    yyerrorf("failed to add font mapping: %d",ret);
  }
  else {
    font_mapping_names=(char**)kate_realloc(font_mapping_names,ki->nfont_mappings*sizeof(char*));
    if (!font_mapping_names) {
      yyerror("Out of memory");
      exit(-1);
    }
    font_mapping_names[ki->nfont_mappings-1]=name?dupstring(name):NULL;
  }
}

static int find_font_mapping(const kate_info *ki,const char *name)
{
  return find_item(name,ki->nfont_mappings,font_mapping_names);
}

static void kd_write_headers(void)
{
  int ret=write_headers(katedesc_out);
  if (ret<0) {
    yyerrorf("Failed to write headers: %d\n",ret);
    exit(-1);
  }
}

static void kd_encode_text(kate_state *kstate,kd_event *ev)
{
  int ret;
  ogg_packet op;

  if (!ev) { yyerror("internal error: no event"); exit(-1); }
  ret=kate_encode_set_markup_type(kstate,ev->text_markup_type);
  if (ret<0) {
    yyerrorf("failed to set text markup type: %d",ret);
    return;
  }
  update_stream_time(kstate,katedesc_out,kate_duration_granule(kstate->ki,timebase+ev->t0));
  ret=kate_ogg_encode_text(kstate,timebase+ev->t0,timebase+ev->t1,ev->text?ev->text:"",ev->text?strlen(ev->text):0,&op);
  if (ret<0) {
    yyerrorf("failed to encode text %s: %d",ev->text?ev->text:"<none>",ret);
    return;
  }
  ret=send_packet(katedesc_out,&op,kate_duration_granule(kstate->ki,timebase+ev->t0));
  if (ret<0) {
    yyerrorf("failed to send text packet: %d",ret);
  }
}

static void kd_encode_set_language(kate_state *kstate,const char *s)
{
  int ret;
  if (!s) { yyerror("internal error: no language string"); exit(-1); }
  ret=kate_encode_set_language(kstate,s);
  if (ret<0) yyerrorf("failed to set event language override: %d",ret);
}

static uint32_t make_color(uint32_t r,uint32_t g,uint32_t b,uint32_t a)
{
  return (r<<24)|(g<<16)|(b<<8)|a;
}

static uint32_t make_color_alpha(uint32_t c,uint32_t a)
{
  return (c&0xffffff00)|a;
}

static void record_macro_name(const char *name)
{
  size_t len;
  if (!name) { yyerror("internal error: no macro name"); exit(-1); }
  len=strlen(name);
  if (temp_macro_name) kate_free(temp_macro_name);
  temp_macro_name=kate_malloc(len+1);
  if (!temp_macro_name) { yyerror("out of memory"); exit(-1); }
  strcpy(temp_macro_name,name);
}

static void add_temp_macro(const char *body)
{
  if (!body) { yyerror("internal error: no macro body"); exit(-1); }
  if (!temp_macro_name) { yyerror("internal error - unknown macro name"); return; }
  add_macro(temp_macro_name,body);
  kate_free(temp_macro_name);
  temp_macro_name=NULL;
}

static void set_granule_rate(unsigned int numerator,unsigned int denominator)
{
  ki.gps_numerator=numerator;
  ki.gps_denominator=denominator;
}

static void set_granule_shift(unsigned int granule_shift)
{
  if (granule_shift>=64) yyerror("granule shift out of range (0-64)\n");
  ki.granule_shift=granule_shift;
}

static void set_canvas_size(unsigned int width,unsigned int height)
{
  int ret=kate_info_set_original_canvas_size(&ki,width,height);
  if (ret<0) {
    yyerror("failed to set original canvas size");
    exit(-1);
  }
}

static void add_comment(kate_comment *kc,const char *s)
{
  /* check for "ENCODER=," as kateenc now sets it and we don't want cycles
     of decode/encode to duplicate them, and we want the new one to replace
     any existing one */
  int different=0;
  const char *encoder="ENCODER=",*sptr=s;
  while (*encoder) if ((*encoder++|32)!=(*sptr++|32)) {
    different=1;
    break;
  }
  if (different) {
    CHECK_KATE_API_ERROR(kate_comment_add(kc,s));
  }
}

static void cleanup_names(char **names,size_t count)
{
  size_t n;
  if (names) {
    for (n=0;n<count;++n) if (names[n]) kate_free(names[n]);
    kate_free(names);
  }
}

static void cleanup_memory(void)
{
  cleanup_names(style_names,ki.nstyles);
  cleanup_names(region_names,ki.nregions);
  cleanup_names(curve_names,ki.ncurves);
  cleanup_names(motion_names,ki.nmotions);
  cleanup_names(bitmap_names,ki.nbitmaps);
  cleanup_names(palette_names,ki.npalettes);
  cleanup_names(font_range_names,ki.nfont_ranges);
  cleanup_names(font_mapping_names,ki.nfont_mappings);
  cleanup_names(local_bitmap_names,n_local_bitmaps);

  free_macros();
}


%}

%pure_parser

%union {
  int number;
  unsigned int unumber;
  kate_float fp;
  const char *string;
  char *dynstring;
  kate_style *style;
  kate_region *region;
  kate_curve *curve;
  uint32_t color;
}

%token <number> KATE
%token <number> DEFS
%token <number> LANGUAGE COMMENT CATEGORY
%token <number> DEFINE MACRO STYLE REGION CURVE
%token <number> TEXT BACKGROUND COLOR POSITION SIZE DEFAULT METRIC
%token <number> HALIGN VALIGN HLEFT HCENTER HRIGHT VTOP VCENTER VBOTTOM
%token <number> POINTS
%token <number> EVENT STARTS ENDS AT START END TIME DURATION ARROW FROM TO
%token <number> MAPPING NONE FRAME MOTION BEZIER_CUBIC LINEAR CATMULL_ROM
%token <number> BSPLINE STATIC
%token <number> SEMANTICS EXTERNAL INTERNAL ALIGNMENT RG BA FOR ALPHA
%token <number> TIMEBASE MARKER POINTER
%token <number> SIMPLE_TIMED_GLYPH_MARKER
%token <number> SIMPLE_TIMED_GLYPH_STYLE_MORPH
%token <number> GLYPH PAUSE IN MORPH SECONDARY
%token <number> PATH SECTION PERIODIC
%token <number> DIRECTIONALITY L2R_T2B R2L_T2B T2B_R2L T2B_L2R
%token <number> BITMAP PALETTE COLORS
%token <number> FONT RANGE FIRST LAST CODE POINT
%token <number> USER SOURCE PNG DRAW VISIBLE
%token <number> ID BOLD ITALICS UNDERLINE STRIKE JUSTIFY
%token <number> BASE OFFSET GRANULE RATE SHIFT WIDTH HEIGHT CANVAS
%token <number> LEFT TOP RIGHT BOTTOM MARGIN MARGINS
%token <number> HORIZONTAL VERTICAL CLIP PRE MARKUP
%token <number> LOCAL WRAP WORD META

%token <number> NUMBER
%token <unumber> UNUMBER
%token <string> STRING
%token <fp> FLOAT
%token <color> COLORSPEC
%token <string> IDENTIFIER MACRO_BODY

%type <number> kd_kate kd_opt_defs kd_defs kd_def kd_events kd_event
%type <number> kd_curve_points kd_palette_colors
%type <number> kd_bitmap_pixels kd_png_bitmap_pixels
%type <number> kd_style_defs kd_style_def
%type <number> kd_region_defs kd_region_def
%type <number> kd_curve_defs kd_curve_def
%type <number> kd_palette_defs kd_palette_def
%type <number> kd_bitmap_defs kd_bitmap_def
%type <number> kd_font_range_defs kd_font_range_def
%type <number> kd_font_mapping_defs kd_font_mapping_def
%type <string> kd_opt_name
%type <number> kd_style_name_or_index
%type <number> kd_region_name_or_index
%type <number> kd_curve_name_or_index kd_curve_def_name_or_index
%type <number> kd_palette_name_or_index
%type <number> kd_bitmap_name_or_index
%type <number> kd_motion_name_or_index
%type <number> kd_font_range_name_or_index
%type <number> kd_font_mapping_name_or_index
%type <fp> kd_optional_curve_duration
%type <fp> float timespec
%type <number> kd_event_defs kd_event_def
%type <number> kd_motion_defs kd_motion_def
%type <number> kd_motion_mapping kd_motion_semantics
%type <number> kd_curvetype
%type <color> kd_color
%type <number> kd_simple_timed_glyph_marker_defs kd_simple_timed_glyph_marker_def
%type <number> kd_simple_timed_glyph_style_morph_defs kd_simple_timed_glyph_style_morph_def
%type <number> kd_opt_comma
%type <number> bitmap_x_offset bitmap_y_offset
%type <fp> float60
%type <unumber> unumber60
%type <dynstring> strings
%type <number> kd_optional_secondary
%type <number> directionality
%type <number> kd_opt_space_metric
%type <number> kd_wrap_mode
%type <number> kd_meta_byte_stream_def kd_byte_stream

%%

kd_kate: {nlines=1;} KATE '{' kd_opt_defs { kd_write_headers(); } kd_events '}' { cleanup_memory(); }
       ;

kd_opt_defs: DEFS '{' kd_defs '}'
           | {}
           ;

kd_defs: kd_defs kd_def
       | {}
       ;

kd_def: LANGUAGE STRING { CHECK_KATE_API_ERROR(kate_info_set_language(&ki,$2)); }
      | CATEGORY STRING { CHECK_KATE_API_ERROR(kate_info_set_category(&ki,$2)); }
      | DIRECTIONALITY directionality { CHECK_KATE_API_ERROR(kate_info_set_text_directionality(&ki,$2)); }
      | COMMENT STRING { add_comment(&kc,$2); }
      | DEFINE MACRO {set_macro_mode();} IDENTIFIER {record_macro_name($4);} MACRO_BODY
                     { add_temp_macro($6); unset_macro_mode(); }
      | DEFINE STYLE kd_opt_name {init_style(&kstyle);} '{' kd_style_defs '}' { add_style(&ki,$3,&kstyle); }
      | DEFINE REGION kd_opt_name {init_region(&kregion);} '{' kd_region_defs '}' { add_region(&ki,$3,&kregion); }
      | DEFINE CURVE kd_opt_name {init_curve();} '{' kd_curve_defs '}' { add_curve(&ki,$3,kcurve.curve); }
      | DEFINE MOTION kd_opt_name {init_motion();} '{' kd_motion_defs '}' { add_motion(&ki,$3,kmotion); }
      | DEFINE PALETTE kd_opt_name {init_palette();} '{' kd_palette_defs '}' { add_palette(&ki,$3,kpalette.palette); }
      | DEFINE BITMAP kd_opt_name {init_bitmap();} '{' kd_bitmap_defs '}' { add_bitmap(&ki,$3,kbitmap.bitmap); }
      | DEFINE FONT RANGE kd_opt_name {init_font_range();} '{' kd_font_range_defs '}' { add_font_range(&ki,$4,krange); }
      | DEFINE FONT MAPPING kd_opt_name {init_font_mapping();} '{' kd_font_mapping_defs '}' { add_font_mapping(&ki,$4,kmapping); }
      | TIMEBASE timespec { timebase=$2; }
      | GRANULE RATE UNUMBER '/' UNUMBER {set_granule_rate($3,$5);}
      | GRANULE SHIFT UNUMBER {set_granule_shift($3);}
      | CANVAS SIZE UNUMBER UNUMBER {set_canvas_size($3,$4);}
      ;

kd_style_defs: kd_style_defs kd_style_def
             | {}
             ;

kd_style_def: HALIGN float { kstyle.halign=$2; }
            | VALIGN float { kstyle.valign=$2; }
            | HLEFT { kstyle.halign=(kate_float)-1.0; }
            | HCENTER { kstyle.halign=(kate_float)0.0; }
            | HRIGHT { kstyle.halign=(kate_float)1.0; }
            | VTOP { kstyle.valign=(kate_float)-1.0; }
            | VCENTER { kstyle.valign=(kate_float)0.0; }
            | VBOTTOM { kstyle.valign=(kate_float)1.0; }
            | TEXT COLOR kd_color { set_color(&kstyle.text_color,$3); }
            | BACKGROUND COLOR kd_color { set_color(&kstyle.background_color,$3); }
            | DRAW COLOR kd_color { set_color(&kstyle.draw_color,$3); }
            | BOLD { kstyle.bold=1; }
            | ITALICS { kstyle.italics=1; }
            | UNDERLINE { kstyle.underline=1; }
            | STRIKE { kstyle.strike=1; }
            | JUSTIFY { kstyle.justify=1; }
            | WRAP kd_wrap_mode { kstyle.wrap_mode=$2; }
            | FONT STRING { set_font(&kstyle,$2); }
            | FONT SIZE float kd_opt_space_metric { set_font_size(&kstyle,$3,$4); }
            | FONT WIDTH float kd_opt_space_metric { set_font_width(&kstyle,$3,$4); }
            | FONT HEIGHT float kd_opt_space_metric { set_font_height(&kstyle,$3,$4); }
            | LEFT MARGIN float kd_opt_space_metric { set_margin(&kstyle,&kstyle.left_margin,$3,$4); }
            | TOP MARGIN float kd_opt_space_metric { set_margin(&kstyle,&kstyle.top_margin,$3,$4); }
            | RIGHT MARGIN float kd_opt_space_metric { set_margin(&kstyle,&kstyle.right_margin,$3,$4); }
            | BOTTOM MARGIN float kd_opt_space_metric { set_margin(&kstyle,&kstyle.bottom_margin,$3,$4); }
            | MARGINS float kd_opt_space_metric float kd_opt_space_metric float kd_opt_space_metric float kd_opt_space_metric
                  { set_margins(&kstyle,$2,$3,$4,$5,$6,$7,$8,$9); }
            | META STRING '=' STRING { add_meta(&kstyle.meta,$2,$4); }
            | META STRING '=' UNUMBER {init_byte_stream($4);} '{' kd_byte_stream '}' { add_meta_byte_stream(&kstyle.meta,$2); }
            ;

kd_opt_space_metric: '%' { $$=kate_percentage; }
                   | 'm' { $$=kate_millionths; }
                   | { $$=kate_pixel; }
                   ;

kd_region_defs: kd_region_defs kd_region_def
              | {}
              ;

kd_region_def: METRIC {kregion.metric=$1; }
             | POSITION float float { kregion.x=$2;kregion.y=$3; }
             | SIZE float float { kregion.w=$2;kregion.h=$3; }
             | CLIP { kregion.clip=1; }
             | DEFAULT STYLE kd_style_name_or_index { kregion.style=$3; }
             | META STRING '=' STRING { add_meta(&kregion.meta,$2,$4); }
             | META STRING '=' UNUMBER {init_byte_stream($4);} '{' kd_byte_stream '}' { add_meta_byte_stream(&kregion.meta,$2); }
             ;

kd_curve_defs: kd_curve_defs kd_curve_def
              | {}
              ;

kd_curve_def: kd_curvetype { kcurve.curve->type=$1; n_curve_pts=0; }
            | UNUMBER POINTS { init_curve_points($1); } '{' kd_curve_points '}' {
                if ((size_t)n_curve_pts!=kcurve.curve->npts*2) katedesc_error("Wrong number of points in the curve");
              }
            | POINTS { init_open_ended_curve_points(); } '{' kd_curve_points '}'
            ;

kd_palette_defs: kd_palette_defs kd_palette_def
               | {}
               ;

kd_palette_def: UNUMBER COLORS { init_palette_colors($1); } '{' kd_palette_colors '}' {
                  if ((size_t)n_palette_colors!=kpalette.palette->ncolors) {
                    katedesc_error("Wrong number of colors in the palette");
                  }
                }
              | SOURCE STRING { load_palette($2); }
              ;

kd_bitmap_defs: kd_bitmap_defs kd_bitmap_def
              | {}
              ;

kd_bitmap_def: UNUMBER 'x' UNUMBER 'x' UNUMBER { init_bitmap_pixels($1,$3,$5); } '{' kd_bitmap_pixels '}' {
                 if ((size_t)n_bitmap_pixels!=kbitmap.bitmap->width*kbitmap.bitmap->height) {
                   katedesc_error("Wrong number of pixels in the bitmap");
                 }
               }
             | UNUMBER 'x' UNUMBER PNG UNUMBER { init_png_bitmap_pixels($1,$3,$5); } '{' kd_png_bitmap_pixels '}' {
                 if ((size_t)n_bitmap_pixels!=kbitmap.bitmap->size) {
                   katedesc_error("Wrong number of bytes in the PNG bitmap");
                 }
               }
             | SOURCE STRING { load_bitmap($2,0); }
             | DEFAULT PALETTE kd_palette_name_or_index { kbitmap.bitmap->palette=$3; }
             | OFFSET bitmap_x_offset bitmap_y_offset { kbitmap.bitmap->x_offset=$2; kbitmap.bitmap->y_offset=$3; }
             | META STRING '=' STRING { add_meta(&kbitmap.bitmap->meta,$2,$4); }
             | META STRING '=' UNUMBER {init_byte_stream($4);} '{' kd_byte_stream '}' { add_meta_byte_stream(&kbitmap.bitmap->meta,$2); }
             ;

kd_color: UNUMBER UNUMBER UNUMBER { $$=make_color($1,$2,$3,255); }
        | UNUMBER UNUMBER UNUMBER UNUMBER { $$=make_color($1,$2,$3,$4); }
        | COLORSPEC { $$=make_color_alpha($1,255); }
        | COLORSPEC ALPHA UNUMBER { $$=make_color_alpha($1,$3); }
        ;

kd_wrap_mode: NONE { $$=kate_wrap_none; }
            | WORD { $$=kate_wrap_word; }
            ;

kd_curvetype: NONE { $$=kate_curve_none; }
            | STATIC { $$=kate_curve_static; }
            | LINEAR { $$=kate_curve_linear; }
            | CATMULL_ROM { $$=kate_curve_catmull_rom_spline; }
            | BEZIER_CUBIC { $$=kate_curve_bezier_cubic_spline; }
            | BSPLINE { $$=kate_curve_bspline; }
            ;

kd_curve_points: kd_curve_points float {
                   if (open_ended_curve) {
                     add_open_ended_curve_point($2);
                   }
                   else {
                     if ((size_t)n_curve_pts>=kcurve.curve->npts*2) katedesc_error("Too many points in curve");
                     else { kcurve.curve->pts[n_curve_pts++]=$2; }
                   }
                 }
               | {}
               ;

kd_palette_colors: kd_palette_colors '{' kd_color '}' kd_opt_comma {
                     if ((size_t)n_palette_colors>=kpalette.palette->ncolors) katedesc_error("Too many colors in palette");
                     else { set_color(&kpalette.palette->colors[n_palette_colors++],$3); }
                   }
                 | {}
                 ;

kd_bitmap_pixels: kd_bitmap_pixels UNUMBER kd_opt_comma {
                    if ((size_t)n_bitmap_pixels>=kbitmap.bitmap->width*kbitmap.bitmap->height) {
                      katedesc_error("Too many pixels in bitmap");
                    }
                    else {
                      if ($2>(unsigned int)(1<<kbitmap.bitmap->bpp)-1) {
                        katedesc_error("pixels out of range for given bpp");
                      }
                      else {
                        kbitmap.bitmap->pixels[n_bitmap_pixels++]=$2;
                      }
                    }
                  }
                | {}
                ;

kd_png_bitmap_pixels: kd_png_bitmap_pixels UNUMBER kd_opt_comma {
                        if ((size_t)n_bitmap_pixels>=kbitmap.bitmap->size) {
                          katedesc_error("Too many pixels in bitmap");
                        }
                        else {
                          kbitmap.bitmap->pixels[n_bitmap_pixels++]=$2;
                        }
                      }
                    | {}
                    ;

kd_byte_stream: kd_byte_stream UNUMBER kd_opt_comma {
                        if (n_bytes_in_stream>=byte_stream_size) {
                          katedesc_error("Too many bytes in byte stream");
                        }
                        else {
                          byte_stream[n_bytes_in_stream++]=$2;
                        }
                      }
              | {}
              ;

kd_opt_name: STRING { $$=$1; }
           | { $$=NULL; }
           ;

kd_style_name_or_index: STRING { $$=find_style(&ki,$1); }
                      | UNUMBER { if ($1>=ki.nstyles) yyerrorf("Invalid style index (%u/%d)",$1,ki.nstyles); $$=$1; }
                      ;

kd_region_name_or_index: STRING { $$=find_region(&ki,$1); }
                      | UNUMBER { if ($1>=ki.nregions) yyerrorf("Invalid region index (%u/%u)",$1,ki.nregions); $$=$1; }
                      ;

kd_curve_name_or_index: STRING { $$=find_curve(&ki,$1); }
                      | UNUMBER { if ($1>=ki.ncurves) yyerrorf("Invalid curve index (%u/%u)",$1,ki.ncurves); $$=$1; }
                      ;

kd_motion_name_or_index: STRING { $$=find_motion(&ki,$1); }
                       | UNUMBER { if ($1>=ki.nmotions) yyerrorf("Invalid motion index (%u/%u)",$1,ki.nmotions); $$=$1; }
                       ;

kd_palette_name_or_index: STRING { $$=find_palette(&ki,$1); }
                        | UNUMBER { if ($1>=ki.npalettes) yyerrorf("Invalid palette index (%u/%u)",$1,ki.npalettes); $$=$1; }
                        ;

kd_bitmap_name_or_index: STRING { $$=find_bitmap(&ki,$1); }
                       | UNUMBER { if ($1>=ki.nbitmaps) yyerrorf("Invalid bitmap index (%u/%u)",$1,ki.nbitmaps); $$=$1; }
                       ;

kd_font_range_name_or_index: STRING { $$=find_font_range(&ki,$1); }
                           | UNUMBER { if ($1>=ki.nfont_ranges) yyerrorf("Invalid font range index (%u/%u)",$1,ki.nfont_ranges); $$=$1; }
                           ;

kd_font_mapping_name_or_index: STRING { $$=find_font_mapping(&ki,$1); }
                             | UNUMBER { if ($1>=ki.nfont_mappings) yyerrorf("Invalid font mapping index (%u/%u)",$1,ki.nfont_mappings); $$=$1; }
                             ;

kd_curve_def_name_or_index: kd_curve_name_or_index { reference_curve_from($1); }
                          | { init_curve(); } '{' kd_curve_defs '}' {}
                          | kd_curve_name_or_index { init_curve_from($1); } '{' kd_curve_defs '}' {}
                          ;


float: FLOAT { $$=$1; }
     | UNUMBER { $$=(kate_float)$1; }
     | NUMBER { $$=(kate_float)$1; }
     ;

bitmap_x_offset: UNUMBER { $$=$1; }
               | NUMBER { $$=$1; }
               | float '%' { $$=compute_bitmap_x_offset($1); }
               ;

bitmap_y_offset: UNUMBER { $$=$1; }
               | NUMBER { $$=$1; }
               | float '%' { $$=compute_bitmap_y_offset($1); }
               ;

timespec: UNUMBER ':' unumber60 ':' float60 { $$=$1*3600+$3*60+$5; }
        | UNUMBER ':' float60 { $$=$1*60+$3; }
        | float60 { $$=$1; }
        ;

kd_events: kd_events kd_event
         | {}
         ;

kd_event: EVENT { init_event(&kevent); } '{' kd_event_defs '}' {
            check_event(&kevent); kd_encode_text(&k,&kevent); clear_event(&kevent);
          }
        | EVENT { init_event(&kevent); } kd_event_defs {
            check_event(&kevent); kd_encode_text(&k,&kevent); clear_event(&kevent);
          }
        ;

kd_event_defs: kd_event_defs kd_event_def
             | {}
             ;

kd_event_def: ID UNUMBER { kd_encode_set_id(&k,$2); }
            | LANGUAGE STRING { kd_encode_set_language(&k,$2); }
            | DIRECTIONALITY directionality { CHECK_KATE_API_ERROR(kate_encode_set_text_directionality(&k,$2)); }
            | STARTS AT timespec { set_event_t0(&kevent,$3); }
            | ENDS AT timespec { set_event_t1(&kevent,$3); }
            | START TIME timespec { set_event_t0(&kevent,$3); }
            | END TIME timespec { set_event_t1(&kevent,$3); }
            | FROM timespec TO timespec { set_event_t0_t1(&kevent,$2,$4); }
            | DURATION timespec { set_event_duration(&kevent,$2); }
            | FROM timespec FOR timespec { set_event_t0(&kevent,$2); set_event_duration(&kevent,$4); }
            | timespec ARROW timespec { set_event_t0_t1(&kevent,$1,$3); }
            | REGION kd_region_name_or_index { set_event_region_index(&kevent,$2); }
            | REGION kd_region_name_or_index { init_region_from($2); } '{' kd_region_defs '}'
                     { set_event_region(&kevent,&kregion); }
            | REGION { init_region(&kregion); } '{' kd_region_defs '}' { set_event_region(&kevent,&kregion); }
            | kd_optional_secondary STYLE kd_style_name_or_index
                    { if ($1) set_event_secondary_style_index(&kevent,$3); else set_event_style_index(&kevent,$3); }
            | kd_optional_secondary STYLE kd_style_name_or_index {init_style_from($3); } '{' kd_style_defs '}'
                    { if ($1) set_event_secondary_style(&kevent,&kstyle); else set_event_style(&kevent,&kstyle); }
            | kd_optional_secondary STYLE { init_style(&kstyle); } '{' kd_style_defs '}'
                    { if ($1) set_event_secondary_style(&kevent,&kstyle); else set_event_style(&kevent,&kstyle); }
            | TEXT strings { set_event_text(&kevent,$2,0,0); kate_free($2); }
            | PRE TEXT strings { set_event_text(&kevent,$3,1,0); kate_free($3); }
            | MARKUP strings { set_event_text(&kevent,$2,0,1); kate_free($2); }
            | PRE MARKUP strings { set_event_text(&kevent,$3,1,1); kate_free($3); }
            | TEXT SOURCE STRING { set_event_text_from(&kevent,$3,0,0); }
            | PRE TEXT SOURCE STRING { set_event_text_from(&kevent,$4,1,0); }
            | MARKUP SOURCE STRING { set_event_text_from(&kevent,$3,0,1); }
            | PRE MARKUP SOURCE STRING { set_event_text_from(&kevent,$4,1,1); }
            | strings { set_event_text(&kevent,$1,0,0); kate_free($1); }
            | MOTION { init_motion(); } '{' kd_motion_defs '}' { kd_add_event_motion(kmotion); }
            | MOTION kd_motion_name_or_index { kd_add_event_motion_index($2); }
            | SIMPLE_TIMED_GLYPH_MARKER {init_simple_glyph_pointer_motion(); } '{' kd_simple_timed_glyph_marker_defs '}'
                    { kd_finalize_simple_timed_glyph_motion(kmotion); kd_add_event_motion(kmotion); }
            | SIMPLE_TIMED_GLYPH_STYLE_MORPH {init_simple_glyph_pointer_motion(); } '{' kd_simple_timed_glyph_style_morph_defs '}'
                    { kd_finalize_simple_timed_glyph_motion(kmotion); kd_add_event_motion(kmotion); }
            | FONT MAPPING kd_font_mapping_name_or_index { CHECK_KATE_API_ERROR(kate_encode_set_font_mapping_index(&k,$3)); }
            | PALETTE kd_palette_name_or_index { set_event_palette_index(&kevent,$2); }
            | PALETTE { init_palette(); } '{' kd_palette_defs '}' { set_event_palette(&kevent,kpalette.palette); }
            | BITMAP kd_bitmap_name_or_index { set_event_bitmap_index(&kevent,$2); }
            | BITMAP { init_bitmap(); } '{' kd_bitmap_defs '}' { set_event_bitmap(&kevent,kbitmap.bitmap); }
            | DEFINE LOCAL BITMAP kd_opt_name {init_bitmap();} '{' kd_bitmap_defs '}' { add_local_bitmap(&k,$4,kbitmap.bitmap); }
            | DEFINE LOCAL BITMAP kd_opt_name '=' kd_bitmap_name_or_index { add_local_bitmap_index(&k,$4,$6); }
            | META STRING '=' STRING { kd_add_event_meta($2,$4); }
            | META STRING '=' kd_meta_byte_stream_def { kd_add_event_meta_byte_stream($2); }
            ;

kd_meta_byte_stream_def: UNUMBER {init_byte_stream($1);} '{' kd_byte_stream '}' { $$=0; }
                       ;

kd_optional_secondary: SECONDARY { $$=1; }
                     | { $$=0; }
                     ;

strings: strings '+' STRING { $$=catstrings($1,$3); }
       | STRING { $$=catstrings(NULL,$1); }
       ;

kd_motion_defs: kd_motion_defs kd_motion_def
              | {}
              ;

kd_motion_def: MAPPING kd_motion_mapping { set_motion_mapping(kmotion,$2,$2); }
             | MAPPING kd_motion_mapping kd_motion_mapping { set_motion_mapping(kmotion,$2,$3); }
             | SEMANTICS kd_motion_semantics { set_motion_semantics(kmotion,$2); }
             | CURVE kd_curve_def_name_or_index kd_optional_curve_duration { add_curve_to_motion(kmotion,$3); }
             | PERIODIC { kmotion->periodic=1; }
             ;

kd_optional_curve_duration: FOR float { $$=$2; }
                          | FOR float '%' { $$=-$2/(kate_float)100.0; }
                          | FOR float 'm' { $$=-$2/(kate_float)1000000.0; }
                          | { $$=(kate_float)-1.0; }
                          ;

kd_motion_mapping: NONE { $$=kate_motion_mapping_none; }
                 | FRAME { $$=kate_motion_mapping_frame; }
                 | REGION { $$=kate_motion_mapping_region; }
                 | EVENT DURATION { $$=kate_motion_mapping_event_duration; }
                 | BITMAP SIZE { $$=kate_motion_mapping_bitmap_size; }
                 | USER UNUMBER {
                     if ($2<kate_motion_mapping_user) yyerrorf("invalid value for user motion mapping (%u), should be 128 or more",$2);
                     $$=(kate_motion_mapping)$2;
                   }
                 ;

kd_motion_semantics: TIME { $$=kate_motion_semantics_time; }
                   | REGION POSITION { $$=kate_motion_semantics_region_position; }
                   | REGION SIZE { $$=kate_motion_semantics_region_size; }
                   | TEXT ALIGNMENT { $$=kate_motion_semantics_text_alignment_int; }
                   | INTERNAL TEXT ALIGNMENT { $$=kate_motion_semantics_text_alignment_int; }
                   | EXTERNAL TEXT ALIGNMENT { $$=kate_motion_semantics_text_alignment_ext; }
                   | TEXT POSITION { $$=kate_motion_semantics_text_position; }
                   | TEXT SIZE { $$=kate_motion_semantics_text_size; }
                   | MARKER UNUMBER POSITION { $$=kd_get_marker_position_semantics($2); }
                   | GLYPH POINTER UNUMBER { $$=kd_get_glyph_pointer_semantics($3); }
                   | TEXT COLOR RG { $$=kate_motion_semantics_text_color_rg; }
                   | TEXT COLOR BA { $$=kate_motion_semantics_text_color_ba; }
                   | BACKGROUND COLOR RG { $$=kate_motion_semantics_background_color_rg; }
                   | BACKGROUND COLOR BA { $$=kate_motion_semantics_background_color_ba; }
                   | DRAW COLOR RG { $$=kate_motion_semantics_draw_color_rg; }
                   | DRAW COLOR BA { $$=kate_motion_semantics_draw_color_ba; }
                   | STYLE MORPH { $$=kate_motion_semantics_style_morph; }
                   | TEXT PATH { $$=kate_motion_semantics_text_path; }
                   | TEXT PATH SECTION { $$=kate_motion_semantics_text_path_section; }
                   | DRAW { $$=kate_motion_semantics_draw; }
                   | VISIBLE SECTION { $$=kate_motion_semantics_text_visible_section; }
                   | 'z' { $$=kate_motion_semantics_z; }
                   | HORIZONTAL MARGINS { $$=kate_motion_semantics_horizontal_margins; }
                   | VERTICAL MARGINS { $$=kate_motion_semantics_vertical_margins; }
                   | BITMAP POSITION { $$=kate_motion_semantics_bitmap_position; }
                   | BITMAP SIZE { $$=kate_motion_semantics_bitmap_size; }
                   | MARKER UNUMBER BITMAP { $$=kd_get_marker_bitmap_semantics($2); }
                   | GLYPH POINTER UNUMBER BITMAP { $$=kd_get_glyph_pointer_bitmap_semantics($3); }
                   | DRAW WIDTH { $$=kate_motion_semantics_draw_width; }
                   | USER UNUMBER {
                       if ($2<kate_motion_semantics_user) yyerrorf("invalid value for user motion semantics (%u), should be 128 or more",$2);
                       $$=(kate_motion_semantics)$2;
                     }
                   ;

kd_simple_timed_glyph_marker_defs: kd_simple_timed_glyph_marker_defs kd_simple_timed_glyph_marker_def
                                 | {}
                                 ;

kd_simple_timed_glyph_marker_def: GLYPH POINTER UNUMBER { kmotion->semantics=get_glyph_pointer_offset($3); }
                                | 'Y' MAPPING kd_motion_mapping { kmotion->y_mapping=$3; }
                                | HEIGHT FROM float TO float { karaoke_base_height=$3; karaoke_top_height=$5; }
                                | PAUSE FOR timespec { add_glyph_pause($3,(kate_float)0.0); }
                                | GLYPH UNUMBER IN timespec { add_glyph_transition($2,$4,(kate_float)0.0,(kate_float)1.0,0,(kate_float)0.0); }
                                | GLYPH UNUMBER AT timespec { add_glyph_transition($2,$4,(kate_float)0.0,(kate_float)1.0,1,(kate_float)0.0); }
                                ;

kd_simple_timed_glyph_style_morph_defs: kd_simple_timed_glyph_style_morph_defs kd_simple_timed_glyph_style_morph_def
                                      | {}
                                      ;

kd_simple_timed_glyph_style_morph_def: GLYPH POINTER UNUMBER { kmotion->semantics=get_glyph_pointer_offset($3); }
                                     | FROM STYLE kd_style_name_or_index TO STYLE kd_style_name_or_index
                                              { set_style_morph(&kevent,$3,$6); }
                                     | PAUSE FOR timespec { add_glyph_pause($3,(kate_float)0.0); }
                                     | GLYPH UNUMBER IN timespec { add_glyph_transition($2,$4,(kate_float)0.0,(kate_float)0.0,0,(kate_float)1.0); }
                                     | GLYPH UNUMBER AT timespec { add_glyph_transition($2,$4,(kate_float)0.0,(kate_float)0.0,1,(kate_float)1.0); }
                                     | STRING IN timespec { add_glyph_transition_to_text($1,$3,(kate_float)0.0,(kate_float)0.0,0,(kate_float)1.0); }
                                     | STRING AT timespec { add_glyph_transition_to_text($1,$3,(kate_float)0.0,(kate_float)0.0,1,(kate_float)1.0); }
                                     ;

unumber60: UNUMBER { if ($1>59) yyerrorf("Value must be between 0 and 59, but is %u",$1); } { $$=$1; }
         ;

float60: float { if ($1<(kate_float)0.0 || $1>=(kate_float)60.0) yyerrorf("Value must be between 0 (inclusive) and 60 (exclusive), but is %f",$1); } { $$=$1; }
       ;

kd_opt_comma: ',' {}
            | {}
            ;

directionality: L2R_T2B { $$=kate_l2r_t2b; }
              | R2L_T2B { $$=kate_r2l_t2b; }
              | T2B_R2L { $$=kate_t2b_r2l; }
              | T2B_L2R { $$=kate_t2b_l2r; }
              ;

kd_font_range_defs: kd_font_range_defs kd_font_range_def
                  | {}
                  ;

kd_font_range_def: FIRST CODE POINT STRING { set_font_range_first_code_point_string($4); }
                 | FIRST CODE POINT UNUMBER { set_font_range_first_code_point($4); }
                 | LAST CODE POINT STRING { set_font_range_last_code_point_string($4); }
                 | LAST CODE POINT UNUMBER { set_font_range_last_code_point($4); }
                 | FIRST BITMAP kd_bitmap_name_or_index { set_font_range_first_bitmap($3); }
                 ;

kd_font_mapping_defs: kd_font_mapping_defs kd_font_mapping_def
                    | {}
                    ;

kd_font_mapping_def: RANGE {init_font_range();} '{' kd_font_range_defs '}' { add_font_range_to_mapping(); }
                   | RANGE kd_font_range_name_or_index { krange=ki.font_ranges[$2]; add_font_range_to_mapping(); }
                   ;

%%

