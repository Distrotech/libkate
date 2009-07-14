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
#include "kate_encode_state.h"
#include "kate_fp.h"
#include "kate_rle.h"
#include "kate_meta.h"

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
  k->kes=kate_encode_state_create(ki);
  if (!k->kes) return KATE_E_OUT_OF_MEMORY;

  return 0;
}

static inline void kate_pack_write1(kate_pack_buffer *kpb,long value)
{
  kate_pack_write(kpb,value,1);
}

static void kate_writebuf(kate_pack_buffer *kpb,const char *s,int len)
{
  while (len--) kate_pack_write(kpb,*s++,8);
}

static void kate_write32(kate_pack_buffer *kpb,kate_int32_t v)
{
  kate_pack_write(kpb,v&0xff,8);
  v>>=8;
  kate_pack_write(kpb,v&0xff,8);
  v>>=8;
  kate_pack_write(kpb,v&0xff,8);
  v>>=8;
  kate_pack_write(kpb,v&0xff,8);
}

static void kate_write32v(kate_pack_buffer *kpb,kate_int32_t v)
{
  if (v>=0 && v<=14) {
    kate_pack_write(kpb,v,4);
  }
  else {
    int bits=0;
    kate_int32_t tmp;
    kate_pack_write(kpb,15,4);
    if (v&0x80000000) {
      kate_pack_write1(kpb,1);
      v=-v;
    }
    else {
      kate_pack_write1(kpb,0);
    }
    tmp=v;
    while (tmp) {
      ++bits;
      tmp>>=1;
    }
    if (bits==0) bits=1;
    kate_pack_write(kpb,bits-1,5);
    kate_pack_write(kpb,v,bits);
  }
}

static void kate_write64(kate_pack_buffer *kpb,kate_int64_t v)
{
  kate_write32(kpb,(kate_int32_t)v);
  v>>=32;
  kate_write32(kpb,(kate_int32_t)v);
}

static void kate_open_warp(kate_pack_buffer *warp)
{
  kate_pack_writeinit(warp);
}

static void kate_close_warp(kate_pack_buffer *warp,kate_pack_buffer *kpb)
{
  int bits=kate_pack_bits(warp);
  unsigned char *buffer=kate_pack_get_buffer(warp);
  kate_write32v(kpb,bits);
  while (bits>0) {
    kate_pack_writecopy(kpb,buffer,bits>32?32:bits);
    buffer+=32/8;
    bits-=32;
  }
  kate_pack_writeclear(warp);
}

static void kate_warp(kate_pack_buffer *kpb)
{
  kate_pack_buffer warp;
  kate_open_warp(&warp);
  kate_close_warp(&warp,kpb);
}

static void kate_write_metadata(kate_pack_buffer *kpb,const kate_meta *km)
{
  size_t n;

  kate_pack_write1(kpb,km?1:0);
  if (km) {
    kate_meta_leaf *kml=km->meta;
    kate_write32v(kpb,km->nmeta);
    for (n=0;n<km->nmeta;++n,++kml) {
      size_t len=strlen(kml->tag);
      kate_write32v(kpb,len);
      kate_writebuf(kpb,kml->tag,len);
      kate_write32v(kpb,kml->len);
      kate_writebuf(kpb,kml->value,kml->len);
      kate_warp(kpb);
    }

    kate_warp(kpb);
  }
}

static int kate_finalize_packet_buffer(kate_pack_buffer *kpb,kate_packet *kp,kate_state *k)
{
  if (!kpb || !kp || !k) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  /* fill up any remaining bits in the last byte with zeros */
  kate_pack_writealign(kpb);

  kp->nbytes=kate_pack_bytes(kpb);
  kp->data=kate_malloc(kp->nbytes);
  if (!kp->data) return KATE_E_OUT_OF_MEMORY;

  memcpy(kp->data,kate_pack_get_buffer(kpb),kp->nbytes);

  /* reset the buffer so we're ready for next packet */
  kate_pack_writeclear(kpb);
  kate_pack_writeinit(kpb);

  ++k->kes->packetno;

  /* clear any overrides */
  return kate_encode_state_clear_overrides(k->kes);
}

static int kate_encode_start_header(kate_pack_buffer *kpb,int headerid)
{
  if (!kpb || !(headerid&0x80)) return KATE_E_INVALID_PARAMETER;

  kate_pack_write(kpb,headerid,8);
  kate_writebuf(kpb,"kate\0\0\0",7);
  kate_pack_write(kpb,0,8); /* reserved - 0 */

  return 0;
}

static int kate_encode_write_canvas_size(kate_pack_buffer *kpb,size_t size)
{
  size_t base=size;
  size_t shift=0;

  if (!kpb) return KATE_E_INVALID_PARAMETER;

  while (base&~((1<<12)-1)) {
    /* we have a high bit we can't fit, increase shift if we wouldn't lose low bits */
    if ((size>>shift)&1) return KATE_E_LIMIT;
    ++shift;
    base>>=1;
  }
  if (shift>=16) return KATE_E_LIMIT;

  /* the size can be represented in our encoding */
  kate_pack_write(kpb,shift,4);
  kate_pack_write(kpb,base&0x0f,4);
  kate_pack_write(kpb,base>>4,8);

  return 0;
}

static int kate_encode_info(kate_state *k,kate_packet *kp)
{
  kate_pack_buffer *kpb;
  kate_info *ki;
  size_t len;
  int ret;

  if (!k || !kp) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  kpb=&k->kes->kpb;

  ret=kate_encode_start_header(kpb,0x80);
  if (ret<0) return ret;

  ki=k->ki;
  kate_pack_write(kpb,KATE_BITSTREAM_VERSION_MAJOR,8);
  kate_pack_write(kpb,KATE_BITSTREAM_VERSION_MINOR,8);
  kate_pack_write(kpb,ki->num_headers,8);
  kate_pack_write(kpb,ki->text_encoding,8);
  kate_pack_write(kpb,ki->text_directionality,8);
  kate_pack_write(kpb,0,8); /* reserved - 0 */
  kate_pack_write(kpb,kate_granule_shift(k->ki),8);
  ret=kate_encode_write_canvas_size(kpb,ki->original_canvas_width);
  if (ret<0) return ret;
  ret=kate_encode_write_canvas_size(kpb,ki->original_canvas_height);
  if (ret<0) return ret;
  kate_write32(kpb,0); /* reserved - 0 */
  kate_write32(kpb,ki->gps_numerator);
  kate_write32(kpb,ki->gps_denominator);

  /* language is a 15+1 character max null terminated string */
  if (ki->language) {
    len=strlen(ki->language);
    if (len>15) return KATE_E_LIMIT;
    kate_writebuf(kpb,ki->language,len);
  }
  else len=0;
  while (len++<16) kate_pack_write(kpb,0,8);

  /* category is a 15+1 character max null terminated string */
  if (ki->category) {
    len=strlen(ki->category);
    if (len>15) return KATE_E_LIMIT;
    kate_writebuf(kpb,ki->category,len);
  }
  else len=0;
  while (len++<16) kate_pack_write(kpb,0,8);

  return kate_finalize_packet_buffer(kpb,kp,k);
}

static int kate_encode_comment(kate_state *k,kate_comment *kc,kate_packet *kp)
{
  kate_pack_buffer *kpb;
  const char *vendor;
  int vendor_len;
  int ret;

  if (!k || !kc || !kp) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  kpb=&k->kes->kpb;

  ret=kate_encode_start_header(kpb,0x81);
  if (ret<0) return ret;

  vendor=kate_get_version_string();
  if (!vendor) return KATE_E_INIT; /* wtf ??? */
  vendor_len=strlen(vendor);

  /* mostly copied from theora encoder_toplevel.c */
  kate_write32(kpb,vendor_len);
  kate_writebuf(kpb,vendor,vendor_len);

  if (kc->comments<0) return KATE_E_INIT;
  kate_write32(kpb,kc->comments);
  if (kc->comments) {
    int i;
    for(i=0;i<kc->comments;++i) {
      if (kc->user_comments[i]) {
        if (kc->comment_lengths[i]<0) return KATE_E_INIT;
        kate_write32(kpb,kc->comment_lengths[i]);
        ret=kate_text_validate(kate_utf8,kc->user_comments[i],kc->comment_lengths[i]);
        if (ret<0) return ret;
        kate_writebuf(kpb,kc->user_comments[i],kc->comment_lengths[i]);
      }
      else {
        kate_write32(kpb,0);
      }
    }
  }

  return kate_finalize_packet_buffer(kpb,kp,k);
}

static int kate_encode_region(const kate_region *kr,kate_pack_buffer *kpb)
{
  if (!kr || !kpb) return KATE_E_INVALID_PARAMETER;

  kate_pack_write(kpb,kr->metric,8);
  kate_write32v(kpb,kr->x);
  kate_write32v(kpb,kr->y);
  kate_write32v(kpb,kr->w);
  kate_write32v(kpb,kr->h);
  kate_write32v(kpb,kr->style);

  {
    /* bitstream 0.2: add clip */
    kate_pack_buffer warp;
    kate_open_warp(&warp);
    kate_pack_write1(&warp,kr->clip);
    kate_close_warp(&warp,kpb);
  }

  {
    /* bitstream 0.6: add metadata */
    kate_pack_buffer warp;
    kate_open_warp(&warp);
    kate_write_metadata(&warp,kr->meta);
    kate_close_warp(&warp,kpb);
  }

  kate_warp(kpb);

  return 0;
}

static int kate_encode_regions(kate_state *k,kate_packet *kp)
{
  kate_pack_buffer *kpb;
  kate_info *ki;
  size_t n;
  int ret;

  if (!k || !kp) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  kpb=&k->kes->kpb;

  ret=kate_encode_start_header(kpb,0x82);
  if (ret<0) return ret;

  ki=k->ki;
  if (!ki) return KATE_E_INIT;

  kate_write32v(kpb,ki->nregions);

  for(n=0;n<ki->nregions;++n) {
    ret=kate_encode_region(ki->regions[n],kpb);
    if (ret<0) return ret;
  }

  kate_warp(kpb);

  return kate_finalize_packet_buffer(kpb,kp,k);
}

static int kate_encode_color(const kate_color *kc,kate_pack_buffer *kpb)
{
  if (!kc || !kpb) return KATE_E_INVALID_PARAMETER;
  kate_pack_write(kpb,kc->r,8);
  kate_pack_write(kpb,kc->g,8);
  kate_pack_write(kpb,kc->b,8);
  kate_pack_write(kpb,kc->a,8);
  return 0;
}

static int kate_encode_style(const kate_style *ks,kate_pack_buffer *kpb)
{
  kate_float d[8];
  size_t idx;

  if (!ks || !kpb) return KATE_E_INVALID_PARAMETER;

  idx=0;
  d[idx++]=ks->halign;
  d[idx++]=ks->valign;
  d[idx++]=ks->font_width;
  d[idx++]=ks->font_height;
  d[idx++]=ks->left_margin;
  d[idx++]=ks->top_margin;
  d[idx++]=ks->right_margin;
  d[idx++]=ks->bottom_margin;
  kate_fp_encode_kate_float(sizeof(d)/sizeof(d[0]),d,1,kpb);
  kate_encode_color(&ks->text_color,kpb);
  kate_encode_color(&ks->background_color,kpb);
  kate_encode_color(&ks->draw_color,kpb);
  kate_pack_write(kpb,ks->font_metric,8);
  kate_pack_write(kpb,ks->margin_metric,8);
  kate_pack_write1(kpb,ks->bold);
  kate_pack_write1(kpb,ks->italics);
  kate_pack_write1(kpb,ks->underline);
  kate_pack_write1(kpb,ks->strike);

  {
    /* bitstream 0.2: add justify and font */
    kate_pack_buffer warp;
    kate_open_warp(&warp);
    kate_pack_write1(&warp,ks->justify);
    if (ks->font) {
      size_t len=strlen(ks->font);
      kate_write32v(&warp,len);
      kate_writebuf(&warp,ks->font,len);
    }
    else {
      kate_write32v(&warp,0);
    }
    kate_close_warp(&warp,kpb);
  }

  {
    /* bitstream 0.4: add nowrap */
    kate_pack_buffer warp;
    kate_open_warp(&warp);
    kate_write32v(&warp,ks->wrap_mode);
    kate_close_warp(&warp,kpb);
  }

  {
    /* bitstream 0.6: add metadata */
    kate_pack_buffer warp;
    kate_open_warp(&warp);
    kate_write_metadata(&warp,ks->meta);
    kate_close_warp(&warp,kpb);
  }

  kate_warp(kpb);

  return 0;
}

static int kate_encode_styles(kate_state *k,kate_packet *kp)
{
  kate_pack_buffer *kpb;
  kate_info *ki;
  size_t n;
  int ret;

  if (!k || !kp) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  kpb=&k->kes->kpb;

  ret=kate_encode_start_header(kpb,0x83);
  if (ret<0) return ret;

  ki=k->ki;
  if (!ki) return KATE_E_INIT;

  kate_write32v(kpb,ki->nstyles);

  for(n=0;n<ki->nstyles;++n) {
    ret=kate_encode_style(ki->styles[n],kpb);
    if (ret<0) return ret;
  }

  kate_warp(kpb);

  return kate_finalize_packet_buffer(kpb,kp,k);
}

static int kate_encode_curve(const kate_curve *kc,kate_pack_buffer *kpb)
{
  if (!kc || !kpb) return KATE_E_INVALID_PARAMETER;

  kate_pack_write(kpb,kc->type,8);
  kate_write32v(kpb,kc->npts);
  kate_warp(kpb);
  if (kc->npts) kate_fp_encode_kate_float(kc->npts,kc->pts,2,kpb);

  return 0;
}

static int kate_encode_curves(kate_state *k,kate_packet *kp)
{
  kate_pack_buffer *kpb;
  kate_info *ki;
  size_t n;
  int ret;

  if (!k || !kp) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  kpb=&k->kes->kpb;

  ret=kate_encode_start_header(kpb,0x84);
  if (ret<0) return ret;

  ki=k->ki;
  if (!ki) return KATE_E_INIT;

  kate_write32v(kpb,ki->ncurves);

  for(n=0;n<ki->ncurves;++n) {
    ret=kate_encode_curve(ki->curves[n],kpb);
    if (ret<0) return ret;
  }

  kate_warp(kpb);

  return kate_finalize_packet_buffer(kpb,kp,k);
}

static int kate_encode_motion(const kate_info *ki,const kate_motion *km,kate_pack_buffer *kpb)
{
  size_t n;
  int ret;

  if (!ki || !km || !kpb) return KATE_E_INVALID_PARAMETER;

  kate_write32v(kpb,km->ncurves);
  for (n=0;n<km->ncurves;++n) {
    int idx=kate_find_curve(ki,km->curves[n]);
    if (idx<0) {
      kate_pack_write1(kpb,0);
      ret=kate_encode_curve(km->curves[n],kpb);
      if (ret<0) return ret;
    }
    else {
      kate_pack_write1(kpb,1);
      kate_write32v(kpb,idx);
    }
  }
  kate_fp_encode_kate_float(km->ncurves,km->durations,1,kpb);
  kate_pack_write(kpb,km->x_mapping,8);
  kate_pack_write(kpb,km->y_mapping,8);
  kate_pack_write(kpb,km->semantics,8);
  kate_pack_write1(kpb,km->periodic);

  {
    /* bitstream 0.6: add metadata */
    kate_pack_buffer warp;
    kate_open_warp(&warp);
    kate_write_metadata(&warp,km->meta);
    kate_close_warp(&warp,kpb);
  }


  kate_warp(kpb);

  return 0;
}

static int kate_encode_motions(kate_state *k,kate_packet *kp)
{
  kate_pack_buffer *kpb;
  kate_info *ki;
  size_t n;
  int ret;

  if (!k || !kp) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  kpb=&k->kes->kpb;

  ret=kate_encode_start_header(kpb,0x85);
  if (ret<0) return ret;

  ki=k->ki;
  if (!ki) return KATE_E_INIT;

  kate_write32v(kpb,ki->nmotions);

  for(n=0;n<ki->nmotions;++n) {
    ret=kate_encode_motion(ki,ki->motions[n],kpb);
    if (ret<0) return ret;
  }

  kate_warp(kpb);

  return kate_finalize_packet_buffer(kpb,kp,k);
}

static int kate_encode_palette(const kate_palette *kp,kate_pack_buffer *kpb)
{
  size_t n;

  if (!kp || !kpb) return KATE_E_INVALID_PARAMETER;
  if (kp->ncolors<=0 || kp->ncolors>256) return KATE_E_LIMIT;

  kate_pack_write(kpb,kp->ncolors-1,8);
  for (n=0;n<kp->ncolors;++n) {
    int ret=kate_encode_color(kp->colors+n,kpb);
    if (ret<0) return ret;
  }

  {
    /* bitstream 0.6: add metadata */
    kate_pack_buffer warp;
    kate_open_warp(&warp);
    kate_write_metadata(&warp,kp->meta);
    kate_close_warp(&warp,kpb);
  }


  kate_warp(kpb);

  return 0;
}

static int kate_encode_palettes(kate_state *k,kate_packet *kp)
{
  kate_pack_buffer *kpb;
  kate_info *ki;
  size_t n;
  int ret;

  if (!k || !kp) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  kpb=&k->kes->kpb;

  ret=kate_encode_start_header(kpb,0x86);
  if (ret<0) return ret;

  ki=k->ki;
  if (!ki) return KATE_E_INIT;

  kate_write32v(kpb,ki->npalettes);

  for(n=0;n<ki->npalettes;++n) {
    ret=kate_encode_palette(ki->palettes[n],kpb);
    if (ret<0) return ret;
  }

  kate_warp(kpb);

  return kate_finalize_packet_buffer(kpb,kp,k);
}

#if 0
static int kate_encode_paletted_bitmap(const kate_bitmap *kb,kate_pack_buffer *kpb)
{
  size_t w,h,n;
  unsigned int maxpixel;

  if (kb->bpp>8) return KATE_E_LIMIT;

  kate_write32v(kpb,kb->palette);
  n=0;
  maxpixel=(1<<kb->bpp)-1;
  for (h=0;h<kb->height;++h) {
    for (w=0;w<kb->width;++w) {
      unsigned int pixel=kb->pixels[n++];
      if (pixel>maxpixel) return KATE_E_LIMIT;
      kate_pack_write(kpb,pixel,kb->bpp);
    }
  }

  return 0;
}
#endif

static int kate_encode_rle_bitmap(const kate_bitmap *kb,kate_pack_buffer *kpb)
{
  if (kb->bpp>8) return KATE_E_LIMIT;

  kate_pack_write(kpb,kate_bitmap_type_paletted,8);
  kate_pack_write(kpb,1,8); /* RLE encoding */
  kate_write32v(kpb,kb->bpp);
  kate_write32v(kpb,kb->palette);

  return kate_rle_encode(kb->width,kb->height,kb->pixels,kb->bpp,kpb);
}

static int kate_encode_png_bitmap(const kate_bitmap *kb,kate_pack_buffer *kpb)
{
  kate_pack_write(kpb,kate_bitmap_type_png,8);
  kate_write32(kpb,kb->size);
  kate_writebuf(kpb,(const char*)kb->pixels,kb->size);

  return 0;
}

static int kate_encode_bitmap(const kate_bitmap *kb,kate_pack_buffer *kpb)
{
  int ret;

  if (!kb || !kpb) return KATE_E_INVALID_PARAMETER;

  kate_write32v(kpb,kb->width);
  kate_write32v(kpb,kb->height);
#if 0
  /* paletted bitmaps are now written compressed */
  kate_pack_write(kpb,kb->bpp,8); /* 0 marks a raw bitmap */
#else
  kate_pack_write(kpb,0,8); /* 0 marks a raw bitmap */
#endif

  switch (kb->type) {
    case kate_bitmap_type_paletted:
      ret=kate_encode_rle_bitmap(kb,kpb);
      break;
    case kate_bitmap_type_png:
      ret=kate_encode_png_bitmap(kb,kpb);
      break;
    default:
      ret=KATE_E_INVALID_PARAMETER;
      break;
  }

  if (ret<0) return ret;

  {
    /* bitstream 0.4: x/y offsets */
    kate_pack_buffer warp;
    kate_open_warp(&warp);
    kate_write32v(&warp,kb->x_offset);
    kate_write32v(&warp,kb->y_offset);
    kate_close_warp(&warp,kpb);
  }

  {
    /* bitstream 0.6: add metadata */
    kate_pack_buffer warp;
    kate_open_warp(&warp);
    /* for backward binary compatiblity, old code did not initialize 'meta', but had 'internal' set to 0 */
    kate_write_metadata(&warp,kb->internal?kb->meta:NULL);
    kate_close_warp(&warp,kpb);
  }

  kate_warp(kpb);

  return 0;
}

static int kate_encode_bitmaps(kate_state *k,kate_packet *kp)
{
  kate_pack_buffer *kpb;
  kate_info *ki;
  size_t n;
  int ret;

  if (!k || !kp) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  kpb=&k->kes->kpb;

  ret=kate_encode_start_header(kpb,0x87);
  if (ret<0) return ret;

  ki=k->ki;
  if (!ki) return KATE_E_INIT;

  kate_write32v(kpb,ki->nbitmaps);

  for(n=0;n<ki->nbitmaps;++n) {
    ret=kate_encode_bitmap(ki->bitmaps[n],kpb);
    if (ret<0) return ret;
  }

  kate_warp(kpb);

  return kate_finalize_packet_buffer(kpb,kp,k);
}

static int kate_encode_font_range(const kate_info *ki,const kate_font_range *kfr,kate_pack_buffer *kpb)
{
  if (!ki || !kfr || !kpb) return KATE_E_INVALID_PARAMETER;

  if (!kate_is_valid_code_point(kfr->first_code_point)) return KATE_E_TEXT;
  if (!kate_is_valid_code_point(kfr->last_code_point)) return KATE_E_TEXT;
  if (kfr->first_bitmap<0) return KATE_E_LIMIT;
  if ((size_t)(kfr->first_bitmap+(kfr->last_code_point-kfr->first_code_point))>=ki->nbitmaps) return KATE_E_LIMIT;

  kate_write32v(kpb,kfr->first_code_point);
  kate_write32v(kpb,kfr->last_code_point);
  kate_write32v(kpb,kfr->first_bitmap);
  kate_warp(kpb);

  return 0;
}

static int kate_encode_font_ranges(kate_state *k,kate_packet *kp)
{
  kate_pack_buffer *kpb;
  kate_info *ki;
  size_t n,l;
  int ret;

  if (!k || !kp) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  kpb=&k->kes->kpb;

  ret=kate_encode_start_header(kpb,0x88);
  if (ret<0) return ret;

  ki=k->ki;
  if (!ki) return KATE_E_INIT;

  kate_write32v(kpb,ki->nfont_ranges);
  for(n=0;n<ki->nfont_ranges;++n) {
    ret=kate_encode_font_range(ki,ki->font_ranges[n],kpb);
    if (ret<0) return ret;
  }

  kate_write32v(kpb,ki->nfont_mappings);
  for(n=0;n<ki->nfont_mappings;++n) {
    const kate_font_mapping *kfm=ki->font_mappings[n];
    kate_write32v(kpb,kfm->nranges);
    for (l=0;l<kfm->nranges;++l) {
      const kate_font_range *kfr=kfm->ranges[l];
      int idx=kate_find_font_range(ki,kfr);
      if (idx>=0) {
        kate_pack_write1(kpb,1);
        kate_write32v(kpb,idx);
      }
      else {
        kate_pack_write1(kpb,0);
        ret=kate_encode_font_range(ki,kfr,kpb);
        if (ret<0) return ret;
      }
    }
  }

  kate_warp(kpb);

  return kate_finalize_packet_buffer(kpb,kp,k);
}

static inline int kate_check_granule(kate_state *k,kate_int64_t *granulepos)
{
  if (*granulepos<k->kes->granulepos) return -1;
  return 0;
}

#define WRITE_OVERRIDE(kpb,var,def,write) \
  do \
    if (ret==0 && (kes->overrides.var!=(def))) { \
      kate_pack_write1(kpb,1); \
      write; \
    } \
    else kate_pack_write1(kpb,0); \
  while(0)

static int kate_encode_overrides(kate_state *k,kate_pack_buffer *kpb)
{
  kate_encode_state *kes;
  size_t n;
  int ret=0;

  if (!k || !kpb) return KATE_E_INVALID_PARAMETER;
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
    kate_pack_write1(kpb,1);
    WRITE_OVERRIDE(kpb,text_encoding,k->ki->text_encoding,kate_pack_write(kpb,kes->overrides.text_encoding,8));
    WRITE_OVERRIDE(kpb,text_directionality,k->ki->text_directionality,kate_pack_write(kpb,kes->overrides.text_directionality,8));
    WRITE_OVERRIDE(kpb,language,NULL,
      do {
        size_t len=strlen(kes->overrides.language);
        kate_write32v(kpb,len);
        kate_writebuf(kpb,kes->overrides.language,len);
      } while(0));
    WRITE_OVERRIDE(kpb,region_index,-1,kate_write32v(kpb,kes->overrides.region_index));
    WRITE_OVERRIDE(kpb,region,NULL,ret=kate_encode_region(kes->overrides.region,kpb));
    WRITE_OVERRIDE(kpb,style_index,-1,kate_write32v(kpb,kes->overrides.style_index));
    WRITE_OVERRIDE(kpb,style,NULL,ret=kate_encode_style(kes->overrides.style,kpb));
    WRITE_OVERRIDE(kpb,secondary_style_index,-1,kate_write32v(kpb,kes->overrides.secondary_style_index));
    WRITE_OVERRIDE(kpb,secondary_style,NULL,ret=kate_encode_style(kes->overrides.secondary_style,kpb));
    WRITE_OVERRIDE(kpb,font_mapping_index,-1,kate_write32v(kpb,kes->overrides.font_mapping_index));
  }
  else kate_pack_write1(kpb,0);

  if (ret==0) {
    /* bitstream 0.2: add palette, bitmap, markup type */
    kate_pack_buffer warp;
    kate_open_warp(&warp);
    if (kes->overrides.palette_index>=0
     || kes->overrides.palette
     || kes->overrides.bitmap_index>=0
     || kes->overrides.bitmap
     || kes->overrides.text_markup_type!=k->ki->text_markup_type) {
      kate_pack_write1(&warp,1);
      WRITE_OVERRIDE(&warp,palette_index,-1,kate_write32v(&warp,kes->overrides.palette_index));
      WRITE_OVERRIDE(&warp,palette,NULL,ret=kate_encode_palette(kes->overrides.palette,&warp));
      WRITE_OVERRIDE(&warp,bitmap_index,-1,kate_write32v(&warp,kes->overrides.bitmap_index));
      WRITE_OVERRIDE(&warp,bitmap,NULL,ret=kate_encode_bitmap(kes->overrides.bitmap,&warp));
      WRITE_OVERRIDE(&warp,text_markup_type,k->ki->text_markup_type,kate_pack_write(&warp,kes->overrides.text_markup_type,8));
    }
    else kate_pack_write1(&warp,0);
    kate_close_warp(&warp,kpb);
  }

  if (ret==0) {
    /* bitstream 0.4: add bitmaps */
    kate_pack_buffer warp;
    kate_open_warp(&warp);
    kate_write32v(&warp,kes->nbitmaps);
    for(n=0;n<kes->nbitmaps;++n) {
      if (kes->bitmaps[n]==NULL) {
        /* we have an index into the bitmaps header */
        kate_pack_write1(&warp,1);
        kate_write32v(&warp,kes->bitmap_indices[n]);
      }
      else {
        /* we have a fully defined bitmap */
        kate_pack_write1(&warp,0);
        ret=kate_encode_bitmap(kes->bitmaps[n],&warp);
        if (ret<0) break;
      }
    }
    kate_close_warp(&warp,kpb);
  }

  if (ret==0) {
    /* bitstream 0.6: add metadata */
    kate_pack_buffer warp;
    kate_open_warp(&warp);
    kate_write_metadata(&warp,kes->meta);
    kate_close_warp(&warp,kpb);
  }

  kate_warp(kpb);

  return ret;
}

/**
  \ingroup encoding
  Encodes a text (which may be NULL) of the given size, starting at t0 and ending at t1.
  This should always be called when encoding an event, even if the text is NULL.
  After this is called, the event is fully encoded and cannot be added to anymore.
  \param k the kate_state to add the text to
  \param t0 the start time in granule units of the text
  \param t1 the end time in granule units of the text
  \param text the text to add (may be NULL)
  \param sz the length in bytes of the text to add
  \param kp the packet to encode to
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_text_raw_times(kate_state *k,kate_int64_t t0,kate_int64_t t1,const char *text,size_t sz,kate_packet *kp)
{
  kate_pack_buffer *kpb;
  kate_int64_t start_granulepos;
  kate_int64_t start;
  kate_int64_t duration;
  kate_int64_t backlink;
  kate_int64_t earliest_t;
  int ret;
  size_t n;

  if (!k || !text || !kp) return KATE_E_INVALID_PARAMETER;
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

  start_granulepos=(earliest_t<<k->ki->granule_shift)|(t0-earliest_t);
  if (start_granulepos<0) return KATE_E_BAD_GRANULE;
  if (kate_check_granule(k,&start_granulepos)<0) return KATE_E_BAD_GRANULE;

  start=t0;
  if (start<0) return KATE_E_BAD_GRANULE;
  duration=t1-t0;
  if (duration<0) return KATE_E_BAD_GRANULE;
  backlink=t0-earliest_t;
  if (backlink<0) return KATE_E_BAD_GRANULE;

  kpb=&k->kes->kpb;
  kate_pack_write(kpb,0x00,8);

  kate_write64(kpb,start);
  kate_write64(kpb,duration);
  kate_write64(kpb,backlink);

  kate_write32(kpb,sz);
  kate_writebuf(kpb,text,sz);

  if (k->kes->id>=0) {
    kate_pack_write1(kpb,1);
    kate_write32v(kpb,k->kes->id);
  }
  else {
    kate_pack_write1(kpb,0);
  }

  if (k->kes->nmotions) {
    kate_pack_write1(kpb,1);
    kate_write32v(kpb,k->kes->nmotions);
    for (n=0;n<k->kes->nmotions;++n) {
      if (k->kes->motions[n]==NULL) {
        /* we have an index into the motions header */
        kate_pack_write1(kpb,1);
        kate_write32v(kpb,k->kes->motion_indices[n]);
      }
      else {
        /* we have a fully defined motion */
        kate_pack_write1(kpb,0);
        ret=kate_encode_motion(k->ki,k->kes->motions[n],kpb);
        if (ret<0) return ret;
      }
    }
  }
  else kate_pack_write1(kpb,0);

  kate_encode_overrides(k,kpb);

  if (start_granulepos>k->kes->furthest_granule) k->kes->furthest_granule=start_granulepos;

  k->kes->granulepos=start_granulepos;
  ret=kate_finalize_packet_buffer(kpb,kp,k);
  if (ret<0) return ret;

  /* save the packet in case we have to repeat it */
  ret=kate_encode_state_save_event_buffer(k->kes,kp->nbytes,kp->data);
  if (ret<0) return ret;

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
  \param kp the packet to encode to
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_text(kate_state *k,kate_float t0,kate_float t1,const char *text,size_t sz,kate_packet *kp)
{
  if (!k) return KATE_E_INVALID_PARAMETER;
  return kate_encode_text_raw_times(k,kate_duration_granule(k->ki,t0),kate_duration_granule(k->ki,t1),text,sz,kp);
}

/**
  \ingroup encoding
  Emits a keepalive packet, to help with seeking.
  \param k the kate_state to encode to
  \param t the timestamp (in granule rate units) for the keepalive packet
  \param kp the packet to encode to
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_keepalive_raw_times(kate_state *k,kate_int64_t t,kate_packet *kp)
{
  kate_pack_buffer *kpb;
  kate_int64_t granulepos;
  kate_int64_t earliest_t;
  int ret;

  if (!k || !kp) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;
  if (k->kes->eos) return KATE_E_INIT;

  ret=kate_encode_state_trim_events(k->kes,t);
  if (ret<0) return ret;

  ret=kate_encode_state_get_earliest_event(k->kes,&earliest_t,NULL);
  if (ret==KATE_E_NOT_FOUND) {
    /* if there are no live events yet, base from now */
    earliest_t=t;
    ret=0;
  }
  if (ret<0) return ret;

  granulepos=(earliest_t<<k->ki->granule_shift)|(t-earliest_t);
  if (granulepos<0) return KATE_E_BAD_GRANULE;

  if (kate_check_granule(k,&granulepos)<0) return KATE_E_BAD_GRANULE;
  k->kes->granulepos=granulepos;

  ret=kate_encode_state_add_event(k->kes,t,t);
  if (ret<0) return ret;

  kpb=&k->kes->kpb;
  kate_pack_write(kpb,0x01,8);

  return kate_finalize_packet_buffer(kpb,kp,k);
}

/**
  \ingroup encoding
  Emits a keepalive packet, to help with seeking.
  \param k the kate_state to encode to
  \param t the timestamp for the keepalive packet
  \param kp the packet to encode to
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_keepalive(kate_state *k,kate_float t,kate_packet *kp)
{
  if (!k) return KATE_E_INVALID_PARAMETER;
  return kate_encode_keepalive_raw_times(k,kate_duration_granule(k->ki,t),kp);
}

/**
  \ingroup encoding
  Emits a repeat packet, to help with seeking.
  The first active event at time t, if it or its latest repeat (whichever is
  the latest) was emitted more than threshold ago, of if threshold is zero,
  will have a repeat packet emitted.
  kate_encode_repeat is designed to be called in a loop, as it will return a
  repeat packet for only one event even if more than one event needs a repeat.
  When the returned value is 0, no more events need repeating at time t.
  \param k the kate_state to encode to
  \param t the timestamp (in granule rate units) for the the packets, if any
  \param threshold the time threshold (in granule rate units) at which to emit
         a repeat packets (zero to force a repeat)
  \param kp the packet to encode to
  \returns 0 success, and no repeat packet was encoded
  \returns 1 success, and a repeat packet was encoded
  \returns KATE_E_* error
  */
int kate_encode_repeat_raw_times(kate_state *k,kate_int64_t t,kate_int64_t threshold,kate_packet *kp)
{
  kate_int64_t earliest_t;
  kate_int64_t granulepos;
  int ret;

  if (!k || !kp) return KATE_E_INVALID_PARAMETER;
  if (threshold<0) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;
  if (k->kes->eos) return KATE_E_INIT;

  ret=kate_encode_state_trim_events(k->kes,t);
  if (ret<0) return ret;
  ret=kate_encode_state_get_earliest_event(k->kes,&earliest_t,NULL);
  if (ret==KATE_E_NOT_FOUND) {
    /* if there are no live events yet, base from now */
    earliest_t=t;
  }
  else if (ret<0) {
    return ret;
  }
  granulepos=(earliest_t<<k->ki->granule_shift)|(t-earliest_t);
  if (granulepos<0) return KATE_E_BAD_GRANULE;

  if (kate_check_granule(k,&granulepos)<0) return KATE_E_BAD_GRANULE;

  ret=kate_encode_state_get_repeat(k->kes,t,threshold,kp);
  if (ret>0) {
    /* if we encoded a repeat, update encoding state granpos */
    k->kes->granulepos=granulepos;
  }
  return ret;
}

/**
  \ingroup encoding
  Emits a repeat packet, to help with seeking.
  The first active event at time t, if it or its latest repeat (whichever is
  the latest) was emitted more than threshold ago, of if threshold is zero,
  will have a repeat packet emitted.
  kate_encode_repeat is designed to be called in a loop, as it will return a
  repeat packet for only one event even if more than one event needs a repeat.
  When the returned value is 0, no more events need repeating at time t.
  \param k the kate_state to encode to
  \param t the timestamp for the the packets, if any
  \param threshold the time threshold at which to emit a repeat packets (zero to force a repeat)
  \param kp the packet to encode to
  \returns 0 success, and no repeat packet was encoded
  \returns 1 success, and a repeat packet was encoded
  \returns KATE_E_* error
  */
int kate_encode_repeat(kate_state *k,kate_float t,kate_float threshold,kate_packet *kp)
{
  if (!k) return KATE_E_INVALID_PARAMETER;
  return kate_encode_repeat_raw_times(k,kate_duration_granule(k->ki,t),kate_duration_granule(k->ki,threshold),kp);
}

/**
  \ingroup encoding
  Finalizes the currently encoded stream.
  No more events may be added after this is called.
  \param k the kate_state to encode to
  \param t the timestamp (in granule rate units) for the end (if negative,
           the end time of the last event will be used)
  \param kp the packet to encode to
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_finish_raw_times(kate_state *k,kate_int64_t t,kate_packet *kp)
{
  kate_pack_buffer *kpb;
  kate_int64_t granulepos;
  int ret;

  if (!k || !kp) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;
  if (k->kes->eos) return KATE_E_INIT;

  ret=kate_encode_state_trim_events(k->kes,t);
  if (ret<0) return ret;

  if (t<0) {
    ret=kate_encode_state_get_latest_event(k->kes,NULL,&t);
    if (ret==KATE_E_NOT_FOUND) {
      /* if nothing was encoded, it is still a valid stream */
      t=0;
      ret=0;
    }
    if (ret<0) return ret;
  }

  granulepos=t<<k->ki->granule_shift;
  if (granulepos<0) return KATE_E_BAD_GRANULE;

  if (kate_check_granule(k,&granulepos)<0) return KATE_E_BAD_GRANULE;
  k->kes->granulepos=granulepos;

  kpb=&k->kes->kpb;
  kate_pack_write(kpb,0x7f,8);

  k->kes->eos=1;

  return kate_finalize_packet_buffer(kpb,kp,k);
}

/**
  \ingroup encoding
  Finalizes the currently encoded stream.
  No more events may be added after this is called.
  \param k the kate_state to encode to
  \param t the timestamp for the end (if negative, the end time of the last event will be used)
  \param kp the packet to encode to
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_finish(kate_state *k,kate_float t,kate_packet *kp)
{
  if (!k) return KATE_E_INVALID_PARAMETER;
  return kate_encode_finish_raw_times(k,kate_duration_granule(k->ki,t),kp);
}

/**
  \ingroup encoding
  Encodes a header.
  This should be repeatedly called at the beginning of encoding, until a
  positive value is returned, marking the encoding of the last header.
  \param k the kate_state to encode to
  \param kc the list of comments to add to the headers
  \param kp the packet to encode to
  \returns 0 success
  \returns 1 success, and all headers have been encoded
  \returns KATE_E_* error
  */
int kate_encode_headers(kate_state *k,kate_comment *kc,kate_packet *kp)
{
  if (!k || !kc || !kp) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;
  if (k->kes->eos) return KATE_E_INIT;

  switch (k->kes->packetno+1) {
    case 0: return kate_encode_info(k,kp);
    case 1: return kate_encode_comment(k,kc,kp);
    case 2: return kate_encode_regions(k,kp);
    case 3: return kate_encode_styles(k,kp);
    case 4: return kate_encode_curves(k,kp);
    case 5: return kate_encode_motions(k,kp);
    case 6: return kate_encode_palettes(k,kp);
    case 7: return kate_encode_bitmaps(k,kp);
    case 8: return kate_encode_font_ranges(k,kp);
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
  Adds a bitmap to the currently encoded event.
  \param k the kate_state to add the bitmap to
  \param kb the bitmap to add
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_add_bitmap(kate_state *k,const kate_bitmap *kb)
{
  if (!k || !kb) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  return kate_encode_state_add_bitmap(k->kes,kb);
}

/**
  \ingroup encoding
  Adds a bitmap to the currently encoded event by its index into the list of predefined bitmaps.
  \param k the kate_state to add the bitmap to
  \param bitmap the index of the bitmap to add
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_add_bitmap_index(kate_state *k,size_t bitmap)
{
  if (!k) return KATE_E_INVALID_PARAMETER;
  if (!k->ki) return KATE_E_INIT;
  if (bitmap>=k->ki->nbitmaps) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  return kate_encode_state_add_bitmap_index(k->kes,bitmap);
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
  This function is obsolete and should not be used. An event identifier is automatically
  generated for each event.
  It used to set a unique identifier for the currently encoded event, so it can be referred
  to later, but not does nothing.
  \param k the kate_state to encode to
  \param id unused
  \returns KATE_E_* error
  */
int kate_encode_set_id(kate_state *k,kate_int32_t id)
{
  if (!k) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  (void)id;

  return KATE_E_IMPL;
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

/**
  \ingroup encoding
  Sets the markup type of this event
  \param k the kate_state to encode to
  \param text_markup_type the markup type of this event
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_set_markup_type(kate_state *k,int text_markup_type)
{
  if (!k) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  k->kes->overrides.text_markup_type=text_markup_type;

  return 0;
}

/**
  \ingroup encoding
  Adds a set of metadata to this event
  \param k the kate_state to encode to
  \param km the metadata to add (a reference is taken, km must be live until the event is encoded)
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_add_meta(kate_state *k,const kate_meta *km)
{
  if (!k || !km) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  return kate_encode_state_add_meta(k->kes,km);
}

/**
  \ingroup encoding
  Adds a set of metadata to this event
  \param k the kate_state to encode to
  \param km the metadata to add (will become invalid after this call returns successfully)
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_encode_merge_meta(kate_state *k,kate_meta *km)
{
  if (!k || !km) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  return kate_encode_state_merge_meta(k->kes,km);
}

/**
  \ingroup encoding
  Retrieves the current granulepos
  \param k the kate_state to encode to
  \returns >=0 the current granulepos (the one of the last encoded packet)
  \returns KATE_E_* error
  */
kate_int64_t kate_encode_get_granule(const kate_state *k)
{
  if (!k) return KATE_E_INVALID_PARAMETER;
  if (!k->kes) return KATE_E_INIT;

  return k->kes->granulepos;
}

