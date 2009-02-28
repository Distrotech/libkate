/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kate/kate.h"
#include "../src/kate_internal.h"

#define CHKOVF(test,f) \
  do { \
    size_t res; \
    int ret=f(test.x,test.y,&res); \
    if (test.ret!=ret) { \
      fprintf(stderr,"Failure: " #f " %zu %zu returned %d, expected %d\n",test.x,test.y,ret,test.ret); \
      exit(1); \
    } \
    if (!ret && test.res!=res) { \
      fprintf(stderr,"Failure: " #f " %zu %zu yields %zu, expected %zu\n",test.x,test.y,res,test.res); \
      exit(1); \
    } \
  } while(0)

#define CHKADDOVF(test) CHKOVF(test,kate_check_add_overflow)
#define CHKMULOVF(test) CHKOVF(test,kate_check_mul_overflow)
#define CHKMULGOVF(test) CHKOVF(test,kate_check_mul_overflow_generic)

static const struct {
  size_t x,y;
  int ret;
  size_t res;
} add_tests[]={
  {0,0,0,0},
  {0,(size_t)-1,0,(size_t)-1},
  {1,(size_t)-1,KATE_E_LIMIT,0},
  {2,(size_t)-2,KATE_E_LIMIT,0},
  {((size_t)-1)/2,((size_t)-1)/2+1,0,(size_t)-1},
};

static const struct {
  size_t x,y;
  int ret;
  size_t res;
} mul_tests[]={
  {0,0,0,0},
  {0,(size_t)-1,0,0},
  {(size_t)-1,0,0,0},
  {(size_t)-1,(size_t)-1,KATE_E_LIMIT,0},
  {(size_t)-1,1,0,(size_t)-1},
  {1,(size_t)-1,0,(size_t)-1},
  {((size_t)-1)/2,2,0,((size_t)-1)&~1},
  {((size_t)-1)/2+1,2,KATE_E_LIMIT,0},
};

int main()
{
  size_t n;
  for (n=0;n<sizeof(add_tests)/sizeof(add_tests[0]);++n) {
    CHKADDOVF(add_tests[n]);
  }
  for (n=0;n<sizeof(mul_tests)/sizeof(mul_tests[0]);++n) {
    CHKMULOVF(mul_tests[n]);
    CHKMULGOVF(mul_tests[n]);
  }
  return 0;
}

