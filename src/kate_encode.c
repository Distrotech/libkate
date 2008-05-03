/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#define KATE_INTERNAL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kate/kate.h>
#include "kate_internal.h"
#include "kate_encode_state.h"
#include "kate_fp.h"

#define NUM_HEADERS 9

/**
  \ingroup encoding
  Initializes a kate_state structure for encoding using the supplied kate_info structure.
  When done, the kate_state structure should be cleared using kate_clear.
  \param k the kate_state structure to initialize for encoding
  \param ki the kate_info structure containing the encoding parameters
  \returns 0 success
  \return KATE_E_* error
  */
int kate_encode_init(kate_state *k,kate_info *ki)
{
  if (!k || !ki) return KATE_E_INVALID_PARAMETER;

  k->ki=ki;
  k->kds=NULL;
  k->ki->num_headers=NUM_HEADERS;
  k->kes=kate_encode_state_create();
  if (!k->kes) return KATE_E_OUT_OF_MEMORY;

  return 0;
}

static void kate_writebuf(oggpack_buffer *opb,const char *s,int len)
{
  while (len--) oggpack_write(opb,*s++,8);
}

static void kate_write32(oggpack_buffer *opb,kate_int32_t v)
{
  oggpack_write(opb,v&0xff,8);
  v>>=8;
  oggpack_write(opb,v&0xff,8);
  v>>=8;
  oggpack_write(opb,v&0xff,8);
  v>>=8;
  oggpack_write(opb,v&0xff,8);
}

static void kate_write32v(oggpack_buffer *opb,kate_int32_t v)
{
  if (v>=0 && v<=14) {
    oggpack_write(opb,v,4);
  }
  else {
    int bits=0;
    kate_int32_t tmp;
    oggpack_write(opb,15,4);
    if (v&0x80000000) {
      oggpack_write(opb,1,1);
      v=-v;
    }
    else {
      oggpack_write(opb,0,1);
    }
    tmp=v;
    while (tmp) {
      ++bits;
      tmp>>=1;
    }
    if (bits==0) bits=1;
    oggpack_write(opb,bits-1,5);
    oggpack_write(opb,v,bits);
  }
}

static void kate_write64(oggpack_buffer *opb,kate_int64_t v)
{
  kate_write32(opb,(kate_int32_t)v);
  v>>=32;
  kate_write32(opb,(kate_int32_t)v);
}

static void kate_open_warp(oggpack_buffer *warp)
{
  oggpack_writeinit(warp);
}

static void kate_close_warp(oggpack_buffer *warp,oggpack_buffer *opb)
{
  int bits=oggpack_bits(warp);
  unsigned char *buffer=oggpack_get_buffer(warp);
  kate_write32v(opb,bits);
  while (bits>0) {
    oggpack_writecopy(opb,buffer,bits>32?32:bits);
    buffer+=32/8;
    bits-=32;
  }
  oggpack_writeclear(warp);
}

static void kate_warp(oggpack_buffer *opb)
{
  oggpack_buffer warp;
  kate_open_warp(&warp);
  kate_close_warp(&warp,opb);
}

static int kate_finalize_packet_buffer(oggpack_buffer *opb,kate_packet *op,kate_state *k)
{
  if (!opb || !op || !k) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  /* fill up any remaining bits in the last byte with zeros */
  oggpack_writealign(opb);

  op->nbytes=oggpack_bytes(opb);
  op->data=kate_malloc(op->nbytes);
  if (!op->data) return KATE_E_OUT_OF_MEMORY;

  memcpy(op->data,oggpack_get_buffer(opb),op->nbytes);

  /* reset the buffer so we're ready for next packet */
  oggpack_writeclear(opb);
  oggpack_writeinit(opb);

  ++k->kes->packetno;

  /* clear any overrides */
  kate_encode_state_clear_overrides(k->kes,k->ki);

  return 0;
}

static int kate_encode_start_header(oggpack_buffer *opb,int headerid)
{
  if (!opb || !(headerid&0x80)) return KATE_E_INVALID_PARAMETER;

  oggpack_write(opb,headerid,8);
  kate_writebuf(opb,"kate\0\0\0\0",8);

  return 0;
}

static int kate_encode_info(kate_state *k,kate_packet *op)
{
  oggpack_buffer *opb;
  kate_info *ki;
  size_t len;
  int ret;

  if (!k || !op) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  opb=&k->kes->opb;

  ret=kate_encode_start_header(opb,0x80);
  if (ret<0) return ret;

  ki=k->ki;
  oggpack_write(opb,KATE_BITSTREAM_VERSION_MAJOR,8);
  oggpack_write(opb,KATE_BITSTREAM_VERSION_MINOR,8);
  oggpack_write(opb,ki->num_headers,8);
  oggpack_write(opb,ki->text_encoding,8);
  oggpack_write(opb,ki->text_directionality,8);
  oggpack_write(opb,0,8); /* reserved - 0 */
  oggpack_write(opb,kate_granule_shift(k->ki),8);
  kate_write32(opb,0); /* reserved - 0 */
  kate_write32(opb,0); /* reserved - 0 */
  kate_write32(opb,ki->gps_numerator);
  kate_write32(opb,ki->gps_denominator);

  /* language is a 15+1 character max null terminated string */
  if (ki->language) {
    len=strlen(ki->language);
    if (len>15) return KATE_E_LIMIT;
    kate_writebuf(opb,ki->language,len);
  }
  else len=0;
  while (len++<16) oggpack_write(opb,0,8);

  /* category is a 15+1 character max null terminated string */
  if (ki->category) {
    len=strlen(ki->category);
    if (len>15) return KATE_E_LIMIT;
    kate_writebuf(opb,ki->category,len);
  }
  else len=0;
  while (len++<16) oggpack_write(opb,0,8);

  return kate_finalize_packet_buffer(opb,op,k);
}

static int kate_encode_comment(kate_state *k,kate_comment *kc,kate_packet *op)
{
  oggpack_buffer *opb;
  const char *vendor;
  int vendor_len;
  int ret;

  if (!k || !kc || !op) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  opb=&k->kes->opb;

  ret=kate_encode_start_header(opb,0x81);
  if (ret<0) return ret;

  vendor=kate_get_version_string();
  if (!vendor) return KATE_E_INIT; /* wtf ??? */
  vendor_len=strlen(vendor);

  /* mostly copied from theora encoder_toplevel.c */
  kate_write32(opb,vendor_len);
  kate_writebuf(opb,vendor,vendor_len);

  if (kc->comments<0) return KATE_E_INIT;
  kate_write32(opb,kc->comments);
  if (kc->comments) {
    int i;
    for(i=0;i<kc->comments;++i) {
      if (kc->user_comments[i]) {
        if (kc->comment_lengths[i]<0) return KATE_E_INIT;
        kate_write32(opb,kc->comment_lengths[i]);
        ret=kate_text_validate(kate_utf8,kc->user_comments[i],kc->comment_lengths[i]);
        if (ret<0) return ret;
        kate_writebuf(opb,kc->user_comments[i],kc->comment_lengths[i]);
      }
      else {
        kate_write32(opb,0);
      }
    }
  }

  return kate_finalize_packet_buffer(opb,op,k);
}

static int kate_encode_region(const kate_region *kr,oggpack_buffer *opb)
{
  if (!kr || !opb) return KATE_E_INVALID_PARAMETER;

  oggpack_write(opb,kr->metric,8);
  kate_write32v(opb,kr->x);
  kate_write32v(opb,kr->y);
  kate_write32v(opb,kr->w);
  kate_write32v(opb,kr->h);
  kate_write32v(opb,kr->style);
  kate_warp(opb);

  return 0;
}

static int kate_encode_regions(kate_state *k,kate_packet *op)
{
  oggpack_buffer *opb;
  kate_info *ki;
  size_t n;
  int ret;

  if (!k || !op) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  opb=&k->kes->opb;

  ret=kate_encode_start_header(opb,0x82);
  if (ret<0) return ret;

  ki=k->ki;
  if (!ki) return KATE_E_INIT;

  kate_write32v(opb,ki->nregions);

  for(n=0;n<ki->nregions;++n) {
    ret=kate_encode_region(ki->regions[n],opb);
    if (ret<0) return ret;
  }

  kate_warp(opb);

  return kate_finalize_packet_buffer(opb,op,k);
}

static int kate_encode_color(const kate_color *kc,oggpack_buffer *opb)
{
  if (!kc || !opb) return KATE_E_INVALID_PARAMETER;
  oggpack_write(opb,kc->r,8);
  oggpack_write(opb,kc->g,8);
  oggpack_write(opb,kc->b,8);
  oggpack_write(opb,kc->a,8);
  return 0;
}

static int kate_encode_style(const kate_style *ks,oggpack_buffer *opb)
{
  kate_float d[8];
  size_t idx;

  if (!ks || !opb) return KATE_E_INVALID_PARAMETER;

  idx=0;
  d[idx++]=ks->halign;
  d[idx++]=ks->valign;
  d[idx++]=ks->font_width;
  d[idx++]=ks->font_height;
  d[idx++]=ks->left_margin;
  d[idx++]=ks->top_margin;
  d[idx++]=ks->right_margin;
  d[idx++]=ks->bottom_margin;
  kate_fp_encode_kate_float(sizeof(d)/sizeof(d[0]),d,1,opb);
  kate_encode_color(&ks->text_color,opb);
  kate_encode_color(&ks->background_color,opb);
  kate_encode_color(&ks->draw_color,opb);
  oggpack_write(opb,ks->font_metric,8);
  oggpack_write(opb,ks->margin_metric,8);
  oggpack_write(opb,ks->bold,1);
  oggpack_write(opb,ks->italics,1);
  oggpack_write(opb,ks->underline,1);
  oggpack_write(opb,ks->strike,1);
  kate_warp(opb);

  return 0;
}

static int kate_encode_styles(kate_state *k,kate_packet *op)
{
  oggpack_buffer *opb;
  kate_info *ki;
  size_t n;
  int ret;

  if (!k || !op) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  opb=&k->kes->opb;

  ret=kate_encode_start_header(opb,0x83);
  if (ret<0) return ret;

  ki=k->ki;
  if (!ki) return KATE_E_INIT;

  kate_write32v(opb,ki->nstyles);

  for(n=0;n<ki->nstyles;++n) {
    ret=kate_encode_style(ki->styles[n],opb);
    if (ret<0) return ret;
  }

  kate_warp(opb);

  return kate_finalize_packet_buffer(opb,op,k);
}

static int kate_encode_curve(const kate_curve *kc,oggpack_buffer *opb)
{
  if (!kc || !opb) return KATE_E_INVALID_PARAMETER;

  oggpack_write(opb,kc->type,8);
  kate_write32v(opb,kc->npts);
  kate_warp(opb);
  if (kc->npts) kate_fp_encode_kate_float(kc->npts,kc->pts,2,opb);

  return 0;
}

static int kate_encode_curves(kate_state *k,kate_packet *op)
{
  oggpack_buffer *opb;
  kate_info *ki;
  size_t n;
  int ret;

  if (!k || !op) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  opb=&k->kes->opb;

  ret=kate_encode_start_header(opb,0x84);
  if (ret<0) return ret;

  ki=k->ki;
  if (!ki) return KATE_E_INIT;

  kate_write32v(opb,ki->ncurves);

  for(n=0;n<ki->ncurves;++n) {
    ret=kate_encode_curve(ki->curves[n],opb);
    if (ret<0) return ret;
  }

  kate_warp(opb);

  return kate_finalize_packet_buffer(opb,op,k);
}

static int kate_encode_motion(const kate_info *ki,const kate_motion *km,oggpack_buffer *opb)
{
  size_t n;
  int ret;

  if (!ki || !km || !opb) return KATE_E_INVALID_PARAMETER;

  kate_write32v(opb,km->ncurves);
  for (n=0;n<km->ncurves;++n) {
    int idx=kate_find_curve(ki,km->curves[n]);
    if (idx<0) {
      oggpack_write(opb,0,1);
      ret=kate_encode_curve(km->curves[n],opb);
      if (ret<0) return ret;
    }
    else {
      oggpack_write(opb,1,1);
      kate_write32v(opb,idx);
    }
  }
  kate_fp_encode_kate_float(km->ncurves,km->durations,1,opb);
  oggpack_write(opb,km->x_mapping,8);
  oggpack_write(opb,km->y_mapping,8);
  oggpack_write(opb,km->semantics,8);
  oggpack_write(opb,km->periodic,1);
  kate_warp(opb);

  return 0;
}

static int kate_encode_motions(kate_state *k,kate_packet *op)
{
  oggpack_buffer *opb;
  kate_info *ki;
  size_t n;
  int ret;

  if (!k || !op) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  opb=&k->kes->opb;

  ret=kate_encode_start_header(opb,0x85);
  if (ret<0) return ret;

  ki=k->ki;
  if (!ki) return KATE_E_INIT;

  kate_write32v(opb,ki->nmotions);

  for(n=0;n<ki->nmotions;++n) {
    ret=kate_encode_motion(ki,ki->motions[n],opb);
    if (ret<0) return ret;
  }

  kate_warp(opb);

  return kate_finalize_packet_buffer(opb,op,k);
}

static int kate_encode_palette(const kate_palette *kp,oggpack_buffer *opb)
{
  size_t n;

  if (!kp || !opb) return KATE_E_INVALID_PARAMETER;
  if (kp->ncolors<=0 || kp->ncolors>256) return KATE_E_LIMIT;

  oggpack_write(opb,kp->ncolors-1,8);
  for (n=0;n<kp->ncolors;++n) {
    int ret=kate_encode_color(kp->colors+n,opb);
    if (ret<0) return ret;
  }
  kate_warp(opb);

  return 0;
}

static int kate_encode_palettes(kate_state *k,kate_packet *op)
{
  oggpack_buffer *opb;
  kate_info *ki;
  size_t n;
  int ret;

  if (!k || !op) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  opb=&k->kes->opb;

  ret=kate_encode_start_header(opb,0x86);
  if (ret<0) return ret;

  ki=k->ki;
  if (!ki) return KATE_E_INIT;

  kate_write32v(opb,ki->npalettes);

  for(n=0;n<ki->npalettes;++n) {
    ret=kate_encode_palette(ki->palettes[n],opb);
    if (ret<0) return ret;
  }

  kate_warp(opb);

  return kate_finalize_packet_buffer(opb,op,k);
}

static int kate_encode_paletted_bitmap(const kate_bitmap *kb,oggpack_buffer *opb)
{
  size_t w,h,n;
  unsigned int maxpixel;

  if (kb->bpp>8) return KATE_E_LIMIT;

  kate_write32v(opb,kb->palette);
  n=0;
  maxpixel=(1<<kb->bpp)-1;
  for (h=0;h<kb->height;++h) {
    for (w=0;w<kb->width;++w) {
      unsigned int pixel=kb->pixels[n++];
      if (pixel>maxpixel) return KATE_E_LIMIT;
      oggpack_write(opb,pixel,kb->bpp);
    }
  }

  return 0;
}

static int kate_encode_png_bitmap(const kate_bitmap *kb,oggpack_buffer *opb)
{
  oggpack_write(opb,kate_bitmap_type_png,8);
  kate_write32(opb,kb->size);
  kate_writebuf(opb,(const char*)kb->pixels,kb->size);

  return 0;
}

static int kate_encode_bitmap(const kate_bitmap *kb,oggpack_buffer *opb)
{
  int ret;

  if (!kb || !opb) return KATE_E_INVALID_PARAMETER;

  kate_write32v(opb,kb->width);
  kate_write32v(opb,kb->height);
  oggpack_write(opb,kb->bpp,8); /* 0 marks a raw bitmap */

  switch (kb->type) {
    case kate_bitmap_type_paletted:
      ret=kate_encode_paletted_bitmap(kb,opb);
      break;
    case kate_bitmap_type_png:
      ret=kate_encode_png_bitmap(kb,opb);
      break;
    default:
      ret=KATE_E_INVALID_PARAMETER;
      break;
  }

  if (ret<0) return ret;

  kate_warp(opb);

  return 0;
}

static int kate_encode_bitmaps(kate_state *k,kate_packet *op)
{
  oggpack_buffer *opb;
  kate_info *ki;
  size_t n;
  int ret;

  if (!k || !op) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  opb=&k->kes->opb;

  ret=kate_encode_start_header(opb,0x87);
  if (ret<0) return ret;

  ki=k->ki;
  if (!ki) return KATE_E_INIT;

  kate_write32v(opb,ki->nbitmaps);

  for(n=0;n<ki->nbitmaps;++n) {
    ret=kate_encode_bitmap(ki->bitmaps[n],opb);
    if (ret<0) return ret;
  }

  kate_warp(opb);

  return kate_finalize_packet_buffer(opb,op,k);
}

static int kate_encode_font_range(const kate_info *ki,const kate_font_range *kfr,oggpack_buffer *opb)
{
  if (!ki || !kfr || !opb) return KATE_E_INVALID_PARAMETER;

  if (!kate_is_valid_code_point(kfr->first_code_point)) return KATE_E_TEXT;
  if (!kate_is_valid_code_point(kfr->last_code_point)) return KATE_E_TEXT;
  if (kfr->first_bitmap<0) return KATE_E_LIMIT;
  if ((size_t)(kfr->first_bitmap+(kfr->last_code_point-kfr->first_code_point))>=ki->nbitmaps) return KATE_E_LIMIT;

  kate_write32v(opb,kfr->first_code_point);
  kate_write32v(opb,kfr->last_code_point);
  kate_write32v(opb,kfr->first_bitmap);
  kate_warp(opb);

  return 0;
}

static int kate_encode_font_ranges(kate_state *k,kate_packet *op)
{
  oggpack_buffer *opb;
  kate_info *ki;
  size_t n,l;
  int ret;

  if (!k || !op) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  opb=&k->kes->opb;

  ret=kate_encode_start_header(opb,0x88);
  if (ret<0) return ret;

  ki=k->ki;
  if (!ki) return KATE_E_INIT;

  kate_write32v(opb,ki->nfont_ranges);
  for(n=0;n<ki->nfont_ranges;++n) {
    ret=kate_encode_font_range(ki,ki->font_ranges[n],opb);
    if (ret<0) return ret;
  }

  kate_write32v(opb,ki->nfont_mappings);
  for(n=0;n<ki->nfont_mappings;++n) {
    const kate_font_mapping *kfm=ki->font_mappings[n];
    kate_write32v(opb,kfm->nranges);
    for (l=0;l<kfm->nranges;++l) {
      const kate_font_range *kfr=kfm->ranges[l];
      int idx=kate_find_font_range(ki,kfr);
      if (idx>=0) {
        oggpack_write(opb,1,1);
        kate_write32v(opb,idx);
      }
      else {
        oggpack_write(opb,0,1);
        ret=kate_encode_font_range(ki,kfr,opb);
        if (ret<0) return ret;
      }
    }
  }

  kate_warp(opb);

  return kate_finalize_packet_buffer(opb,op,k);
}

static inline int kate_check_granule(kate_state *k,kate_int64_t *granulepos)
{
  if (*granulepos<k->kes->granulepos) return -1;
  return 0;
}

#define WRITE_OVERRIDE(opb,var,def,write) \
  do \
    if (kes->overrides.var!=(def)) { \
      oggpack_write(opb,1,1); \
      write; \
    } \
    else oggpack_write(opb,0,1); \
  while(0)

static int kate_encode_overrides(kate_state *k,oggpack_buffer *opb)
{
  kate_encode_state *kes;

  if (!k || !opb) return KATE_E_INVALID_PARAMETER;
  kes=k->kes;
  if (!kes) return KATE_E_INIT;

  if (kes->overrides.language
   || kes->overrides.text_encoding!=k->ki->text_encoding
   || kes->overrides.text_directionality!=k->ki->text_directionality
   || kes->overrides.region_index>=0
   || kes->overrides.region
   || kes->overrides.style_index>=0
   || kes->overrides.style
   || kes->overrides.secondary_style_index>=0
   || kes->overrides.secondary_style
   || kes->overrides.font_mapping_index>=0
  ) {
    oggpack_write(opb,1,1);
    WRITE_OVERRIDE(opb,text_encoding,k->ki->text_encoding,oggpack_write(opb,kes->overrides.text_encoding,8));
    WRITE_OVERRIDE(opb,text_directionality,k->ki->text_directionality,oggpack_write(opb,kes->overrides.text_directionality,8));
    WRITE_OVERRIDE(opb,language,NULL,
      do {
        size_t len=strlen(kes->overrides.language);
        kate_write32v(opb,len);
        kate_writebuf(opb,kes->overrides.language,len);
      } while(0));
    WRITE_OVERRIDE(opb,region_index,-1,kate_write32v(opb,kes->overrides.region_index));
    WRITE_OVERRIDE(opb,region,NULL,kate_encode_region(kes->overrides.region,opb));
    WRITE_OVERRIDE(opb,style_index,-1,kate_write32v(opb,kes->overrides.style_index));
    WRITE_OVERRIDE(opb,style,NULL,kate_encode_style(kes->overrides.style,opb));
    WRITE_OVERRIDE(opb,secondary_style_index,-1,kate_write32v(opb,kes->overrides.secondary_style_index));
    WRITE_OVERRIDE(opb,secondary_style,NULL,kate_encode_style(kes->overrides.secondary_style,opb));
    WRITE_OVERRIDE(opb,font_mapping_index,-1,kate_write32v(opb,kes->overrides.font_mapping_index));
  }
  else oggpack_write(opb,0,1);

  {
    /* bitstream 0.2: add palette and bitmap */
    oggpack_buffer warp;
    kate_open_warp(&warp);
    if (kes->overrides.palette_index>=0
     || kes->overrides.palette
     || kes->overrides.bitmap_index>=0
     || kes->overrides.bitmap) {
      oggpack_write(&warp,1,1);
      WRITE_OVERRIDE(&warp,palette_index,-1,kate_write32v(&warp,kes->overrides.palette_index));
      WRITE_OVERRIDE(&warp,palette,NULL,kate_encode_palette(kes->overrides.palette,&warp));
      WRITE_OVERRIDE(&warp,bitmap_index,-1,kate_write32v(&warp,kes->overrides.bitmap_index));
      WRITE_OVERRIDE(&warp,bitmap,NULL,kate_encode_bitmap(kes->overrides.bitmap,&warp));
    }
    else oggpack_write(&warp,0,1);
    kate_close_warp(&warp,opb);
  }

  kate_warp(opb);

  return 0;
}

/**
  \ingroup encoding
  Encodes a text (which may be NULL) of the given size, starting at t0 and ending at t1.
  This should always be called when encoding an event, even if the text is NULL.
  After this is called, the event is fully encoded and cannot be added to anymore.
  \param k the kate_state to add the text to
  \param t0 the start time in seconds of the text
  \param t1 the end time in seconds of the text
  \param text the text to add (may be NULL)
  \param sz the length in bytes of the text to add
  \param op the packet to encode to
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_text(kate_state *k,kate_float t0,kate_float t1,const char *text,size_t sz,kate_packet *op)
{
  oggpack_buffer *opb;
  kate_int64_t start_granulepos;
  kate_int64_t start;
  kate_int64_t duration;
  kate_int64_t backlink;
  kate_float earliest_t;
  int ret;
  size_t n;

  if (!k || !text || !op) return KATE_E_INVALID_PARAMETER;
  if (t0<0 || t1<t0) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;
  if (k->kes->eos) return KATE_E_INIT;

  ret=kate_text_validate(k->kes->overrides.text_encoding,text,sz);
  if (ret<0) return ret;

  ret=kate_encode_state_trim_events(k->kes,t0);
  if (ret<0) return ret;
  ret=kate_encode_state_add_event(k->kes,t0,t1);
  if (ret<0) return ret;
  ret=kate_encode_state_get_earliest_event(k->kes,&earliest_t,NULL);
  if (ret<0) return ret;

  start_granulepos=kate_time_granule(k->ki,earliest_t,t0-earliest_t);
  if (start_granulepos<0) return start_granulepos;
  if (kate_check_granule(k,&start_granulepos)<0) return KATE_E_BAD_GRANULE;

  start=kate_duration_granule(k->ki,t0);
  if (start<0) return start;
  duration=kate_duration_granule(k->ki,t1-t0);
  if (duration<0) return duration;
  backlink=kate_duration_granule(k->ki,t0-earliest_t);
  if (backlink<0) return backlink;

  opb=&k->kes->opb;
  oggpack_write(opb,0x00,8);

  kate_write64(opb,start);
  kate_write64(opb,duration);
  kate_write64(opb,backlink);

  kate_write32(opb,sz);
  kate_writebuf(opb,text,sz);

  if (k->kes->id>=0) {
    oggpack_write(opb,1,1);
    kate_write32v(opb,k->kes->id);
  }
  else {
    oggpack_write(opb,0,1);
  }

  if (k->kes->nmotions) {
    oggpack_write(opb,1,1);
    kate_write32v(opb,k->kes->nmotions);
    for (n=0;n<k->kes->nmotions;++n) {
      if (k->kes->motions[n]==NULL) {
        /* we have an index into the motions header */
        oggpack_write(opb,1,1);
        kate_write32v(opb,k->kes->motion_indices[n]);
      }
      else {
        /* we have a fully defined motion */
        oggpack_write(opb,0,1);
        ret=kate_encode_motion(k->ki,k->kes->motions[n],opb);
        if (ret<0) return ret;
      }
    }
  }
  else oggpack_write(opb,0,1);

  kate_encode_overrides(k,opb);

  if (start_granulepos>k->kes->furthest_granule) k->kes->furthest_granule=start_granulepos;

  k->kes->granulepos=start_granulepos;
  return kate_finalize_packet_buffer(opb,op,k);
}

/**
  \ingroup encoding
  Emits a keepalive packet, to help with seeking.
  \param k the kate_state to encode to
  \param t the timestamp for the keepalive packet
  \param op the packet to encode to
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_keepalive(kate_state *k,kate_float t,kate_packet *op)
{
  oggpack_buffer *opb;
  kate_int64_t granulepos;

  if (!k || !op) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;
  if (k->kes->eos) return KATE_E_INIT;

  granulepos=kate_time_granule(k->ki,t,0);
  if (granulepos<0) return granulepos;

  if (kate_check_granule(k,&granulepos)<0) return KATE_E_BAD_GRANULE;
  k->kes->granulepos=granulepos;

  opb=&k->kes->opb;
  oggpack_write(opb,0x01,8);

  return kate_finalize_packet_buffer(opb,op,k);
}

/**
  \ingroup encoding
  Finalizes the currently encoded stream.
  No more events may be added after this is called.
  \param k the kate_state to encode to
  \param t the timestamp for the end (if negative, the end time of the last event will be used)
  \param op the packet to encode to
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_finish(kate_state *k,kate_float t,kate_packet *op)
{
  oggpack_buffer *opb;
  kate_int64_t granulepos;

  if (!k || !op) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;
  if (k->kes->eos) return KATE_E_INIT;

  if (t<0) {
    int ret=kate_encode_state_get_latest_event(k->kes,NULL,&t);
    if (ret==KATE_E_NOT_FOUND) {
      /* if nothing was encoded, it is still a valid stream */
      t=0;
      ret=0;
    }
    if (ret<0) return ret;
  }

  granulepos=kate_time_granule(k->ki,t,0);
  if (granulepos<0) return granulepos;

  if (kate_check_granule(k,&granulepos)<0) return KATE_E_BAD_GRANULE;
  k->kes->granulepos=granulepos;

  opb=&k->kes->opb;
  oggpack_write(opb,0x7f,8);

  k->kes->eos=1;

  return kate_finalize_packet_buffer(opb,op,k);
}

/**
  \ingroup encoding
  Encodes a header.
  This should be repeatedly called at the beginning of encoding, until a
  positive value is returned, marking the encoding of the last header.
  \param k the kate_state to encode to
  \param kc the list of comments to add to the headers
  \param op the packet to encode to
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_headers(kate_state *k,kate_comment *kc,kate_packet *op)
{
  if (!k || !kc || !op) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;
  if (k->kes->eos) return KATE_E_INIT;

  switch (k->kes->packetno+1) {
    case 0: return kate_encode_info(k,op);
    case 1: return kate_encode_comment(k,kc,op);
    case 2: return kate_encode_regions(k,op);
    case 3: return kate_encode_styles(k,op);
    case 4: return kate_encode_curves(k,op);
    case 5: return kate_encode_motions(k,op);
    case 6: return kate_encode_palettes(k,op);
    case 7: return kate_encode_bitmaps(k,op);
    case 8: return kate_encode_font_ranges(k,op);
    case 9: return 1;
    default: return KATE_E_INVALID_PARAMETER;
  }
}

/**
  \ingroup encoding
  Adds a motion to the currently encoded event.
  If destroy is set, the motion will be automatically destroyed after the current
  event has been encoded.
  \param k the kate_state to add the motion to
  \param km the motion to add
  \param destroy if true, the motion will be destroyed when the event is fully encoded
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_add_motion(kate_state *k,kate_motion *km,int destroy)
{
  if (!k || !km) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  return kate_encode_state_add_motion(k->kes,km,destroy);
}

/**
  \ingroup encoding
  Adds a motion to the currently encoded event by its index into the list of predefined motions.
  \param k the kate_state to add the motion to
  \param motion the index of the motion to add
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_add_motion_index(kate_state *k,size_t motion)
{
  if (!k) return KATE_E_INVALID_PARAMETER;
  if (!k->ki) return KATE_E_INIT;
  if (motion>=k->ki->nmotions) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  return kate_encode_state_add_motion_index(k->kes,motion);
}

/**
  \ingroup encoding
  Sets the region the event should be displayed in, by its index in the predefined regions list.
  \param k the kate_state to encode to
  \param region the index of the predefined region to use
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_set_region_index(kate_state *k,size_t region)
{
  if (!k) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;
  if (!k->ki) return KATE_E_INIT;
  if (region>=k->ki->nregions) return KATE_E_INVALID_PARAMETER;
  if (k->kes->overrides.region) return KATE_E_INIT;
  k->kes->overrides.region_index=region;
  return 0;
}

/**
  \ingroup encoding
  Sets the region the event should be displayed in.
  \param k the kate_state to encode to
  \param kr the region to use
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_set_region(kate_state *k,const kate_region *kr)
{
  if (!k || !kr) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;
  if (!k->ki) return KATE_E_INIT;
  if (k->kes->overrides.region_index>=0) return KATE_E_INIT;
  k->kes->overrides.region=kr;
  return 0;
}

/**
  \ingroup encoding
  Sets the style the event should be displayed with, by its index in the predefined styles list.
  \param k the kate_state to encode to
  \param style the index of the predefined style to use
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_set_style_index(kate_state *k,size_t style)
{
  if (!k) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;
  if (!k->ki) return KATE_E_INIT;
  if (style>=k->ki->nstyles) return KATE_E_INVALID_PARAMETER;
  if (k->kes->overrides.style) return KATE_E_INIT;
  k->kes->overrides.style_index=style;
  return 0;
}

/**
  \ingroup encoding
  Sets the style the event should be displayed with.
  \param k the kate_state to encode to
  \param ks the style to use
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_set_style(kate_state *k,const kate_style *ks)
{
  if (!k || !ks) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;
  if (!k->ki) return KATE_E_INIT;
  if (k->kes->overrides.style_index>=0) return KATE_E_INIT;
  k->kes->overrides.style=ks;
  return 0;
}

/**
  \ingroup encoding
  Sets the secondary style the event should be displayed with, by its index in the predefined styles list.
  \param k the kate_state to encode to
  \param style the index of the predefined style to use
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_set_secondary_style_index(kate_state *k,size_t style)
{
  if (!k) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;
  if (!k->ki) return KATE_E_INIT;
  if (style>=k->ki->nstyles) return KATE_E_INVALID_PARAMETER;
  if (k->kes->overrides.secondary_style) return KATE_E_INIT;
  k->kes->overrides.secondary_style_index=style;
  return 0;
}

/**
  \ingroup encoding
  Sets the secondary style the event should be displayed with.
  \param k the kate_state to encode to
  \param ks the style to use
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_set_secondary_style(kate_state *k,const kate_style *ks)
{
  if (!k || !ks) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;
  if (!k->ki) return KATE_E_INIT;
  if (k->kes->overrides.secondary_style_index>=0) return KATE_E_INIT;
  k->kes->overrides.secondary_style=ks;
  return 0;
}

/**
  \ingroup encoding
  Sets the palette the event should use for its bitmap, by its index in the predefined palette list.
  \param k the kate_state to encode to
  \param palette the index of the predefined palette to use
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_set_palette_index(kate_state *k,size_t palette)
{
  if (!k) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;
  if (!k->ki) return KATE_E_INIT;
  if (palette>=k->ki->npalettes) return KATE_E_INVALID_PARAMETER;
  if (k->kes->overrides.palette) return KATE_E_INIT;
  k->kes->overrides.palette_index=palette;
  return 0;
}

/**
  \ingroup encoding
  Adds a palette to the currently encoded event.
  \param k the kate_state to set the palette for
  \param kp the palette to set
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_set_palette(kate_state *k,const kate_palette *kp)
{
  if (!k || !kp) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;
  if (!k->ki) return KATE_E_INIT;
  if (k->kes->overrides.palette_index>=0) return KATE_E_INIT;
  k->kes->overrides.palette=kp;
  return 0;
}

/**
  \ingroup encoding
  Sets the bitmap the event should use, by its index in the predefined bitmap list.
  \param k the kate_state to encode to
  \param bitmap the index of the predefined bitmap to use
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_set_bitmap_index(kate_state *k,size_t bitmap)
{
  if (!k) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;
  if (!k->ki) return KATE_E_INIT;
  if (bitmap>=k->ki->nbitmaps) return KATE_E_INVALID_PARAMETER;
  if (k->kes->overrides.bitmap) return KATE_E_INIT;
  k->kes->overrides.bitmap_index=bitmap;
  return 0;
}

/**
  \ingroup encoding
  Adds a bitmap to the currently encoded event.
  \param k the kate_state to set the bitmap for
  \param kb the bitmap to set
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_set_bitmap(kate_state *k,const kate_bitmap *kb)
{
  if (!k || !kb) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;
  if (!k->ki) return KATE_E_INIT;
  if (k->kes->overrides.bitmap_index>=0) return KATE_E_INIT;
  k->kes->overrides.bitmap=kb;
  return 0;
}

/**
  \ingroup encoding
  Sets the font mapping this event's text should be displayed with, by its index into the predefined
  font mappings list.
  \param k the kate_state to encode to
  \param font_mapping the font mapping to use
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_set_font_mapping_index(kate_state *k,size_t font_mapping)
{
  if (!k) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;
  if (!k->ki) return KATE_E_INIT;
  if (font_mapping>=k->ki->nfont_mappings) return KATE_E_INVALID_PARAMETER;
  k->kes->overrides.font_mapping_index=font_mapping;
  return 0;
}

/**
  \ingroup encoding
  Sets the character encoding used in this event's text, overriding the default character encoding.
  \param k the kate_state to encode to
  \param text_encoding the text encoding for this event's text
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_set_text_encoding(kate_state *k,kate_text_encoding text_encoding)
{
  if (!k) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;
  k->kes->overrides.text_encoding=text_encoding;
  return 0;
}

/**
  \ingroup encoding
  Sets the text directionality used in this event's text, overriding the default text directionality.
  \param k the kate_state to encode to
  \param text_directionality the text directionality for this event's text
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_set_text_directionality(kate_state *k,kate_text_directionality text_directionality)
{
  if (!k) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;
  k->kes->overrides.text_directionality=text_directionality;
  return 0;
}

/**
  \ingroup encoding
  Sets a unique identifier for the currently encoded event, so it can be referred to later
  \param k the kate_state to encode to
  \param id the id to tag this event with (must be positive)
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_set_id(kate_state *k,kate_int32_t id)
{
  if (!k) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  k->kes->id=id;

  return 0;
}

/**
  \ingroup encoding
  Sets the language used in this event's text, overriding the default language
  \param k the kate_state to encode to
  \param language the language for this event's text - may be NULL (use the default language)
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_set_language(kate_state *k,const char *language) /* language can be NULL */
{
  size_t len;
  char *l=NULL;

  if (!k) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  if (language) {
    len=strlen(language);
    l=(char*)kate_malloc(len+1);
    if (!l) return KATE_E_OUT_OF_MEMORY;
    memcpy(l,language,len+1);
  }

  if (k->kes->overrides.language) kate_free(k->kes->overrides.language);
  k->kes->overrides.language=l;

  return 0;
}

