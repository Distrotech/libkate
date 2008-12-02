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
#include <stdarg.h>
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <errno.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <ctype.h>
#include <ogg/ogg.h>
#include "kate/kate.h"
#include "kstream.h"

static int is_ok_for_filename(const char *s)
{
  int c;
  while ((c=*s++)) {
    if (isalnum(c)) continue;
    if (strchr("-_",c)) continue;
    return 0;
  }
  return 1;
}

static void fcats(char **ptr,const char *s,size_t s_len)
{
  if (s && *s) {
    size_t ptr_len=(*ptr)?strlen(*ptr):0;
    *ptr=(char*)kate_realloc(*ptr,ptr_len+s_len+1);
    memcpy((*ptr)+ptr_len,s,s_len);
    (*ptr)[ptr_len+s_len]=0;
  }
}

static void fcat(char **ptr,const char *s)
{
  if (s && *s) {
    fcats(ptr,s,strlen(s));
  }
}

static int is_filename_used(const char *filename,const kate_stream *streams,size_t nstreams)
{
  size_t n;
  for (n=0;n<nstreams;++n) {
    const kate_stream *ks=streams+n;
    if (ks->filename && !strcmp(filename,ks->filename)) return 1;
  }
  return 0;
}

char *get_filename(const char *basename,const kate_stream *ks,const kate_stream *streams,size_t nstreams)
{
  char tmp[32];
  char *filename=NULL;
  const char *ptr=basename;

  if (!basename) return NULL;
  while (*ptr) {
    const char *percent=strchr(ptr,'%');
    if (!percent) {
      fcat(&filename,ptr);
      break;
    }
    if (percent>ptr) {
      fcats(&filename,ptr,percent-ptr);
    }
    ptr=percent+1;
    if (!*ptr) {
      fprintf(stderr,"absent format tag in filename\n");
      exit(-1);
    }
    switch (*ptr) {
      case '%':
        fcats(&filename,"%",1);
        break;
      case 'c':
        if (!ks->ki.category) {
          fcat(&filename,"-");
        }
        else if (!is_ok_for_filename(ks->ki.category)) {
          fprintf(stderr,"Category '%s' not suitable for using in a filename, using 'INVCAT'",ks->ki.category);
          fcat(&filename,"INVCAT");
        }
        else {
          fcat(&filename,ks->ki.category);
        }
        break;
      case 'l':
        if (!ks->ki.language) {
          fcat(&filename,"-");
        }
        else if (!is_ok_for_filename(ks->ki.language)) {
          fprintf(stderr,"Language '%s' not suitable for using in a filename, using 'INVLANG'",ks->ki.language);
          fcat(&filename,"INVLANG");
        }
        else {
          fcat(&filename,ks->ki.language);
        }
        break;
      case 's':
        snprintf(tmp,sizeof(tmp),"%08x",(ogg_uint32_t)(ks?ks->os.serialno:0));
        fcat(&filename,tmp);
        break;
      case 'i':
        snprintf(tmp,sizeof(tmp),"%d",ks?ks->stream_index:0);
        fcat(&filename,tmp);
        break;
      default:
        fprintf(stderr,"unknown format tag in filename: %c\n",*ptr);
        exit(-1);
        break;
    }
    ++ptr;
  }

  if (is_filename_used(filename,streams,nstreams)) {
    kate_free(filename);
    return NULL;
  }

  return filename;
}

int kstream_init(kate_stream *ks,ogg_page *og,int stream_index)
{
  int ret;

  ogg_stream_init(&ks->os,ogg_page_serialno(og));
  ret=kate_info_init(&ks->ki);
  if (ret<0) {
    fprintf(stderr,"failed to init info\n");
    ogg_stream_clear(&ks->os);
    return ret;
  }
  kate_info_no_limits(&ks->ki,1);
  ret=kate_comment_init(&ks->kc);
  if (ret<0) {
    fprintf(stderr,"failed to init comments\n");
    ogg_stream_clear(&ks->os);
    return ret;
  }
  ks->init=kstream_header_info;
  ks->filename=NULL;
  ks->fout=NULL;
  ks->stream_index=stream_index;
  ks->event_index=0;
  ks->ret=0;

  return 0;
}

int kstream_clear(kate_stream *ks)
{
  int ret;

  ogg_stream_clear(&ks->os);
  if (ks->init==kstream_data) {
    ret=kate_clear(&ks->k);
    if (ret<0) {
      fprintf(stderr,"kate_clear failed: %d\n",ret);
      /* continue anyway */
    }
  }
  ret=kate_info_clear(&ks->ki);
  if (ret<0) {
    fprintf(stderr,"kate_info_clear failed: %d\n",ret);
    /* continue anyway */
  }
  ret=kate_comment_clear(&ks->kc);
  if (ret<0) {
    fprintf(stderr,"kate_comment_clear failed: %d\n",ret);
    /* continue anyway */
  }
  if (ks->fout && ks->fout!=stdout) {
    ret=fclose(ks->fout);
    if (ret<0) {
      fprintf(stderr,"fclose failed (%d) - file %s might be corrupted\n",ret,ks->filename);
    }
    if (ks->ret<0) {
      ret=unlink(ks->filename);
      if (ret<0) {
        fprintf(stderr,"unlink failed (%d) - corrupt file %s will not be deleted\n",ret,ks->filename);
      }
    }
    kate_free(ks->filename);
  }

  return 0;
}

