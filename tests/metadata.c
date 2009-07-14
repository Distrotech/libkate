/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#include <stdio.h>
#include <string.h>
#include "kate/kate.h"

static const struct meta_op {
  const char *op;
  const char *tag;
  const char *value;
  size_t len;
  int idx;
  int expected_return_code;
} meta_ops[]={
  { "count", NULL, "", -1, 0, 0 },
  { "adds", "", "", -1, -1, KATE_E_INVALID_PARAMETER },
  { "adds", "\xff", "", -1, -1, KATE_E_BAD_TAG },
  { "adds", "raw", "\xff", -1, -1, KATE_E_TEXT },
  { "addr", "raw", "\xff", 1, -1, 0 },
  { "adds", "foo", "bar", -1, -1, 0 },
  { "count", NULL, "", -1, 0, 2 },
  { "remove", "foo", "bar", -1, -1, KATE_E_INVALID_PARAMETER },
  { "remove", "foo", "bar", -1, 1, KATE_E_INVALID_PARAMETER },
  { "remove", "foo", "bar", -1, 0, 0 },
  { "remove", "foo", "bar", -1, 0, KATE_E_INVALID_PARAMETER },
  { "count", NULL, "", -1, 0, 1 },
  { "count", "foo", "", -1, 0, 0 },
  { "adds", "foo", "bar", -1, -1, 0 },
  { "count", "foo", "", -1, 0, 1 },
  { "count", "bar", "", -1, 0, 0 },
  { "count", NULL, "", -1, 0, 2 },
  { "remove", "raw", "", -1, 0, 0 },
  { "count", NULL, "", -1, 0, 1 },
  { "adds", "bar", "baz", -1, -1, 0 },
  { "remove", NULL, "", -1, 1, 0 },
  { "remove", NULL, "", -1, 0, 0 },
  { "count", NULL, "", -1, 0, 0 },
};

int main()
{
  size_t i,count;
  kate_meta *km;
  int ret;

  ret=kate_meta_create(&km);
  if (ret<0) {
    fprintf(stderr,"kate_meta_init failed: %d\n",ret);
    return ret;
  }

  for (i=0;i<sizeof(meta_ops)/sizeof(meta_ops[0]);++i) {
    const struct meta_op *op=meta_ops+i;
    if (!strcmp(op->op,"adds")) {
      ret=kate_meta_add_string(km,op->tag,op->value);
    }
    else if (!strcmp(op->op,"addr")) {
      ret=kate_meta_add(km,op->tag,op->value,op->len);
    }
    else if (!strcmp(op->op,"remove")) {
      ret=op->tag?kate_meta_remove_tag(km,op->tag,op->idx):kate_meta_remove(km,op->idx);
    }
    else if (!strcmp(op->op,"count")) {
      ret=op->tag?kate_meta_query_tag_count(km,op->tag):kate_meta_query_count(km);
    }
    else {
      fprintf(stderr,"error: unknown op: %s\n",op->op);
      return -1;
    }
    if (ret!=op->expected_return_code) {
      fprintf(stderr,"error in op %zu: %d different from expected %d\n",i,ret,op->expected_return_code);
      return -1;
    }
  }

  ret=kate_meta_destroy(km);
  if (ret<0) {
    fprintf(stderr,"kate_meta_clear failed: %d\n",ret);
    return ret;
  }

  return 0;
}

