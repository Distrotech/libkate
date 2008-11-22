#include <stdio.h>
#include <string.h>
#include "kate/kate.h"

/** \defgroup text Text manipulation */
extern int kate_text_get_character(kate_text_encoding text_encoding,const char ** const text,size_t *len0);
extern int kate_text_set_character(kate_text_encoding text_encoding,int c,char ** const text,size_t *len0);
extern int kate_text_remove_markup(kate_text_encoding text_encoding,char *text,size_t *len0);
extern int kate_text_validate(kate_text_encoding text_encoding,const char *text,size_t len0);

static int count_glyphs(const char *text)
{
  int ret;
  size_t len0=strlen(text)+1;
  int glyphs=0;
  while ((ret=kate_text_get_character(kate_utf8,&text,&len0))>0)
    ++glyphs;
  if (ret<0) return ret;
  return glyphs;
}

#define COUNT_GLYPHS(text,glyphs) \
  do { \
    int ret=count_glyphs(text); \
    if (ret!=glyphs) { \
      fprintf(stderr,"%s: counted %d glyphs, should be %d\n",text,ret,glyphs); \
      return 1; \
    } \
  } while(0)

#define REMOVE_MARKUP(s0,s1) \
  do { \
    int ret; \
    size_t len0; \
    char *removed=strdup(s0); \
    if (!removed) { \
      fprintf(stderr,"failed to allocate memory - cannot test\n"); \
      return 77; \
    } \
    len0=strlen(removed)+1; \
    ret=kate_text_remove_markup(kate_utf8,removed,&len0); \
    if (ret<0) { \
      fprintf(stderr,"%s: failed to remove markup\n",s0); \
      return 1; \
    } \
    if (strcmp(removed,s1)) { \
      fprintf(stderr,"%s: should be %s without markup, but is %s\n",s0,s1,removed); \
      return 1; \
    } \
    free(removed); \
  } while(0)

int main()
{
  COUNT_GLYPHS("",0);
  COUNT_GLYPHS("Kate",4);
  COUNT_GLYPHS("\377",KATE_E_TEXT);
  COUNT_GLYPHS("\xe0\xb8\x87",1);
  COUNT_GLYPHS("-\xe0\xb8\x87-\xe0\xbd\xa9-",5);
  REMOVE_MARKUP("","");
  REMOVE_MARKUP("<foo></foo>","");
  REMOVE_MARKUP("simple","simple");
  REMOVE_MARKUP("<foo></foo>","");
  REMOVE_MARKUP("\xe0\xb8\x87<foo>\xe0\xb8\x87</foo>\xe0\xb8\x87","\xe0\xb8\x87\xe0\xb8\x87\xe0\xb8\x87");
  REMOVE_MARKUP("\xe0\xb8\x87<\xe0\xb8\x87>\xe0\xb8\x87","\xe0\xb8\x87\xe0\xb8\x87");
  return 0;
}

