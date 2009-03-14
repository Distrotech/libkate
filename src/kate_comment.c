/* Copyright (C) 2008, 2009 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#define KATE_INTERNAL
#include "kate_internal.h"

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include "kate/kate.h"

/**
  \ingroup comments
  Initializes a kate_comment structure.
  \param kc the structure to initialize
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_comment_init(kate_comment *kc)
{
  if (!kc) return KATE_E_INVALID_PARAMETER;

  kc->user_comments=NULL;
  kc->comment_lengths=NULL;
  kc->comments=0;
  kc->vendor=NULL;

  return 0;
}

/**
  \ingroup comments
  Destroys a kate_comment structure, it must be initialized again before being used.
  \param kc the structure to clear
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_comment_clear(kate_comment *kc)
{
  int n;

  if (!kc) return KATE_E_INVALID_PARAMETER;

  for (n=0;n<kc->comments;++n) kate_free(kc->user_comments[n]);
  if (kc->user_comments) kate_free(kc->user_comments);
  if (kc->comment_lengths) kate_free(kc->comment_lengths);
  if (kc->vendor) kate_free(kc->vendor);

  return 0;
}

static int kate_comment_check_tag(const char *tag,size_t len)
{
  if (!tag) return KATE_E_INVALID_PARAMETER;

  if (len==0) return KATE_E_BAD_TAG;
  while (len--) {
    int c=*tag++;
    if (c<0x20 || c>0x7d || c=='=') return KATE_E_BAD_TAG;
  }
  return 0;
}

/**
  \ingroup comments
  Adds a comment to the kate_comment structure.
  The comment must be of the form "tag=value"
  The comment tag must be 7 bit ASCII, and may not contain embedded NULs
  The comment value is UTF-8 and may contain embedded NULs
  \param kc the kate_comment structure to add the comment to
  \param comment the comment to add (a stream of len bytes)
  \param len the number of bytes in the comment
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_comment_add_length(kate_comment *kc,const char *comment,size_t len)
{
  int *cl;
  char **uc;
  const char *equals;
  int ret;

  if (!kc || !comment) return KATE_E_INVALID_PARAMETER;

  ret=kate_check_add_overflow(kc->comments,1,NULL);
  if (ret<0) return ret;
  ret=kate_check_add_overflow(len,1,NULL);
  if (ret<0) return ret;

  equals=memchr(comment,'=',len);
  if (!equals) return KATE_E_BAD_TAG;
  ret=kate_comment_check_tag(comment,equals-comment);
  if (ret<0) return ret;
  ret=kate_text_validate(kate_utf8,equals,len-(equals-comment));
  if (ret<0) return ret;

  uc=kate_checked_realloc(kc->user_comments,(kc->comments+1),sizeof(char*));
  if (!uc) return KATE_E_OUT_OF_MEMORY;
  kc->user_comments=uc;
  cl=kate_checked_realloc(kc->comment_lengths,(kc->comments+1),sizeof(int));
  if (!cl) return KATE_E_OUT_OF_MEMORY;
  kc->comment_lengths=cl;

  kc->user_comments[kc->comments]=(char*)kate_malloc(len+1);
  if (!kc->user_comments[kc->comments]) return KATE_E_OUT_OF_MEMORY;
  memcpy(kc->user_comments[kc->comments],comment,len);
  kc->user_comments[kc->comments][len]=0;
  kc->comment_lengths[kc->comments]=len;

  ++kc->comments;

  return 0;
}

/**
  \ingroup comments
  Adds a comment to the kate_comment structure.
  The comment must be of the form "tag=value"
  The comment tag must be 7 bit ASCII, and may not contain embedded NULs
  The comment value is UTF-8 and may not contain embedded NULs as the comments ends at the first NUL encountered
  \param kc the kate_comment structure to add the comment to
  \param comment the comment to add, NUL terminated
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_comment_add(kate_comment *kc,const char *comment)
{
  if (!kc || !comment) return KATE_E_INVALID_PARAMETER;

  return kate_comment_add_length(kc,comment,strlen(comment));
}

/**
  \ingroup comments
  Adds a comment to the kate_comment structure, formatted as "tag=value".
  The tag must be 7 bit ASCII and comply with Vorbis comment tag rules.
  The value must be valid UTF-8 text.
  Neither tag nor value may contain embedded NULs. To embed comments with
  embedded NUL in the payload, see kate_comment_add_length.
  \param kc the kate_comment structure to add the comment to
  \param tag the tag of the comment to add
  \param value the contents of the comment to add
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_comment_add_tag(kate_comment *kc,const char *tag,const char *value)
{
  char *comment;

  if (!kc || !tag || !value) return KATE_E_INVALID_PARAMETER;

  comment=(char*)kate_malloc(strlen(tag)+1+strlen(value)+1);
  if (!comment) return KATE_E_OUT_OF_MEMORY;
  sprintf(comment,"%s=%s",tag,value);
  kate_comment_add(kc,comment);
  kate_free(comment);

  return 0;
}

/**
  \ingroup comments
  Queries the value of a comment that has the given tag.
  The tags are case insensitive, so "tag", "TAG", "Tag", and "TaG" are all equivalent.
  If there are multiple comments with the same tag, count may be used to
  select which one to return.
  The number of comments with a given tag may be retrieved using kate_comment_query_count.
  \param kc the kate_comment structure to look into
  \param tag the title of the comment to look for
  \param count the index of the matching comment to return (if there are several)
  \returns 0 success
  \returns KATE_E_* error
  */
const char *kate_comment_query(const kate_comment *kc,const char *tag,int count)
{
  int n;
  size_t bytes;

  if (!kc) return NULL;

  for (n=0;n<kc->comments;++n) {
    const char *eq=strchr(kc->user_comments[n],'=');
    if (!eq) continue; /* wrong format */
    bytes=eq-kc->user_comments[n];
    if (!kate_ascii_strncasecmp(tag,kc->user_comments[n],bytes)) {
      if (count==0) return eq+1;
      --count;
    }
  }
  /* not found */
  return NULL;
}

/**
  \ingroup comments
  Returns the number of comments with the given tag.
  The tags are case insensitive, so "tag", "TAG", "Tag", and "TaG" are all equivalent.
  \param kc the kate_comment structure to look into
  \param tag the title of the comment to look for
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_comment_query_count(const kate_comment *kc,const char *tag)
{
  int n,count;
  size_t bytes;

  if (!kc) return KATE_E_INVALID_PARAMETER;

  count=0;
  for (n=0;n<kc->comments;++n) {
    const char *eq=strchr(kc->user_comments[n],'=');
    if (!eq) continue; /* wrong format */
    bytes=eq-kc->user_comments[n];
    if (!kate_ascii_strncasecmp(tag,kc->user_comments[n],bytes)) {
      ++count;
    }
  }

  return count;
}

