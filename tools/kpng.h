/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#ifndef _KATE_ENCODER_PNG_H_
#define _KATE_ENCODER_PNG_H_

extern int kd_read_png8(const char *filename,int *w,int *h,int *bpp,kate_color **palette,int *ncolors,unsigned char **pixels);
extern int kd_read_png(const char *filename,int *w,int *h,unsigned char **pixels,size_t *size);

#endif

