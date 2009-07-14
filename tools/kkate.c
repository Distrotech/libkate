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
#include "kutil.h"
#include "kstrings.h"
#include "kkate.h"

#ifdef DEBUG
int write_bitmaps=0;
#endif

void write_kate_start(FILE *f)
{
  fprintf(f,"kate {\n");
}

void write_kate_end(FILE *f)
{
  fprintf(f,"}\n");
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
      const char *escape_list="";
      switch (text_markup_type) {
        case kate_markup_none: escape_list="\"\r\n`'|\\"; break;
        case kate_markup_simple: escape_list="\"\r\n`'|\\"; break;
        default: fprintf(stderr,"Unknown text markup type (%d)\n",text_markup_type); break;
      }
      if (ret>=32 && (ret>0xff || !strchr(escape_list,ret))) {
        char utf8[12],*utf8ptr=utf8;
        size_t wlen0=sizeof(utf8);
        ret=kate_text_set_character(kate_utf8,ret,&utf8ptr,&wlen0);
        if (ret<0) {
          fprintf(stderr,"Error writing character\n");
          break;
        }
        ret=kate_text_set_character(kate_utf8,0,&utf8ptr,&wlen0);
        if (ret<0) {
          fprintf(stderr,"Error writing character\n");
          break;
        }
        fprintf(f,"%s",utf8);
      }
      else {
        fprintf(f,"&#%u;",ret);
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

static int is_suitable_string(const char *value,size_t len)
{
  const char *p;
  int ret;
  size_t n;
  static const char *list="\"\r\n";

  ret=kate_text_validate(kate_utf8,value,len);
  if (ret<0) return 0;
  if (len>0) for (n=0;n<len-1;++n) if (!value[n]) return 0;
  for (p=list;*p;++p) if (memchr(value,*p,len)) return 0;
  return 1;
}

static void write_metadata(FILE *f,const kate_meta *km,size_t indent)
{
  char *sindent=(char*)kate_malloc(1+indent);
  size_t count,n,b;
  int ret;

  for (n=0;n<indent;++n) sindent[n]=' ';
  sindent[indent]=0;

  ret=kate_meta_query_count(km);
  if (ret<0) {
    fprintf(stderr,"Error retrieving medata: %d\n",ret);
  }
  else {
    count=ret;
    for (n=0;n<count;++n) {
      const char *tag,*value;
      size_t len;
      ret=kate_meta_query(km,n,&tag,&value,&len);
      if (ret<0) {
        fprintf(stderr,"Error retrieving medata: %d\n",ret);
      }
      else {
        fprintf(f,"%smeta \"%s\"=",sindent,tag);
        if (is_suitable_string(value,len)) {
          fprintf(f,"\"%s\"",value);
        }
        else {
          fprintf(f," %lu {\n",(unsigned long)len);
          for (b=0;b<len;++b) {
            if (b%16==0) fprintf(f,"%s  ",sindent);
            fprintf(f," 0x%02x",value[b]);
            if (b%16==15 || b==len-1) fprintf(f,"\n");
          }
          fprintf(f,"%s}",sindent);
        }
        fprintf(f,"\n");
      }
    }
  }

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
    fprintf(f,"%swrap %s\n",sindent,wrap2text(ks->wrap_mode));
    if (ks->meta) write_metadata(f,ks->meta,indent);
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
  if (kr->meta) write_metadata(f,kr->meta,indent);

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
    fprintf(f,"%s%lu points {\n",sindent,(unsigned long)kc->npts);
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
  if (km->meta) write_metadata(f,km->meta,indent);

  kate_free(sindent);
}

static void write_palette_defs(FILE *f,const kate_palette *kp,size_t indent)
{
  size_t s;
  char *sindent=(char*)kate_malloc(1+indent);
  size_t n;
  for (n=0;n<indent;++n) sindent[n]=' ';
  sindent[indent]=0;

  fprintf(f,"%s%lu colors {\n",sindent,(unsigned long)kp->ncolors);
  for (s=0;s<kp->ncolors;++s) {
    const kate_color *kc=kp->colors+s;
    fprintf(f,"%s  { %d %d %d %d },\n",sindent,kc->r,kc->g,kc->b,kc->a);
  }
  fprintf(f,"%s}\n",sindent);
  if (kp->meta) write_metadata(f,kp->meta,indent);

  kate_free(sindent);
}

#ifdef DEBUG

static void write_paletted_bitmap(const char *filename,const kate_bitmap *kb,const kate_palette *kp)
{
  size_t n,x,y;
  FILE *f;
  
  f=fopen(filename,"w");
  if (!f) {
    fprintf(stderr,"Failed to open %s: %s\n",filename,strerror(errno));
    return;
  }

  fprintf(f,"P6\n%lu %lu\n255\n",(unsigned long)kb->width,(unsigned long)kb->height);
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

static void write_png_bitmap(const char *filename,const kate_bitmap *kb)
{
  FILE *f;

  f=fopen(filename,"w");
  if (!f) {
    fprintf(stderr,"Failed to open %s: %s\n",filename,strerror(errno));
    return;
  }
  fwrite(kb->pixels,kb->size,1,f);
  fclose(f);
}

#endif

static void write_bitmap_defs(FILE *f,const kate_bitmap *kb,size_t indent)
{
  size_t w,h,p;
  char *sindent=(char*)kate_malloc(1+indent);
  size_t n;
  for (n=0;n<indent;++n) sindent[n]=' ';
  sindent[indent]=0;

  switch (kb->type) {
    case kate_bitmap_type_png:
      fprintf(f,"%s%lux%lu png %lu {\n",sindent,(unsigned long)kb->width,(unsigned long)kb->height,(unsigned long)kb->size);
      for (p=0;p<kb->size;++p) {
        if (p%16==0) fprintf(f,"%s",sindent);
        fprintf(f," 0x%02x",kb->pixels[p]);
        if (p%16==15 || p==kb->size-1) fprintf(f,"\n");
      }
      fprintf(f,"%s}\n",sindent);
      break;
    case kate_bitmap_type_paletted:
      fprintf(f,"%s%lux%lux%d {\n",sindent,(unsigned long)kb->width,(unsigned long)kb->height,kb->bpp);
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
  if (kb->x_offset || kb->y_offset) {
    fprintf(f,"%soffset %d %d\n",sindent,kb->x_offset,kb->y_offset);
  }
  if (kb->palette>=0) fprintf(f,"%sdefault palette %d\n",sindent,kb->palette);
  if (kb->meta) write_metadata(f,kb->meta,indent);

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
    fprintf(f,"%scanvas size %lu %lu\n",sindent,(unsigned long)ki->original_canvas_width,(unsigned long)ki->original_canvas_height);
  }

  kate_free(sindent);
}

void write_kate_headers(FILE *f,const kate_info *ki,const kate_comment *kc)
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

void write_kate_event(FILE *fout,void *data,const kate_event *ev,ogg_int64_t granpos)
{
  const kate_info *ki=ev->ki;
  float t0=ev->start_time;
  float t1=ev->end_time;

  (void)data;
  fprintf(fout,"  event {\n");
  if (ev->id>=0) {
    fprintf(fout,"    id %d\n",ev->id);
  }
  if (granpos>=0) {
    kate_float base,offset;
    kate_granule_split_time(ki,granpos,&base,&offset);
#ifdef DEBUG
    fprintf(fout,"    # granule %llx composition: base %02d:%02d:%02.8g, offset %02d:%02d:%02.8g\n",
      (long long)granpos,
      time_hours(base),time_minutes(base),time_float_seconds(base),
      time_hours(offset),time_minutes(offset),time_float_seconds(offset)
    );
#endif
  }
  fprintf(fout,"    %02d:%02d:%02.8g --> %02d:%02d:%02.8g\n",
    time_hours(t0),time_minutes(t0),time_float_seconds(t0),
    time_hours(t1),time_minutes(t1),time_float_seconds(t1)
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

  if (ev->nbitmaps>0) {
    size_t n;
    for (n=0;n<ev->nbitmaps;++n) {
      const kate_bitmap *kb=ev->bitmaps[n];
      int idx=kate_find_bitmap(ki,kb);
      if (idx<0) {
        fprintf(fout,"    define local bitmap {\n");
        write_bitmap_defs(fout,kb,6);
        fprintf(fout,"    }\n");
      }
      else {
        fprintf(fout,"    define local bitmap = %d\n",idx);
      }
    }
  }

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
#ifdef DEBUG
  if (write_bitmaps && ev->bitmap) {
    static int n=0;
    static char filename[32];
    switch (ev->bitmap->type) {
      case kate_bitmap_type_paletted:
        if (ev->bitmap->bpp>0 && ev->palette) {
          snprintf(filename,sizeof(filename),"/tmp/kate-bitmap-%d",n++);
          filename[sizeof(filename)-1]=0;
          write_paletted_bitmap(filename,ev->bitmap,ev->palette);
        }
        break;
      case kate_bitmap_type_png:
        snprintf(filename,sizeof(filename),"/tmp/kate-bitmap-%d",n++);
        filename[sizeof(filename)-1]=0;
        write_png_bitmap(filename,ev->bitmap);
        break;
    }
  }
#endif
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
  if (ev->meta) write_metadata(fout,ev->meta,4);
  fprintf(fout,"  }\n");
  fprintf(fout,"\n");
}

