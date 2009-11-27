/* Copyright (C) 2009 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int get_random(int n)
{
  return (rand()>>8)%n;
}

static int time_total_milliseconds(float t)
{
    return (int)(t*1000.0f+0.5f);
}

static int time_hours(float t)
{
  return time_total_milliseconds(t)/(60*60*1000);
}

static int time_minutes(float t)
{
  return time_total_milliseconds(t)/(60*1000)%60;
}

static int time_seconds(float t)
{
  return time_total_milliseconds(t)/1000%60;
}

static int time_milliseconds(float t)
{
  return time_total_milliseconds(t)%1000;
}

static void gen_time(float *t,float dt)
{
  float r=*t+(rand()/(float)RAND_MAX)*dt;
  *t+=dt;
  printf("%02d:%02d:%02d,%03d",time_hours(r),time_minutes(r),time_seconds(r),time_milliseconds(r));
}

static void gen_text()
{
  int line,nlines=1+get_random(5);
  for (line=0;line<nlines;++line) {
    int n,nchars=1+get_random(128);
    for (n=0;n<nchars;++n) {
      int c=26+get_random(100);
      if ((n==0 && c==' ') || strchr("\r\n",c)) ++c;
      printf("%c",c);
    }
    printf("\n");
  }
}

int main(int argc,char **argv)
{
  int item,items;
  float t=0.0f;

  if (argc==1 || *argv[1]!='-') {
    srand(argc==1?time(NULL):atoi(argv[1]));

    items=argc==1?262144:get_random(256);
    for (item=1;item<=items;++item) {
      printf("%d\n",item);
      gen_time(&t,0.5f);
      printf(" --> ");
      gen_time(&t,2.5f);
      printf("\n");
      gen_text();
      printf("\n");
    }
  }
  else {
    for (item=0;item<=1000;++item) {
      printf("%d\n",item+1);
      t=(float)item/1000.0f;
      gen_time(&t,0.0f);
      printf(" --> ");
      t+=2.5f;
      gen_time(&t,0.0f);
      printf("\n");
      printf("Text.\n");
      printf("\n");
    }
  }

  return 0;
}

