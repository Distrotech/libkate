/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#include <stdio.h>
#include <string.h>
#include "kate/kate.h"

static const struct {
  const char * const tag;
  const char *expected_tag;
  int expected_return_code;
} langs_trunc[]={
  { "", "", 0 },
  { "en", "en", 0 },
  { "_", NULL, KATE_E_INVALID_PARAMETER },
  { "__", NULL, KATE_E_INVALID_PARAMETER },
  { "___", NULL, KATE_E_INVALID_PARAMETER },
  { "_a_", NULL, KATE_E_INVALID_PARAMETER },
  { "_aa_", NULL, KATE_E_INVALID_PARAMETER },
  { "_blah", NULL, KATE_E_INVALID_PARAMETER },
  { "morethan3", NULL, KATE_E_INVALID_PARAMETER },
  { "en_", NULL, KATE_E_INVALID_PARAMETER },
  { "en_a", NULL, KATE_E_INVALID_PARAMETER },
  { "a_", NULL, KATE_E_INVALID_PARAMETER },
  { "a_EN", "a_EN", 0 },
  { "en_GB", "en_GB", 0 },
  { "i-navajo", "i-navajo", 0 },
  { "a", NULL, KATE_E_INVALID_PARAMETER },
  { "_en_GB", NULL, KATE_E_INVALID_PARAMETER },
  { "en_GB_gb_GB-gb-gb-GB", "en_GB_gb_GB-gb", 1 },
  { "0123456789abcdef", NULL, KATE_E_INVALID_PARAMETER },
  { "en_some_very_long_string", "en_some_very", 1 },
  { "en_GB_a_a_foobar", "en_GB", 1},
  { "a_a-a-a-a-a-a-a-a-a-a-a--a", NULL, KATE_E_INVALID_PARAMETER },
  { "en_GB ", NULL, KATE_E_INVALID_PARAMETER },
  { " en_GB", NULL, KATE_E_INVALID_PARAMETER },
  { " ", NULL, KATE_E_INVALID_PARAMETER },
  { "en_a12345678_GB", NULL, KATE_E_INVALID_PARAMETER },
  { "en_GB_GB_GB_RGB", "en_GB_GB_GB_RGB", 0 },
};

static const struct {
  const char * const tag1;
  const char * const tag2;
  int expected_return_code;
} langs_cmp[]={
  { "", "", 2 },
  { "", "en", 2 },
  { "en", "", 2 },
  { "en_GB", "en_US", 2 },
  { "en", "en_US", 2 },
  { "en_GB", "en", 2 },
  { "en", "de", 0 },
};

int main()
{
  size_t i;
  kate_info ki;
  int ret;

  ret=kate_info_init(&ki);
  if (ret<0) {
    fprintf(stderr,"kate_info_init failed: %d\n",ret);
    return ret;
  }

  for (i=0;i<sizeof(langs_trunc)/sizeof(langs_trunc[0]);++i) {
    /* printf("setting %s\n",langs_trunc[i].tag); */
    ret=kate_info_set_language(&ki,langs_trunc[i].tag);
    if (ret!=langs_trunc[i].expected_return_code) {
      fprintf(stderr,"error: %d different from expected %d\n",ret,langs_trunc[i].expected_return_code);
      return -1;
    }
    else if (ret>=0 && strcasecmp(ki.language,langs_trunc[i].expected_tag)) {
      fprintf(stderr,"error: %s different from expected %s\n",ki.language,langs_trunc[i].expected_tag);
      return -1;
    }
  }

  for (i=0;i<sizeof(langs_cmp)/sizeof(langs_cmp[0]);++i) {
    ret=kate_info_set_language(&ki,langs_cmp[i].tag1);
    if (ret<0) {
      fprintf(stderr,"error: kate_info_set_language failed: %d\n",ret);
      return ret;
    }
    ret=kate_info_matches_language(&ki,langs_cmp[i].tag2);
    if (ret!=langs_cmp[i].expected_return_code) {
      fprintf(stderr,"error: %d different from expected %d\n",ret,langs_cmp[i].expected_return_code);
      return -1;
    }
  }

  ret=kate_info_clear(&ki);
  if (ret<0) {
    fprintf(stderr,"kate_info_clear failed: %d\n",ret);
    return ret;
  }

  return 0;
}

