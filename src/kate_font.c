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

static int kate_font_check_overlap(const kate_font_range *kfr0,const kate_font_range *kfr1)
{
  if (!kfr0 || !kfr1) return KATE_E_INVALID_PARAMETER;

  if (kfr0->last_code_point<kfr1->first_code_point) return 0;
  if (kfr1->last_code_point<kfr0->first_code_point) return 0;

  return KATE_E_INIT;
}

int kate_font_check_ranges(const kate_font_mapping *kfm)
{
  size_t n,l;

  if (!kfm) return KATE_E_INVALID_PARAMETER;

  for (n=0;n<kfm->nranges;++n) {
    const kate_font_range *kfr=kfm->ranges[n];
    if (!kfr) return KATE_E_INIT;
    if (kfr->last_code_point<kfr->first_code_point) return KATE_E_INIT;
    for (l=n+1;l<kfm->nranges;++l) {
      int ret=kate_font_check_overlap(kfr,kfm->ranges[l]);
      if (ret<0) return ret;
    }
  }

  return 0;
}

/**
  \ingroup font
  Returns the index of the bitmap which corresponds to the given code point, if any.
  \param kfm the font mapping to search
  \param c the code point to search for
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_font_get_index_from_code_point(const kate_font_mapping *kfm,int c)
{
  size_t n;

  if (!kfm) return KATE_E_INVALID_PARAMETER;
  if (!kate_is_valid_code_point(c)) return KATE_E_TEXT;

  for (n=0;n<kfm->nranges;++n) {
    const kate_font_range *kfr=kfm->ranges[n];
    if (c>=kfr->first_code_point && c<=kfr->last_code_point) {
      return kfr->first_bitmap+c-kfr->first_code_point;
    }
  }

  /* not found */
  return KATE_E_NOT_FOUND;
}

