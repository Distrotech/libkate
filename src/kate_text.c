/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#define KATE_INTERNAL
#include "kate_internal.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include "kate/kate.h"

/* #define ENABLE_CODE_POINTS_ABOVE_0x10ffff */

inline int kate_is_valid_code_point(int c)
{
  /* surrogate range is invalid */
  if (c>=0xd800 && c<=0xdfff) return 0; /* UTF-16 surrogates */
  if (c>=0xfffe && c<=0xffff) return 0; /* Markus Kuhn's UTF-8 test files says these are invalid */

#ifdef ENABLE_CODE_POINTS_ABOVE_0x10ffff
  return c>=0 && c<=0x7fffffff;
#else
  return c>=0 && c<=0x10ffff;
#endif
}

static inline int get_bytes_for_code_point(int c) __attribute__((const));
static inline int get_bytes_for_code_point(int c)
{
  if (!kate_is_valid_code_point(c)) return -1;
  if (c<=0x7f) return 1;
  if (c<=0x7ff) return 2;
  if (c<=0xffff) return 3;
#ifdef ENABLE_CODE_POINTS_ABOVE_0x10ffff
  if (c<=0x001fffff) return 4;
  if (c<=0x03ffffff) return 5;
  if (c<=0x7fffffff) return 6;
#else
  if (c<=0x10ffff) return 4;
#endif
  return -1;
}

static int kate_text_utf8_read(const char *s,int *cp)
{
  int c;

  if (!s) return KATE_E_INVALID_PARAMETER;

  c=0;

  if (((*s)&0x80)==0) {
    /* 1 byte */
    c=*s;

    *cp=c;
    return 1;
  }
  else if (((*s)&0xe0)==0xc0) {
    /* 2 bytes */
    c|=(((*s)&0x1f)<<6);
    s++;
    if (((*s)&0xc0)!=0x80) return KATE_E_TEXT;
    c|=((*s)&0x3f);
    if (c<=0x7f) return KATE_E_TEXT;

    *cp=c;
    return 2;
  }
  else if (((*s)&0xf0)==0xe0) {
    /* 3 bytes */
    c|=(((*s)&0xf)<<12);
    s++;
    if (((*s)&0xc0)!=0x80) return KATE_E_TEXT;
    c|=(((*s)&0x3f)<<6);
    s++;
    if (((*s)&0xc0)!=0x80) return KATE_E_TEXT;
    c|=(((*s)&0x3f));
    if (c<=0x7ff) return KATE_E_TEXT;

    *cp=c;
    return 3;
  }
  else if (((*s)&0xf8)==0xf0) {
    /* 4 bytes */
    c|=(((*s)&0x7)<<18);
    s++;
    if (((*s)&0xc0)!=0x80) return KATE_E_TEXT;
    c|=(((*s)&0x3f)<<12);
    s++;
    if (((*s)&0xc0)!=0x80) return KATE_E_TEXT;
    c|=(((*s)&0x3f)<<6);
    s++;
    if (((*s)&0xc0)!=0x80) return KATE_E_TEXT;
    c|=(((*s)&0x3f));
    if (c<=0xffff) return KATE_E_TEXT;

    *cp=c;
    return 4;
  }
#ifdef ENABLE_CODE_POINTS_ABOVE_0x10ffff
  /* 5 and 6 bytes are for unicode code points not yet assigned */
  else if (((*s)&0xfc)==0xf8) {
    /* 5 bytes */
    c|=(((*s)&0x3)<<24);
    s++;
    if (((*s)&0xc0)!=0x80) return KATE_E_TEXT;
    c|=(((*s)&0x3f)<<18);
    s++;
    if (((*s)&0xc0)!=0x80) return KATE_E_TEXT;
    c|=(((*s)&0x3f)<<12);
    s++;
    if (((*s)&0xc0)!=0x80) return KATE_E_TEXT;
    c|=(((*s)&0x3f)<<6);
    s++;
    if (((*s)&0xc0)!=0x80) return KATE_E_TEXT;
    c|=(((*s)&0x3f));
    if (c<=0x001fffff) return KATE_E_TEXT;

    *cp=c;
    return 5;
  }
  else if (((*s)&0xfe)==0xfc) {
    /* 6 bytes */
    c|=(((*s)&0x1)<<30);
    s++;
    if (((*s)&0xc0)!=0x80) return KATE_E_TEXT;
    c|=(((*s)&0x3f)<<24);
    s++;
    if (((*s)&0xc0)!=0x80) return KATE_E_TEXT;
    c|=(((*s)&0x3f)<<18);
    s++;
    if (((*s)&0xc0)!=0x80) return KATE_E_TEXT;
    c|=(((*s)&0x3f)<<12);
    s++;
    if (((*s)&0xc0)!=0x80) return KATE_E_TEXT;
    c|=(((*s)&0x3f)<<6);
    s++;
    if (((*s)&0xc0)!=0x80) return KATE_E_TEXT;
    c|=(((*s)&0x3f));
    if (c<=0x03ffffff) return KATE_E_TEXT;

    *cp=c;
    return 6;
  }
#endif

  return KATE_E_TEXT;
}

static int kate_text_utf8_write(char *s,int cp)
{
  int bytes;

  if (!s) return KATE_E_INVALID_PARAMETER;
  if (!kate_is_valid_code_point(cp)) return KATE_E_INVALID_PARAMETER;

  bytes=get_bytes_for_code_point(cp);
  if (bytes<=0) return KATE_E_INVALID_PARAMETER;

  switch (bytes) {
  case 1:
    *s++=cp;
    break;
  case 2:
    *s++=0xc0 | (cp>>6);
    *s++=0x80 | (cp&0x3f);
    break;
  case 3:
    *s++=0xe0 | (cp>>12);
    *s++=0x80 | ((cp>>6)&0x3f);
    *s++=0x80 | (cp&0x3f);
    break;
  case 4:
    *s++=0xf0 | (cp>>18);
    *s++=0x80 | ((cp>>12)&0x3f);
    *s++=0x80 | ((cp>>6)&0x3f);
    *s++=0x80 | (cp&0x3f);
    break;
#ifdef ENABLE_CODE_POINTS_ABOVE_0x10ffff
  case 5:
    *s++=0xf8 | (cp>>24);
    *s++=0x80 | ((cp>>18)&0x3f);
    *s++=0x80 | ((cp>>12)&0x3f);
    *s++=0x80 | ((cp>>6)&0x3f);
    *s++=0x80 | (cp&0x3f);
    break;
  case 6:
    *s++=0xfc | (cp>>30);
    *s++=0x80 | ((cp>>24)&0x3f);
    *s++=0x80 | ((cp>>18)&0x3f);
    *s++=0x80 | ((cp>>12)&0x3f);
    *s++=0x80 | ((cp>>6)&0x3f);
    *s++=0x80 | (cp&0x3f);
    break;
#endif
  default:
    return KATE_E_INVALID_PARAMETER;
  }
  return bytes;
}

/**
  \ingroup text
  Reads a code point from the string, advancing the text pointer to the next one.
  \param text_encoding the character encoding the text is coded in
  \param text a pointer to the text pointer, to be advanced to the next character on success
  \param len0 a pointer to the length of the buffer, including any terminating NUL, to be decreased by the number of bytes that text is advanced on success
  \returns >=0 success, the unicode code point read
  \returns KATE_E_* error
  */
int kate_text_get_character(kate_text_encoding text_encoding,const char ** const text,size_t *len0)
{
  const char *new_text;
  int c,ret;
  size_t bytes;

  if (!text || !len0) return KATE_E_INVALID_PARAMETER;

  switch (text_encoding) {
    case kate_utf8:
      new_text=*text;
      ret=kate_text_utf8_read(new_text,&c);
      if (ret<0) return ret;
      bytes=ret;
      if (bytes>*len0) return KATE_E_TEXT;
      *len0-=bytes;
      *text+=bytes;
      return c;
    default:
      return KATE_E_INVALID_PARAMETER;
  }
}

/**
  \ingroup text
  Writes a code point to the given string, advancing the text pointer to the next byte.
  \param text_encoding the character encoding the text is coded in
  \param c the unicode code point to write to the string
  \param text a pointer to the text pointer, to be advanced to the next character on success
  \param len0 a pointer to the length of the buffer, including any terminating NUL, to be decreased by the number of bytes that text is advanced on success
  \returns >=0 success, the number of bytes used to write this code point
  \returns KATE_E_* error
  */
int kate_text_set_character(kate_text_encoding text_encoding,int c,char ** const text,size_t *len0)
{
  char tmp[8]={0};
  size_t bytes;
  int ret;

  if (!text || !len0) return KATE_E_INVALID_PARAMETER;

  switch (text_encoding) {
    case kate_utf8:
      ret=kate_text_utf8_write(tmp,c);
      if (ret<0) return ret;
      bytes=ret;
      if (bytes>*len0) return KATE_E_TEXT;
      memcpy(*text,tmp,bytes);
      *text+=bytes;
      *len0-=bytes;
      return bytes;
    default:
      return KATE_E_INVALID_PARAMETER;
  }
}

/**
  \ingroup text
  Removes markup from the given text.
  \param text_encoding the character encoding the text is coded in
  \param text the text to remove markup from
  \param len0 the length in bytes of the text, including any terminating NUL - will be set to the length of the text with markup removed, including any terminating NUL
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_text_remove_markup(kate_text_encoding text_encoding,char *text,size_t *len0)
{
  char *r=text,*w=text;
  int in_tag=0;
  size_t n;

  if (!text || !len0) return KATE_E_INVALID_PARAMETER;

  switch (text_encoding) {
    case kate_utf8:
      while (*r && (size_t)(r-text)<*len0) {
        int ret,c;
        ret=kate_text_utf8_read(r,&c);
        if (ret<0) return ret;
        r+=ret;
        if (r>text+*len0) {
          /* we went over the limit, we probably read dummy data, discard */
          break;
        }
        if (c=='<') {
          in_tag++;
          /* add a newline if we just removed a br */
          if (*len0>=3 && !strncmp(r,"br>",3)) {
            ret=kate_text_utf8_write(w,'\n');
            if (ret<0) return ret;
            w+=ret;
          }
        }
        if (!in_tag) {
          ret=kate_text_utf8_write(w,c);
          if (ret<0) return ret;
          w+=ret;
        }
        if (c=='>') {
          in_tag--;
        }
      }
      /* zero all bytes we removed */
      for (n=0;n<*len0-(w-text);++n) w[n]=0;
      /* adjust len0 to new size */
      *len0=w-text;
      break;
    default:
      return KATE_E_INVALID_PARAMETER;
  }

  return 0;
}

/**
  \ingroup text
  Validates text for the given character encoding, flagging partial sequences and invalid sequences.
  \param text_encoding the character encoding the text is coded in
  \param text the text to validate (may include embedded NULs)
  \param len0 the length in bytes of the text, including any terminating NUL
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_text_validate(kate_text_encoding text_encoding,const char *text,size_t len0)
{
  if (!text) return KATE_E_INVALID_PARAMETER;

  switch (text_encoding) {
    case kate_utf8:
      while (len0>0) {
        int ret,c;
        ret=kate_text_utf8_read(text,&c);
        if (ret<0) return ret;
        if (!kate_is_valid_code_point(c)) return KATE_E_TEXT;
        if ((size_t)ret>len0) {
          /* we went over the limit, we probably read dummy data, discard */
          return KATE_E_TEXT;
        }
        text+=ret;
        len0-=ret;
      }
      break;
    default:
      return KATE_E_INVALID_PARAMETER;
  }

  return 0;
}

