/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


/* adapted from example.c from the libpng distribution */

#include "config.h"

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <errno.h>
#include <png.h>
#include "kate/kate.h"

static FILE *kd_open(const char *filename)
{
  FILE *f;
  size_t bytes;
  unsigned char magic[8];

  if (!filename) {
    fprintf(stderr,"No filename specified\n");
    return NULL;
  }

  f=fopen(filename,"rb");
  if (!f) {
    fprintf(stderr,"Failed to open %s: %s\n",filename,strerror(errno));
    return NULL;
  }

  bytes=fread(magic,1,8,f);
  if (bytes<8) {
    fprintf(stderr,"Failed to read signature bytes from file\n");
    goto error;
  }
  if (png_sig_cmp(magic,0,8)) {
    fprintf(stderr,"Signature bytes do not match PNG\n");
    goto error;
  }

  return f;

error:
  fclose(f);
  return NULL;
}

int kd_read_png8(const char *filename,int *w,int *h,int *bpp,kate_color **palette,int *ncolors,unsigned char **pixels)
{
  FILE *f=NULL;
  png_structp png_ptr=NULL;
  png_infop info_ptr=NULL;
  int cdepth,ctype;
  int channels;
  int width,height;
  png_bytep *row_pointers=NULL;
  int transforms=PNG_TRANSFORM_STRIP_16;
  png_colorp png_palette;
  int x,y,n,num_palette;
  png_bytep trans;
  int num_trans;
  int has_trans;

  f=kd_open(filename);
  if (!f) return -1;

  png_ptr=png_create_read_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
  if (!png_ptr) {
    fprintf(stderr,"Cannot created PNG read structure\n");
    goto error;
  }

  info_ptr=png_create_info_struct(png_ptr);
  if (!info_ptr) {
    fprintf(stderr,"Cannot created PNG info structure\n");
    goto error;
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    fprintf(stderr,"PNG read failed\n");
    goto error;
  }

  png_init_io(png_ptr,f);
  png_set_sig_bytes(png_ptr,8);
  png_read_png(png_ptr,info_ptr,transforms,NULL);

  channels=png_get_channels(png_ptr,info_ptr);
  if (channels!=1) {
    fprintf(stderr,"Channels is not 1 (%d)",channels);
    goto error;
  }

  ctype=png_get_color_type(png_ptr,info_ptr);
  if (!(ctype&PNG_COLOR_MASK_PALETTE)) {
    fprintf(stderr,"Image is not paletted\n");
    goto error;
  }
  if (!(ctype&PNG_COLOR_MASK_COLOR)) {
    fprintf(stderr,"Image does not contain color information\n");
    goto error;
  }

  cdepth=png_get_bit_depth(png_ptr,info_ptr);
  if (cdepth>8) {
    fprintf(stderr,"Color depth is higher than 8\n");
    goto error;
  }

  width=png_get_image_width(png_ptr,info_ptr);
  height=png_get_image_height(png_ptr,info_ptr);

  if (!png_get_PLTE(png_ptr,info_ptr,&png_palette,&num_palette)) {
    fprintf(stderr,"Failed to retrieve palette data\n");
    goto error;
  }

  /* this may or may not be there */
  has_trans=(png_get_tRNS(png_ptr,info_ptr,&trans,&num_trans,NULL)&PNG_INFO_tRNS);

  row_pointers=png_get_rows(png_ptr,info_ptr);
  if (!row_pointers) {
    fprintf(stderr,"Failed to retrieve image data\n");
    goto error;
  }

  /* allocate memory */
  if (palette) {
    *palette=(kate_color*)kate_malloc(num_palette*sizeof(kate_color));
    if (!*palette) {
      fprintf(stderr,"Not enough memory to allocate space for palette (%d colors)\n",num_palette);
      goto error;
    }
  }
  if (pixels) {
    *pixels=(unsigned char*)kate_malloc(width*height);
    if (!*pixels) {
      fprintf(stderr,"Not enough memory to allocate space for pixels (%dx%d)\n",width,height);
      if (palette) kate_free(*palette);
      goto error;
    }
  }

  /* fill data */
  if (w) *w=width;
  if (h) *h=height;
  if (bpp) *bpp=cdepth;
  if (palette) {
    for (n=0;n<num_palette;++n) {
      (*palette)[n].r=png_palette[n].red;
      (*palette)[n].g=png_palette[n].green;
      (*palette)[n].b=png_palette[n].blue;
      (*palette)[n].a=(has_trans && n<num_trans)?trans[n]:255;
    }
  }
  if (ncolors) *ncolors=num_palette;
  if (pixels) {
    /* the PNG_TRANSFORM_PACKING and PNG_TRANSFORM_PACKSWAP bits don't seem to do anything to my 4 bit images,
       so do it by hand */
    int ppb=8/cdepth;
    int mask=(1<<cdepth)-1;
    n=0;
    for (y=0;y<height;++y) {
      unsigned char *row=row_pointers[y];
      for (x=0;x<width;++x) {
        (*pixels)[n++]=(row[x/ppb]>>((ppb-1-x%ppb)*cdepth))&mask;
      }
    }
  }

  png_destroy_read_struct(&png_ptr,&info_ptr,NULL);
  fclose(f);

  return 0;

error:
  if (png_ptr) png_destroy_read_struct(&png_ptr,info_ptr?&info_ptr:NULL,NULL);
  if (f) fclose(f);
  return -1;
}

int kd_read_png(const char *filename,int *w,int *h,unsigned char **pixels,size_t *size)
{
  FILE *f;
  png_structp png_ptr=NULL;
  png_infop info_ptr=NULL;
  long sz;

  f=kd_open(filename);
  if (!f) return -1;

  png_ptr=png_create_read_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
  if (!png_ptr) {
    fprintf(stderr,"Cannot created PNG read structure\n");
    goto error;
  }

  info_ptr=png_create_info_struct(png_ptr);
  if (!info_ptr) {
    fprintf(stderr,"Cannot created PNG info structure\n");
    goto error;
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    fprintf(stderr,"PNG read failed\n");
    goto error;
  }

  png_init_io(png_ptr,f);
  png_set_sig_bytes(png_ptr,8);
  png_read_info(png_ptr,info_ptr);

  if (w) *w=png_get_image_width(png_ptr,info_ptr);
  if (h) *h=png_get_image_height(png_ptr,info_ptr);

  png_destroy_read_struct(&png_ptr,&info_ptr,NULL);

  /* now read the whole file as a binary blob */
  fseek(f,0,SEEK_END);
  sz=ftell(f);
  fseek(f,0,SEEK_SET);
  *pixels=malloc(sz);
  if (!*pixels) goto error;
  if ((long)fread(*pixels,1,sz,f)!=sz) {
    fprintf(stderr,"Failed to read %ld bytes\n",sz);
    free(*pixels);
    *pixels=NULL;
    goto error;
  }

  if (size) *size=sz;

  fclose(f);

  return 0;

error:
  if (png_ptr) png_destroy_read_struct(&png_ptr,info_ptr?&info_ptr:NULL,NULL);
  if (f) fclose(f);
  return -1;
}

