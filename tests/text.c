#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kate/kate.h"

/** \defgroup text Text manipulation */
//extern int kate_text_get_character(kate_text_encoding text_encoding,const char ** const text,size_t *len0);
//extern int kate_text_set_character(kate_text_encoding text_encoding,int c,char ** const text,size_t *len0);
//extern int kate_text_remove_markup(kate_text_encoding text_encoding,char *text,size_t *len0);
//extern int kate_text_validate(kate_text_encoding text_encoding,const char *text,size_t len0);

static char *dupstr(const char *s)
{
  size_t len=strlen(s);
  char *s2=(char*)malloc(len+1);
  if (!s2) return NULL;
  memcpy(s2,s,len+1);
  return s2;
}

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
      fprintf(stderr,"line %u: %s: counted %d glyphs, should be %d\n",__LINE__,text,ret,glyphs); \
      return 1; \
    } \
  } while(0)

#define REMOVE_MARKUP(s0,s1) \
  do { \
    int ret; \
    size_t len0; \
    char *removed=dupstr(s0); \
    if (!removed) { \
      fprintf(stderr,"failed to allocate memory - cannot test\n"); \
      return 77; \
    } \
    len0=strlen(removed)+1; \
    ret=kate_text_remove_markup(kate_utf8,removed,&len0); \
    if (ret<0) { \
      fprintf(stderr,"line %u: %s: failed to remove markup\n",__LINE__,s0); \
      return 1; \
    } \
    if (strcmp(removed,s1)) { \
      fprintf(stderr,"line %u: %s: should be %s without markup, but is %s\n",__LINE__,s0,s1,removed); \
      return 1; \
    } \
    free(removed); \
  } while(0)

#define CHECK_VALID(text) \
  do { \
    int ret=kate_text_validate(kate_utf8,text,strlen(text)+1); \
    if (ret<0) { \
      fprintf(stderr,"line %u: %s: supposed to be valid, but kate_text_validate returned an error (%d)\n",__LINE__,text,ret); \
      return 1; \
    } \
  } while(0)

#define CHECK_INVALID(text) \
  do { \
    int ret=kate_text_validate(kate_utf8,text,strlen(text)+1); \
    if (ret!=KATE_E_TEXT) { \
      fprintf(stderr,"line %u: %s: supposed to be invalid, but kate_text_validate did not return a text error (%d)\n",__LINE__,text,ret); \
      return 1; \
    } \
  } while(0)

/* CHECK_VALID2 checks ranges outside the "currently" assigned range of 0-0x10ffff,
   so define it to either CHECK_VALID or CHECK_INVALID depending on whether libkate
   was built to accept them or not. By default, it is not. */
#define CHECK_VALID2 CHECK_INVALID
/* #define CHECK_VALID2 CHECK_VALID */

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

  /* those taken from Markus Kuhn's UTF-8 test file */

  /* Some correct UTF-8 text */
  CHECK_VALID("Îºá½¹ÏƒÎ¼Îµ");

  /* 2.1  First possible sequence of a certain length */
  CHECK_VALID("\0");
  CHECK_VALID("Â€");
  CHECK_VALID("à €");
  CHECK_VALID("ð€€");
  CHECK_VALID2("øˆ€€€");
  CHECK_VALID2("ü„€€€€");

  /* 2.2  Last possible sequence of a certain length */
  CHECK_VALID("");
  CHECK_VALID("ß¿");
  CHECK_INVALID("ï¿¿");
  CHECK_VALID2("÷¿¿¿");
  CHECK_VALID2("û¿¿¿¿");
  CHECK_VALID2("ý¿¿¿¿¿");

  /* 2.3  Other boundary conditions */
  CHECK_VALID("íŸ¿");
  CHECK_VALID("î€€");
  CHECK_VALID("ï¿½");
  CHECK_VALID("ô¿¿");
  CHECK_VALID2("ô€€");

  /* 3.1  Unexpected continuation bytes */
  CHECK_INVALID("€");
  CHECK_INVALID("¿");

  CHECK_INVALID("€¿");
  CHECK_INVALID("€¿€");
  CHECK_INVALID("€¿€¿");
  CHECK_INVALID("€¿€¿€");
  CHECK_INVALID("€¿€¿€¿");
  CHECK_INVALID("€¿€¿€¿€");

  CHECK_INVALID("€‚ƒ„…†‡ˆ‰Š‹ŒŽ‘’“”•–—˜™š›œžŸ¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿");
  CHECK_INVALID("à á â ã ä å æ ç è)");
  CHECK_INVALID("ð ñ ò ó ô)");
  CHECK_INVALID("ø ù ú)");
  CHECK_INVALID("ü ý ");

  /* 3.3  Sequences with last continuation byte missing */
  CHECK_INVALID("À");
  CHECK_INVALID("à€");
  CHECK_INVALID("ð€€");
  CHECK_INVALID("ø€€€");
  CHECK_INVALID("ü€€€€");
  CHECK_INVALID("ß");
  CHECK_INVALID("ï¿");
  CHECK_INVALID("÷¿¿");
  CHECK_INVALID("û¿¿¿");
  CHECK_INVALID("ý¿¿¿¿");

  /* 3.4  Concatenation of incomplete sequences */
  CHECK_INVALID("Àà€ð€€ø€€€ü€€€€ßï¿÷¿¿û¿¿¿ý¿¿¿¿");

  /* 3.5  Impossible bytes */
  CHECK_INVALID("þ");
  CHECK_INVALID("ÿ");
  CHECK_INVALID("þþÿÿ");

  /* 4.1  Examples of an overlong ASCII character */
  CHECK_INVALID("À¯");
  CHECK_INVALID("à€¯");
  CHECK_INVALID("ð€€¯");
  CHECK_INVALID("ø€€€¯");
  CHECK_INVALID("ü€€€€¯");

  /* 4.2  Maximum overlong sequences */
  CHECK_INVALID("Á¿");
  CHECK_INVALID("àŸ¿");
  CHECK_INVALID("ð¿¿");
  CHECK_INVALID("ø‡¿¿¿");
  CHECK_INVALID("üƒ¿¿¿¿");

  /* 4.3  Overlong representation of the NUL character */
  CHECK_INVALID("À€");
  CHECK_INVALID("à€€");
  CHECK_INVALID("ð€€€");
  CHECK_INVALID("ø€€€€");
  CHECK_INVALID("ü€€€€€");

  /* 5.1 Single UTF-16 surrogates */
  CHECK_INVALID("í €");
  CHECK_INVALID("í­¿");
  CHECK_INVALID("í®€");
  CHECK_INVALID("í¯¿");
  CHECK_INVALID("í°€");
  CHECK_INVALID("í¾€");
  CHECK_INVALID("í¿¿");

  /* 5.2 Paired UTF-16 surrogates */
  CHECK_INVALID("í €í°€");
  CHECK_INVALID("í €í¿¿");
  CHECK_INVALID("í­¿í°€");
  CHECK_INVALID("í­¿í¿¿");
  CHECK_INVALID("í®€í°€");
  CHECK_INVALID("í®€í¿¿");
  CHECK_INVALID("í¯¿í°€");
  CHECK_INVALID("í¯¿í¿¿");

  /* 5.3 Other illegal code positions */
  CHECK_INVALID("ï¿¾");
  CHECK_INVALID("ï¿¿");

  return 0;
}

