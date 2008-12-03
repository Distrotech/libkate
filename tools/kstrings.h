/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#ifndef KATE_kstrings_h_GUARD
#define KATE_kstrings_h_GUARD

#include "kate/kate.h"

extern const char *halign2text(kate_float d);
extern const char *valign2text(kate_float d);
extern const char *metric2text(kate_space_metric d);
extern const char *metric2suffix(kate_space_metric d);
extern const char *curve2text(kate_curve_type d);
extern const char *semantics2text(kate_motion_semantics d);
extern const char *mapping2text(kate_motion_mapping d);
extern const char *directionality2text(kate_text_directionality d);
extern const char *wrap2text(kate_wrap_mode w);
extern const char *encoding2text(kate_text_encoding text_encoding);

#endif

