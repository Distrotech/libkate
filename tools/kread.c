/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#if defined WIN32 || defined _WIN32 || defined MSDOS || defined __CYGWIN__ || defined __EMX__ || defined OS2
#include <io.h>
#include <fcntl.h>
#endif
#if defined WIN32 || defined _WIN32
#include <process.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <errno.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <ogg/ogg.h>
#include "kate/oggkate.h"
#include "kate_internal.h"
#include "ksrt.h"
#include "klrc.h"
#include "kkate.h"
#include "kfuzz.h"
#include "kstream.h"
#include "kutil.h"

static int read_raw_packet(FILE *f,char **buffer,ogg_int64_t bytes)
{
  size_t ret;

  *buffer=(char*)kate_realloc(*buffer,bytes);
  if (!*buffer) return -1;

  ret=fread(*buffer,1,bytes,f);
  if (ret<(size_t)bytes) return -1;
  return 0;
}

int read_raw_size_and_packet(FILE *f,char **buffer,ogg_int64_t *bytes)
{
  size_t bytes_read;
  int ret;

  /* all subsequent packets are prefixed with 64 bits (signed) of the packet length in bytes */
  bytes_read=fread(bytes,1,8,f);
  if (bytes_read!=8 || *bytes<=0) {
    fprintf(stderr,"failed to read raw kate packet size (read %zu, bytes %lld)\n",bytes_read,(long long)*bytes);
    return -1;
  }
  ret=read_raw_packet(f,buffer,*bytes);
  if (ret<0) {
    fprintf(stderr,"failed to read raw kate packet (%lld bytes)\n",(long long)*bytes);
    return -1;
  }

  return 0;
}

FILE *open_and_probe_stream(const char *filename)
{
  FILE *fin;

  if (!filename || !strcmp(filename,"-")) {
#if defined WIN32 || defined _WIN32
    _setmode(_fileno(stdin),_O_BINARY);
#else
#if defined MSDOS || defined __CYGWIN__ || defined __EMX__ || defined OS2 || defined __BORLANDC__
    setmode(fileno(stdin),_O_BINARY);
#endif
#endif
    fin=stdin;
  }
  else {
    fin=fopen(filename,"rb");
    if (!fin) {
      fprintf(stderr,"%s: %s\n",filename,strerror(errno));
      return NULL;
    }
  }

  return fin;
}

