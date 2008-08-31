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
#if defined WIN32 || defined _WIN32 || defined MSDOS || defined __CYGWIN__ || defined __EMX__ || defined OS2
#include <io.h>
#include <fcntl.h>
#endif
#if defined WIN32 || defined _WIN32
#include <process.h>
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

enum { uninitialized, header_info, header_comment, data };

typedef struct {
  ogg_stream_state os;
  kate_state k;
  kate_info ki;
  kate_comment kc;
  int init;
  char *filename;
  FILE *fout;
  unsigned int stream_index;
  int event_index;
} kate_stream;

static int write_bitmaps=0;

static void fcats(char **ptr,const char *s,size_t s_len)
{
  if (s && *s) {
    size_t ptr_len=(*ptr)?strlen(*ptr):0;
    *ptr=(char*)kate_realloc(*ptr,ptr_len+s_len+1);
    memcpy((*ptr)+ptr_len,s,s_len);
    (*ptr)[ptr_len+s_len]=0;
  }
}

static void fcat(char **ptr,const char *s)
{
  if (s && *s) {
    fcats(ptr,s,strlen(s));
  }
}

static int is_filename_used(const char *filename,const kate_stream *streams,size_t nstreams)
{
  size_t n;
  for (n=0;n<nstreams;++n) {
    const kate_stream *ks=streams+n;
    if (ks->filename && !strcmp(filename,ks->filename)) return 1;
  }
  return 0;
}

static char *get_filename(const char *basename,const kate_stream *ks,const kate_stream *streams,size_t nstreams)
{
  char tmp[32];
  char *filename=NULL;
  const char *ptr=basename;

  if (!basename) return NULL;
  while (*ptr) {
    const char *percent=strchr(ptr,'%');
    if (!percent) {
      fcat(&filename,ptr);
      break;
    }
    if (percent>ptr) {
      fcats(&filename,ptr,percent-ptr);
    }
    ptr=percent+1;
    if (!*ptr) {
      fprintf(stderr,"absent format tag in filename\n");
      exit(-1);
    }
    switch (*ptr) {
      case '%':
        fcats(&filename,"%",1);
        break;
      case 'c':
        fcat(&filename,ks->ki.category);
        break;
      case 'l':
        fcat(&filename,ks->ki.language);
        break;
      case 's':
        snprintf(tmp,sizeof(tmp),"%08x",(ogg_uint32_t)ks->os.serialno);
        fcat(&filename,tmp);
        break;
      case 'i':
        snprintf(tmp,sizeof(tmp),"%d",ks->stream_index);
        fcat(&filename,tmp);
        break;
      default:
        fprintf(stderr,"unknown format tag in filename: %c\n",*ptr);
        exit(-1);
        break;
    }
    ++ptr;
  }

  if (is_filename_used(filename,streams,nstreams)) {
    kate_free(filename);
    return NULL;
  }

  return filename;
}

static void write_kate_start(FILE *f)
{
  fprintf(f,"kate {\n");
}

static void write_kate_end(FILE *f)
{
  fprintf(f,"}\n");
}

static const char *halign2text(kate_float d)
{
  static char tmp[32];
  if (d==0.0) return "hcenter";
  if (d==1.0) return "hright";
  if (d==-1.0) return "hleft";
  snprintf(tmp,sizeof(tmp),"halign %.16g",d);
  tmp[sizeof(tmp)-1]=0;
  return tmp;
}

static const char *valign2text(kate_float d)
{
  static char tmp[32];
  if (d==0.0) return "vcenter";
  if (d==-1.0) return "vtop";
  if (d==1.0) return "vbottom";
  snprintf(tmp,sizeof(tmp),"valign %.16g",d);
  tmp[sizeof(tmp)-1]=0;
  return tmp;
}

static const char *metric2text(kate_space_metric d)
{
  switch (d) {
    case kate_pixel: return "pixel";
    case kate_percentage: return "percent";
    case kate_millionths: return "millionths";
    default: return "invalid";
  }
}

static const char *metric2suffix(kate_space_metric d)
{
  switch (d) {
    case kate_pixel: return "";
    case kate_percentage: return "%";
    case kate_millionths: return "m";
    default: return "invalid";
  }
}

static const char *curve2text(kate_curve_type d)
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

static const char *semantics2text(kate_motion_semantics d)
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
    default: snprintf(tmp,sizeof(tmp),"user %u",d); return tmp;
  }
  return "invalid";
}

static const char *mapping2text(kate_motion_mapping d)
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

static const char *directionality2text(kate_text_directionality d)
{
  switch (d) {
    case kate_l2r_t2b: return "l2r_t2b";
    case kate_r2l_t2b: return "r2l_t2b";
    case kate_t2b_r2l: return "t2b_r2l";
    case kate_t2b_l2r: return "t2b_l2r";
  }
  return "invalid";
}

static void write_text(FILE *f,const char *text,size_t len0,kate_markup_type text_markup_type)
{
  while (1) {
    int ret=kate_text_get_character(kate_utf8,&text,&len0);
    if (ret<0) {
      fprintf(stderr,"Error getting character from text\n");
      break;
    }
    if (ret==0) {
      break;
    }
    else {
      /* be conservative in what we encode */
      const char *escape="";
      switch (text_markup_type) {
        case kate_markup_none: escape="\"\r\n`'|\\"; break;
        case kate_markup_simple: escape="\"\r\n`'|\\"; break;
        default: fprintf(stderr,"Unknown text markup type (%d)\n",text_markup_type); break;
      }
      if (ret>=32 && ret<127 && !strchr(escape,ret)) {
        fprintf(f,"%c",ret);
      }
      else {
        fprintf(f,"&#%x;",ret);
      }
    }
  }
}

static void write_color(FILE *f,const char *name,const kate_color *kc,size_t indent)
{
  char *sindent=(char*)kate_malloc(1+indent);
  size_t n;
  for (n=0;n<indent;++n) sindent[n]=' ';
  sindent[indent]=0;

  fprintf(f,"%s%s %d %d %d %d\n",sindent,name,kc->r,kc->g,kc->b,kc->a);

  kate_free(sindent);
}

static void write_style_defs(FILE *f,const kate_style *ks,size_t indent)
{
  char *sindent=(char*)kate_malloc(1+indent);
  size_t n;
  for (n=0;n<indent;++n) sindent[n]=' ';
  sindent[indent]=0;

  {
    const char *halign=halign2text(ks->halign);
    const char *valign=valign2text(ks->valign);
    kate_color tc=ks->text_color,bc=ks->background_color,dc=ks->draw_color;
    fprintf(
      f,
      "%s%s %s\n",
      sindent,halign,valign
    );
    write_color(f,"text color",&tc,indent);
    write_color(f,"background color",&bc,indent);
    write_color(f,"draw color",&dc,indent);
    if (ks->font) {
      fprintf(f,"%sfont \"%s\"\n",sindent,ks->font);
    }
    if (ks->font_width>=0 || ks->font_height>=0) {
      if (ks->font_width==ks->font_height) {
        fprintf(f,"%sfont size %f%s\n",sindent,ks->font_height,metric2suffix(ks->font_metric));
      }
      else {
        if (ks->font_width>=0)
          fprintf(f,"%sfont width %f%s\n",sindent,ks->font_width,metric2suffix(ks->font_metric));
        if (ks->font_height>=0)
        fprintf(f,"%sfont height %f%s\n",sindent,ks->font_height,metric2suffix(ks->font_metric));
      }
    }
    if (ks->left_margin!=0 || ks->top_margin!=0 || ks->right_margin!=0 || ks->bottom_margin!=0) {
      const char *margin_metric=metric2suffix(ks->margin_metric);
      fprintf(
        f,"%smargins %f%s %f%s %f%s %f%s\n",
        sindent,
        ks->left_margin,margin_metric,ks->top_margin,margin_metric,
        ks->right_margin,margin_metric,ks->bottom_margin,margin_metric
      );
    }
    if (ks->bold) fprintf(f,"%sbold\n",sindent);
    if (ks->italics) fprintf(f,"%sitalics\n",sindent);
    if (ks->underline) fprintf(f,"%sunderline\n",sindent);
    if (ks->strike) fprintf(f,"%sstrike\n",sindent);
    if (ks->justify) fprintf(f,"%sjustify\n",sindent);
  }

  kate_free(sindent);
}

static void write_region_defs(FILE *f,const kate_region *kr,size_t indent)
{
  char *sindent=(char*)kate_malloc(1+indent);
  size_t n;
  for (n=0;n<indent;++n) sindent[n]=' ';
  sindent[indent]=0;

  fprintf(f,"%s%s position %d %d size %d %d\n",sindent,metric2text(kr->metric),kr->x,kr->y,kr->w,kr->h);
  if (kr->style>=0) fprintf(f,"%sdefault style %d\n",sindent,kr->style);
  if (kr->clip) fprintf(f,"%sclip\n",sindent);

  kate_free(sindent);
}

static void write_curve_defs(FILE *f,const kate_curve *kc,size_t indent)
{
  char *sindent=(char*)kate_malloc(1+indent);
  size_t n;
  for (n=0;n<indent;++n) sindent[n]=' ';
  sindent[indent]=0;

  fprintf(f,"%s%s\n",sindent,curve2text(kc->type));
  if (kc->npts>0) {
    size_t pt;
    fprintf(f,"%s%zu points {\n",sindent,kc->npts);
    for (pt=0;pt<kc->npts;++pt) fprintf(f,"%s  %.16g %.16g\n",sindent,kc->pts[pt*2],kc->pts[pt*2+1]);
    fprintf(f,"%s}\n",sindent);
  }

  kate_free(sindent);
}

static void write_motion_defs(FILE *f,const kate_info *ki,const kate_motion *km,size_t indent)
{
  size_t s;
  char *sindent=(char*)kate_malloc(1+indent);
  size_t n;
  for (n=0;n<indent;++n) sindent[n]=' ';
  sindent[indent]=0;

  fprintf(f,"%ssemantics %s\n",sindent,semantics2text(km->semantics));
  if (km->periodic) fprintf(f,"%speriodic\n",sindent);
  if (km->x_mapping!=kate_motion_mapping_none || km->y_mapping!=kate_motion_mapping_none) {
    if (km->x_mapping==km->y_mapping) {
      fprintf(f,"%smapping %s\n",sindent,mapping2text(km->x_mapping));
    }
    else {
      /* two calls as mapping2text can return a static buffer */
      fprintf(f,"%smapping %s",sindent,mapping2text(km->x_mapping));
      fprintf(f," %s\n",mapping2text(km->y_mapping));
    }
  }
  for (s=0;s<km->ncurves;++s) {
    const kate_curve *kc=km->curves[s];
    int idx;
    idx=kate_find_curve(ki,kc);
    if (idx<0) {
      fprintf(f,"%scurve {\n",sindent);
      write_curve_defs(f,kc,indent+2);
      fprintf(f,"%s}",sindent);
    }
    else {
      fprintf(f,"%scurve %d",sindent,idx);
    }
    if (km->durations[s]!=-1) {
      fprintf(f," for %.16g\n",km->durations[s]);
    }
    else {
      fprintf(f,"\n");
    }
  }

  kate_free(sindent);
}

static void write_palette_defs(FILE *f,const kate_palette *kp,size_t indent)
{
  size_t s;
  char *sindent=(char*)kate_malloc(1+indent);
  size_t n;
  for (n=0;n<indent;++n) sindent[n]=' ';
  sindent[indent]=0;

  fprintf(f,"%s%zu colors {\n",sindent,kp->ncolors);
  for (s=0;s<kp->ncolors;++s) {
    const kate_color *kc=kp->colors+s;
    fprintf(f,"%s  { %d %d %d %d },\n",sindent,kc->r,kc->g,kc->b,kc->a);
  }
  fprintf(f,"%s}\n",sindent);

  kate_free(sindent);
}

static void write_bitmap(const char *filename,const kate_bitmap *kb,const kate_palette *kp)
{
  size_t n,x,y;
  FILE *f;
  
  f=fopen(filename,"w");
  if (!f) {
    fprintf(stderr,"Failed to open %s: %s\n",filename,strerror(errno));
    return;
  }

  fprintf(f,"P6\n%zu %zu\n255\n",kb->width,kb->height);
  n=0;
  for (y=0;y<kb->height;++y) {
    for (x=0;x<kb->width;++x) {
      int pix=kb->pixels[n++];
      const kate_color *kc=kp->colors+pix;
      fprintf(f,"%c%c%c",kc->r,kc->g,kc->b);
    }
  }

  fclose(f);
}

static void write_bitmap_defs(FILE *f,const kate_bitmap *kb,size_t indent)
{
  size_t w,h,p;
  char *sindent=(char*)kate_malloc(1+indent);
  size_t n;
  for (n=0;n<indent;++n) sindent[n]=' ';
  sindent[indent]=0;

  switch (kb->type) {
    case kate_bitmap_type_png:
      fprintf(f,"%s%zux%zu png %zu {\n",sindent,kb->width,kb->height,kb->size);
      for (p=0;p<kb->size;++p) {
        if (p%16==0) fprintf(f,"%s",sindent);
        fprintf(f," %3d",kb->pixels[p]);
        if (p%16==15) fprintf(f,"\n");
      }
      fprintf(f,"%s}\n",sindent);
      break;
    case kate_bitmap_type_paletted:
      fprintf(f,"%s%zux%zux%d {\n",sindent,kb->width,kb->height,kb->bpp);
      p=0;
      for (h=0;h<kb->height;++h) {
        fprintf(f,"%s ",sindent);
        for (w=0;w<kb->width;++w) {
          fprintf(f," %3d",kb->pixels[p++]);
        }
        fprintf(f,"\n");
      }
      fprintf(f,"%s}\n",sindent);
      break;
    default:
      fprintf(stderr,"Error: unknown bitmap type: %d\n",kb->type);
      break;
  }
  if (kb->palette>=0) fprintf(f,"%sdefault palette %d\n",sindent,kb->palette);

  kate_free(sindent);
}

static void write_font_range_defs(FILE *f,const kate_font_range *kfr,size_t indent)
{
  char *sindent=(char*)kate_malloc(1+indent);
  size_t n;
  for (n=0;n<indent;++n) sindent[n]=' ';
  sindent[indent]=0;

  fprintf(f,"%sfirst code point 0x%x\n",sindent,kfr->first_code_point);
  fprintf(f,"%slast code point 0x%x\n",sindent,kfr->last_code_point);
  fprintf(f,"%sfirst bitmap %d\n",sindent,kfr->first_bitmap);

  kate_free(sindent);
}

static void write_font_mapping_defs(FILE *f,const kate_info *ki,const kate_font_mapping *kfm,size_t indent)
{
  char *sindent=(char*)kate_malloc(1+indent);
  size_t n;
  for (n=0;n<indent;++n) sindent[n]=' ';
  sindent[indent]=0;

  for (n=0;n<kfm->nranges;++n) {
    const kate_font_range *kfr=kfm->ranges[n];
    int idx=kate_find_font_range(ki,kfr);
    if (idx>=0) {
      fprintf(f,"%srange %d\n",sindent,idx);
    }
    else {
      fprintf(f,"%srange {\n",sindent);
      write_font_range_defs(f,kfr,indent+2);
      fprintf(f,"%s}\n",sindent);
    }
  }

  kate_free(sindent);
}

static void write_granule_encoding(FILE *f,const kate_info *ki,size_t indent)
{
  char *sindent=(char*)kate_malloc(1+indent);
  size_t n;
  for (n=0;n<indent;++n) sindent[n]=' ';
  sindent[indent]=0;

  if (ki->gps_numerator!=1000 || ki->gps_denominator!=1) {
    fprintf(f,"%sgranule rate %u/%u\n",sindent,ki->gps_numerator,ki->gps_denominator);
  }
  if (ki->granule_shift!=32) fprintf(f,"%sgranule shift %u\n",sindent,ki->granule_shift);

  kate_free(sindent);
}

static void write_canvas_size(FILE *f,const kate_info *ki,size_t indent)
{
  char *sindent=(char*)kate_malloc(1+indent);
  size_t n;
  for (n=0;n<indent;++n) sindent[n]=' ';
  sindent[indent]=0;

  if (ki->original_canvas_width>0 || ki->original_canvas_height>0) {
    fprintf(f,"%scanvas size %zu %zu\n",sindent,ki->original_canvas_width,ki->original_canvas_height);
  }

  kate_free(sindent);
}

static void write_kate_headers(FILE *f,const kate_info *ki,const kate_comment *kc)
{
  size_t n;
  int c;

  fprintf(f,"\n");
  fprintf(f,"  defs {\n");
  write_granule_encoding(f,ki,4);
  fprintf(f,"    category \"%s\"\n",ki->category);
  fprintf(f,"    language \"%s\"\n",ki->language);
  fprintf(f,"    directionality %s\n",directionality2text(ki->text_directionality));
  write_canvas_size(f,ki,4);

  if (kc && kc->comments) {
    fprintf(f,"\n");
    for (c=0;c<kc->comments;++c) {
      fprintf(f,"    comment   \"%s\"\n",kc->user_comments[c]);
    }
  }

  if (ki->nstyles) {
    fprintf(f,"\n");
    for (n=0;n<ki->nstyles;++n) {
      const kate_style *ks=ki->styles[n];
      fprintf(f,"    define style {\n");
      write_style_defs(f,ks,6);
      fprintf(f,"    }\n");
    }
  }

  if (ki->nregions) {
    fprintf(f,"\n");
    for (n=0;n<ki->nregions;++n) {
      const kate_region *kr=ki->regions[n];
      fprintf(f,"    define region {\n");
      write_region_defs(f,kr,6);
      fprintf(f,"    }\n");
    }
  }

  if (ki->ncurves) {
    fprintf(f,"\n");
    for (n=0;n<ki->ncurves;++n) {
      const kate_curve *kc=ki->curves[n];
      fprintf(f,"    define curve {\n");
      write_curve_defs(f,kc,6);
      fprintf(f,"    }\n");
    }
  }

  if (ki->nmotions) {
    fprintf(f,"\n");
    for (n=0;n<ki->nmotions;++n) {
      const kate_motion *km=ki->motions[n];
      fprintf(f,"    define motion {\n");
      write_motion_defs(f,ki,km,6);
      fprintf(f,"    }\n");
    }
  }

  if (ki->npalettes) {
    fprintf(f,"\n");
    for (n=0;n<ki->npalettes;++n) {
      const kate_palette *kp=ki->palettes[n];
      fprintf(f,"    define palette {\n");
      write_palette_defs(f,kp,6);
      fprintf(f,"    }\n");
    }
  }

  if (ki->nbitmaps) {
    fprintf(f,"\n");
    for (n=0;n<ki->nbitmaps;++n) {
      const kate_bitmap *kb=ki->bitmaps[n];
      fprintf(f,"    define bitmap {\n");
      write_bitmap_defs(f,kb,6);
      fprintf(f,"    }\n");
    }
  }

  if (ki->nfont_ranges) {
    fprintf(f,"\n");
    for (n=0;n<ki->nfont_ranges;++n) {
      const kate_font_range *kfr=ki->font_ranges[n];
      fprintf(f,"    define font range {\n");
      write_font_range_defs(f,kfr,6);
      fprintf(f,"    }\n");
    }
  }

  if (ki->nfont_mappings) {
    fprintf(f,"\n");
    for (n=0;n<ki->nfont_mappings;++n) {
      const kate_font_mapping *kfm=ki->font_mappings[n];
      fprintf(f,"    define font mapping {\n");
      write_font_mapping_defs(f,ki,kfm,6);
      fprintf(f,"    }\n");
    }
  }

  fprintf(f,"  }\n");
  fprintf(f,"\n");
}

static int time_hours(kate_float t)
{
  return (int)(t/3600);
}

static int time_minutes(kate_float t)
{
  int h=(int)(t/3600);
  t-=h*3600;
  return t/60;
}

static kate_float time_seconds(kate_float t)
{
  int h,m;

  h=(int)(t/3600);
  t-=h*3600;
  m=(int)(t/60);
  t-=m*60;
  return t;
}

static const char *eat_arg(int argc,char **argv,int *n)
{
  if (*n==argc-1) {
    fprintf(stderr,"%s needs an argument\n",argv[*n]);
    exit(-1);
  }
  return argv[++*n];
}

static void print_version(void)
{
  printf("Kate reference decoder - %s\n",kate_get_version_string());
}

static void output_kate_event(FILE *fout,const kate_event *ev,ogg_int64_t granpos,int event_index)
{
  const kate_info *ki=ev->ki;
  float t0=ev->start_time;
  float t1=ev->end_time;

  (void)event_index;
  fprintf(fout,"  event {\n");
  if (ev->id>=0) {
    fprintf(fout,"    id %d\n",ev->id);
  }
  if (granpos>=0) {
    kate_float base,offset;
    kate_granule_split_time(ki,granpos,&base,&offset);
    fprintf(fout,"    # granule %llx composition: base %02d:%02d:%02.8g, offset %02d:%02d:%02.8g\n",
      (long long)granpos,
      time_hours(base),time_minutes(base),time_seconds(base),
      time_hours(offset),time_minutes(offset),time_seconds(offset)
    );
  }
  fprintf(fout,"    %02d:%02d:%02.8g --> %02d:%02d:%02.8g\n",
    time_hours(t0),time_minutes(t0),time_seconds(t0),
    time_hours(t1),time_minutes(t1),time_seconds(t1)
  );
  if (ev->language) {
    fprintf(fout,"    language \"%s\"\n",ev->language);
  }
  if (ev->text_directionality!=ki->text_directionality) {
    fprintf(fout,"    directionality %s\n",directionality2text(ev->text_directionality));
  }
  fprintf(fout,"    pre ");
  switch (ev->text_markup_type) {
    default: /* default to a sensible default */
    case kate_markup_none: fprintf(fout,"text"); break;
    case kate_markup_simple: fprintf(fout,"markup"); break;
  }
  fprintf(fout," \"");
  write_text(fout,ev->text,ev->len0,ev->text_markup_type);
  fprintf(fout,"\"\n");
  if (ev->region) {
    int idx=kate_find_region(ki,ev->region);
    if (idx<0) {
      fprintf(fout,"    region {\n");
      write_region_defs(fout,ev->region,6);
      fprintf(fout,"    }\n");
    }
    else {
      fprintf(fout,"    region %d\n",idx);
    }
  }
  if (ev->style) {
    int idx=kate_find_style(ki,ev->style);
    if (idx<0) {
      fprintf(fout,"    style {\n");
      write_style_defs(fout,ev->style,6);
      fprintf(fout,"    }\n");
    }
    else if (ev->region && idx!=ev->region->style) {
      /* don't mention it if it's the region style, we don't want an override here */
      fprintf(fout,"    style %d\n",idx);
    }
  }
  if (ev->secondary_style) {
    int idx=kate_find_style(ki,ev->secondary_style);
    if (idx<0) {
      fprintf(fout,"    secondary style {\n");
      write_style_defs(fout,ev->secondary_style,6);
      fprintf(fout,"    }\n");
    }
    else {
      fprintf(fout,"    secondary style %d\n",idx);
    }
  }
  if (ev->font_mapping) {
    int idx=kate_find_font_mapping(ki,ev->font_mapping);
    if (idx>=0) {
      fprintf(fout,"    font mapping %d\n",idx);
    }
    else {
      fprintf(fout,"    font mapping {\n");
      write_font_mapping_defs(fout,ki,ev->font_mapping,6);
      fprintf(fout,"    }\n");
    }
  }
  if (ev->palette) {
    int idx=kate_find_palette(ki,ev->palette);
    if (idx<0) {
      fprintf(fout,"    palette {\n");
      write_palette_defs(fout,ev->palette,6);
      fprintf(fout,"    }\n");
    }
    else {
      fprintf(fout,"    palette %d\n",idx);
    }
  }
  if (ev->bitmap) {
    int idx=kate_find_bitmap(ki,ev->bitmap);
    if (idx<0) {
      fprintf(fout,"    bitmap {\n");
      write_bitmap_defs(fout,ev->bitmap,6);
      fprintf(fout,"    }\n");
    }
    else {
      fprintf(fout,"    bitmap %d\n",idx);
    }
  }
  if (write_bitmaps && ev->bitmap && ev->bitmap->bpp>0 && ev->palette) {
    static int n=0;
    static char filename[32];
    snprintf(filename,sizeof(filename),"/tmp/kate-bitmap-%d",n++);
    filename[sizeof(filename)-1]=0;
    write_bitmap(filename,ev->bitmap,ev->palette);
  }
  if (ev->nmotions) {
    size_t m;
    for (m=0;m<ev->nmotions;++m) {
      const kate_motion *km=ev->motions[m];
      int idx=kate_find_motion(ki,km);
      if (idx<0) {
        fprintf(fout,"    motion {\n");
        write_motion_defs(fout,ki,km,6);
        fprintf(fout,"    }\n");
      }
      else {
        fprintf(fout,"    motion %d\n",idx);
      }
    }
  }
  fprintf(fout,"  }\n");
  fprintf(fout,"\n");
}

static void output_srt_event(FILE *fout,const kate_event *ev,ogg_int64_t granpos,int event_index)
{
  float t0=ev->start_time;
  float t1=ev->end_time;

  (void)granpos;
  fprintf(fout,"%d\n",event_index+1);
  fprintf(fout,"%02d:%02d:%#02.4g --> %02d:%02d:%#02.4g\n",
    time_hours(t0),time_minutes(t0),time_seconds(t0),
    time_hours(t1),time_minutes(t1),time_seconds(t1)
  );
  fprintf(fout,"%s\n",ev->text);
  fprintf(fout,"\n");
}

static int read_raw_packet(FILE *f,char **buffer,ogg_int64_t bytes)
{
  size_t ret;

  *buffer=(char*)kate_realloc(*buffer,bytes);
  if (!*buffer) return -1;

  ret=fread(*buffer,1,bytes,f);
  if (ret<(size_t)bytes) return -1;
  return 0;
}

static unsigned long gen_fuzz(unsigned long *seed)
{
  return *seed=*seed*65521+0x9e370001UL;
}

static unsigned long gen_fuzz_n(unsigned long *seed,unsigned long n)
{
  return gen_fuzz(seed)%n;
}

static void fuzz_packet(unsigned long *seed,unsigned long nbytes,unsigned char *data)
{
  unsigned long n=gen_fuzz_n(seed,4),i;
  if (n!=0) return;
  n=gen_fuzz_n(seed,8);
  for (i=0;i<n;++i) {
    unsigned long offset=gen_fuzz_n(seed,nbytes);
    data[offset]^=1;
  }
}

static void fuzz_kate_packet(unsigned long *seed,kate_packet *kp)
{
  fuzz_packet(seed,kp->nbytes,kp->data);
}

static void fuzz_ogg_packet(unsigned long *seed,ogg_packet *op)
{
  fuzz_packet(seed,op->bytes,op->packet);
}

int main(int argc,char **argv)
{
  size_t bytes_read;
  int ret=-1;
  int eos=0;
  const char *input_filename=NULL;
  const char *output_filename=NULL;
  const char *output_filename_type=NULL;
  FILE *fin;
  int verbose=0;
  int arg;
  char signature[64]; /* matches the size of the Kate ID header */
  size_t signature_size;
  int raw;
  char *buffer=NULL;
  ogg_int64_t bytes;
  int headers_written=0;
  int fuzz=0;
  unsigned long fuzz_seed=0;
  size_t n;
  size_t n_kate_streams=0;
  unsigned int n_streams=0;
  kate_stream *kate_streams=NULL;
  ogg_sync_state oy;
  ogg_page og;
  ogg_packet op;
  void (*write_start_function)(FILE*);
  void (*write_end_function)(FILE*);
  void (*write_headers_function)(FILE*,const kate_info*,const kate_comment*);
  void (*write_event_function)(FILE*,const kate_event*,ogg_int64_t,int);

  for (arg=1;arg<argc;++arg) {
    if (argv[arg][0]=='-') {
      switch (argv[arg][1]) {
        case 'V':
          print_version();
          exit(0);
        case 'h':
          print_version();
          printf("usage: %s [options] [filename]\n",argv[0]);
          printf("   -V                  version\n");
          printf("   -v                  verbose\n");
          printf("   -h                  help\n");
          printf("   -o <filename>       set output filename\n");
          printf("   -t <type>           set output format (kate (default), srt\n");
          printf("   -B                  write some bitmaps in /tmp (debug)\n");
          printf("   -f <number>         fuzz testing with given seed (debug)\n");
          exit(0);
        case 'o':
          if (!output_filename) {
            output_filename=eat_arg(argc,argv,&arg);
          }
          else {
            fprintf(stderr,"Only one output filename may be converted at a time\n");
            exit(-1);
          }
          break;
        case 'v':
          ++verbose;
          break;
        case 'B':
          write_bitmaps=1;
          break;
        case 'f':
          fuzz=1;
          fuzz_seed=strtoul(eat_arg(argc,argv,&arg),NULL,10);
          break;
        case 't':
          if (!output_filename_type) {
            output_filename_type=eat_arg(argc,argv,&arg);
          }
          else {
            fprintf(stderr,"Only one output type may be given\n");
            exit(-1);
          }
          break;
        default:
          fprintf(stderr,"Invalid option: %s\n",argv[arg]);
          exit(-1);
      }
    }
    else {
      if (!input_filename) {
        input_filename=argv[arg];
      }
      else {
        fprintf(stderr,"Only one input filename may be converted at a time\n");
        exit(-1);
      }
    }
  }

  if (!output_filename_type || !strcmp(output_filename_type,"kate")) {
    write_start_function=&write_kate_start;
    write_headers_function=&write_kate_headers;
    write_end_function=&write_kate_end;
    write_event_function=&output_kate_event;
  }
  else if (!strcmp(output_filename_type,"srt")) {
    write_start_function=NULL;
    write_headers_function=NULL;
    write_end_function=NULL;
    write_event_function=&output_srt_event;
  }
  else {
    fprintf(stderr,"Invalid format type: %s\n",output_filename_type);
    exit(-1);
  }

  if (!input_filename || !strcmp(input_filename,"-")) {
#if defined WIN32 || defined _WIN32
    _setmode(_fileno(stdin),_O_BINARY);
#else
#if defined MSDOS || defined __CYGWIN__ || defined __EMX__ || defined OS2 || defined __BORLANDC__
    setmode(fileno(stdin),_O_BINARY);
#endif
#endif
    fin=stdin;
  }
  else {
    fin=fopen(input_filename,"rb");
    if (!fin) {
      fprintf(stderr,"%s: %s\n",input_filename,strerror(errno));
      exit(-1);
    }
  }

  /* first, read the first few bytes to know if we have a raw Kate stream
     or a Kate-in-Ogg stream */
  bytes_read=fread(signature,1,sizeof(signature),fin);
  if (bytes_read!=sizeof(signature)) {
    /* A Kate stream's first packet is 64 bytes, so this cannot be one */
    fprintf(stderr,"Failed to read first %zu bytes of stream\n",sizeof(signature));
    exit(-1);
  }

  if (!memcmp(signature,"\200kate\0\0\0",8)) {
    /* raw Kate stream */
    kate_state k;
    FILE *fout;
    int event_index=0;

    if (!output_filename || !strcmp(output_filename,"-")) {
      fout=stdout;
    }
    else {
      char *filename=get_filename(output_filename,0,kate_streams,n_kate_streams);
      fout=fopen(filename,"w");
      if (!fout) {
        fprintf(stderr,"%s: %s\n",filename,strerror(errno));
        exit(-1);
      }
      kate_free(filename);
    }

    raw=1;

    ret=kate_high_decode_init(&k);
    if (ret<0) {
      fprintf(stderr,"failed to init raw kate packet decoding (%d)\n",ret);
      exit(-1);
    }

    bytes=64;
    buffer=(char*)kate_malloc(bytes);
    memcpy(buffer,signature,bytes);

    while (1) {
      const kate_event *ev=NULL;
      kate_packet kp;
      kate_packet_wrap(&kp,bytes,buffer);
      if (fuzz) fuzz_kate_packet(&fuzz_seed,&kp);
      ret=kate_high_decode_packetin(&k,&kp,&ev);
      if (ret<0) {
        fprintf(stderr,"failed to decode raw kate packet (%d)\n",ret);
        exit(-1);
      }
      if (k.ki->probe<0 && !headers_written) {
        if (write_start_function) (*write_start_function)(fout);
        if (write_headers_function) (*write_headers_function)(fout,k.ki,NULL);
        headers_written=1;
      }
      if (ret>0) {
        if (write_end_function) (*write_end_function)(fout);
        break; /* last packet decoded */
      }
      if (ev) {
        if (write_event_function) (*write_event_function)(fout,ev,-1,event_index);
        ++event_index;
      }

      /* all subsequent packets are prefixed with 64 bits (signed) of the packet length in bytes */
      bytes_read=fread(&bytes,1,8,fin);
      if (bytes_read!=8 || bytes<=0) {
        fprintf(stderr,"failed to read raw kate packet size (read %zu, bytes %lld)\n",bytes_read,(long long)bytes);
        exit(-1);
      }
      ret=read_raw_packet(fin,&buffer,bytes);
      if (ret<0) {
        fprintf(stderr,"failed to read raw kate packet (%lld bytes)\n",(long long)bytes);
        exit(-1);
      }
    }

    kate_high_decode_clear(&k);
    kate_free(buffer);

    if (fout!=stdout) fclose(fout);
  }
  else {
    /* we'll assume we're embedded in Ogg */
    raw=0;
    signature_size=bytes_read;
    ogg_sync_init(&oy);

    while (1) {
      buffer=ogg_sync_buffer(&oy,4096);
      if (!buffer) {
        fprintf(stderr,"Failed to get sync buffer\n");
        break;
      }
      if (signature_size>0) {
        memcpy(buffer,signature,signature_size);
        signature_size=0;
      }
      else {
        bytes_read=fread(buffer,1,4096,fin);
      }
      if (bytes_read==0) {
        eos=1;
        break;
      }
      ogg_sync_wrote(&oy,bytes_read);

      while (ogg_sync_pageout(&oy,&og)>0) {
        if (ogg_page_bos(&og)) do {
          kate_stream *ks;
          kate_streams=(kate_stream*)kate_realloc(kate_streams,(n_kate_streams+1)*sizeof(kate_stream));
          ks=&kate_streams[n_kate_streams];
          ks->filename=NULL;
          ks->fout=NULL;
          ogg_stream_init(&ks->os,ogg_page_serialno(&og));
          ret=kate_info_init(&ks->ki);
          if (ret<0) {
            fprintf(stderr,"failed to init info\n");
            ogg_stream_clear(&ks->os);
            break;
          }
          kate_info_no_limits(&ks->ki,1);
          ret=kate_comment_init(&ks->kc);
          if (ret<0) {
            fprintf(stderr,"failed to init comments\n");
            ogg_stream_clear(&ks->os);
            break;
          }
          ks->init=header_info;
          ks->event_index=0;
          ks->stream_index=n_streams++;
          ++n_kate_streams;
        } while(0);
        for (n=0;n<n_kate_streams;++n) {
          kate_stream *ks=kate_streams+n;
          ret=ogg_stream_pagein(&ks->os,&og);
          if (ret>=0) {
            ogg_int64_t granpos=ogg_page_granulepos(&og);
            while (ogg_stream_packetout(&ks->os,&op)) {
              if (verbose>=2) printf("Got packet: %ld bytes\n",op.bytes);
              if (ks->init<data) {
                ret=kate_ogg_decode_headerin(&ks->ki,&ks->kc,&op);
                if (ret>=0) {
                  /* we found a Kate bitstream */
                  if (op.packetno==0) {
                    if (verbose>=1) printf("Bitstream %08x looks like Kate\n",ogg_page_serialno(&og));
                  }
                  if (ret>0) {
                    /* we're done parsing headers, go for data */
                    if (verbose>=1) {
                      printf("Bitstream %08x is Kate (\"%s\" \"%s\", encoding %d)\n",
                        ogg_page_serialno(&og),
                        ks->ki.language,
                        ks->ki.category,
                        ks->ki.text_encoding);
                    }
                    if (!output_filename || !strcmp(output_filename,"-")) {
                      size_t n;
                      int stdout_already_used=0;
                      for (n=0;n<n_kate_streams;++n) {
                        if (kate_streams[n].fout==stdout) {
                          stdout_already_used=1;
                          break;
                        }
                      }
                      if (stdout_already_used) {
                        fprintf(stderr,"Cannot write two Kate streams to the same output, new Kate stream will be ignored\n");
                        break;
                      }
                      ks->fout=stdout;
                    }
                    else {
                      ks->filename=get_filename(output_filename,ks,kate_streams,n_kate_streams);
                      if (!ks->filename) {
                        /* get_filename returns NULL when it can't generate a filename because no format field
                           was given in the output filename and we have more than one Kate stream */
                        fprintf(stderr,"Cannot write two Kate streams to the same output, new Kate stream will be ignored\n");
                        break;
                      }
                      ks->fout=fopen(ks->filename,"w");
                      if (!ks->fout) {
                        fprintf(stderr,"%s: %s\n",ks->filename,strerror(errno));
                        exit(-1);
                      }
                    }
                    if (write_start_function) (*write_start_function)(ks->fout);

                    kate_decode_init(&ks->k,&ks->ki);
                    kate_streams[n].init=data;
                    if (write_headers_function) (*write_headers_function)(ks->fout,&ks->ki,&ks->kc);
                  }
                }
                else {
                  if (ret!=KATE_E_NOT_KATE) {
                    fprintf(stderr,"kate_decode_headerin: packetno %lld: %d\n",(long long)op.packetno,ret);
                  }
                  ogg_stream_clear(&ks->os);
                  kate_info_clear(&ks->ki);
                  kate_comment_clear(&ks->kc);
                  if (n!=n_kate_streams-1) {
                    memmove(kate_streams+n,kate_streams+n+1,(n_kate_streams-1-n)*sizeof(kate_stream));
                  }
                  --n_kate_streams;
                }
              }
              else {
                if (fuzz) fuzz_ogg_packet(&fuzz_seed,&op);
                ret=kate_ogg_decode_packetin(&ks->k,&op);
                if (ret<0) {
                  fprintf(stderr,"error in kate_decode_packetin: %d\n",ret);
                }
                else if (ret>0) {
                  /* we're done */
                  if (write_end_function) (*write_end_function)(ks->fout);
                  eos=1;
                  break;
                }
                else {
                  const kate_event *ev=NULL;
                  ret=kate_decode_eventout(&ks->k,&ev);
                  if (ret<0) {
                    fprintf(stderr,"error in kate_decode_eventout: %d\n",ret);
                  }
                  else if (ret>0) {
                    /* printf("No event to go with this packet\n"); */
                  }
                  else if (ret==0) {
                    if (write_event_function) (*write_event_function)(ks->fout,ev,granpos,ks->event_index);
                    ++ks->event_index;
                  }
                }
              }
            }
            break;
          }
        }
      }

      //if (eos) break;
    }

    for (n=0;n<n_kate_streams;++n) {
      ogg_stream_clear(&kate_streams[n].os);
    }
    ogg_sync_clear(&oy);

    for (n=0;n<n_kate_streams;++n) {
      if (kate_streams[n].init==data) {
        kate_clear(&kate_streams[n].k);
      }
      kate_info_clear(&kate_streams[n].ki);
      kate_comment_clear(&kate_streams[n].kc);
      if (kate_streams[n].fout!=stdout) {
        fclose(kate_streams[n].fout);
        if (ret<0) unlink(kate_streams[n].filename);
        kate_free(kate_streams[n].filename);
      }
    }
    kate_free(kate_streams);
  }

  if (fin!=stdin) fclose(fin);

  return 0;
}
