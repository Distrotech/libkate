/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#define KATE_INTERNAL
#include "kate_internal.h"

#include "kate/kate.h"

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

