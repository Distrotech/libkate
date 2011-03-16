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
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <errno.h>
#include <ogg/ogg.h>
#include "kate/oggkate.h"
#include "katedesc.h"

extern int katedesc_parse(void);
extern int katedesc_restart(FILE*);
extern FILE *katedesc_in;
extern FILE *katedesc_out;
extern int nerrors;

static ogg_stream_state os;
kate_state k;
kate_info ki;
kate_comment kc;
static ogg_int64_t packetno;
char base_path[4096]="";

static char str[4096];
static int headers_written=0;
#ifdef DEBUG
static int raw=0;
#endif
static kate_float repeat_threshold=(kate_float)0;
static kate_float keepalive_threshold=(kate_float)0;
static kate_int64_t last_stream_time=0;
static const unsigned char utf8bom[3]={0xef,0xbb,0xbf};
static const unsigned char utf16lebom[2]={0xff,0xfe};
static const unsigned char utf16bebom[2]={0xfe,0xff};
static const unsigned char utf32lebom[4]={0xff,0xfe,0x00,0x00};
static const unsigned char utf32bebom[4]={0x00,0x00,0xfe,0xff};

static const char *language=NULL;
static const char *category=NULL;

static int flush_page(FILE *f)
{
  ogg_page og;
  while (1) {
    int ret=ogg_stream_flush(&os,&og);
    if (ret==0) break;
    ret=fwrite(og.header,1,og.header_len,f);
    if (ret!=og.header_len) {
      fprintf(stderr,"Write failed (%s)\n",strerror(errno));
      return -1;
    }
    ret=fwrite(og.body,1,og.body_len,f);
    if (ret!=og.body_len) {
      fprintf(stderr,"Write failed (%s)\n",strerror(errno));
      return -1;
    }
    fflush(f);
  }
  return 0;
}

static int poll_page(FILE *f)
{
  /* discontinuous codec, so we want to flush pages every time,
     as Ogg keeps granules per page, not per packet */
  return flush_page(f);
}

int write_headers(FILE *f)
{
  ogg_packet op;

  /* command line overrides */
  if (language) {
    int ret=kate_info_set_language(&ki,language);
    if (ret<0) {
      fprintf(stderr,"Failed to set stream language to '%s': %d\n",language,ret);
      return ret;
    }
  }
  if (category) {
    int ret=kate_info_set_category(&ki,category);
    if (ret<0) {
      fprintf(stderr,"Failed to set stream category to '%s': %d\n",category,ret);
      return ret;
    }
  }

  /* warn if either language or category is missing */
  if (!ki.category || !*ki.category) {
    fprintf(stderr,"warning: no category defined\n");
  }
  if (!ki.language || !*ki.language) {
    fprintf(stderr,"warning: no language defined\n");
  }

  while (1) {
    int ret=kate_ogg_encode_headers(&k,&kc,&op);
    if (ret<0) {
      fprintf(stderr,"error encoding headers: %d\n",ret);
      return ret;
    }
    if (ret>0) break; /* we're done */

#ifdef DEBUG
    if (raw) {
      int ret=send_packet(f,&op,-1);
      if (ret<0) {
        fprintf(stderr,"error sending packet: %d\n",ret);
        return ret;
      }
    }
    else
#endif
    {
      ogg_stream_packetin(&os,&op);
      ogg_packet_clear(&op);
    }

    /* place all headers on the same page, it keeps oggmerge happy */
    /* poll_page(f); */
  }

  return flush_page(f);
}

int send_packet(FILE *f,ogg_packet *op,kate_int64_t t)
{
  if (t>last_stream_time)
    last_stream_time=t;

#ifdef DEBUG
  if (raw) {
    int ret;
    if (op->packetno>0) {
      ogg_int64_t bytes=op->bytes;
      ret=fwrite(&bytes,1,8,f);
      if (ret!=8) {
        fprintf(stderr,"Write failed (%s)\n",strerror(errno));
        return -1;
      }
    }
    ret=fwrite(op->packet,1,op->bytes,f);
    if (ret!=op->bytes) {
      fprintf(stderr,"Write failed (%s)\n",strerror(errno));
      return -1;
    }
    fflush(f);
    return 0;
  }
  else
#endif
  {
    ogg_stream_packetin(&os,op);
    ogg_packet_clear(op);
    return poll_page(f);
  }
}

void cancel_packet(ogg_packet *op)
{
  ogg_packet_clear(op);
}

static int flush_headers(FILE *f)
{
  int ret=0;
  if (!headers_written) {
    ret=write_headers(f);
    headers_written=1;
  }
  return ret;
}

static const char *eat_arg(int argc,char **argv,int *n)
{
  if (*n==argc-1) {
    fprintf(stderr,"%s needs an argument\n",argv[*n]);
    exit(-1);
  }
  return argv[++*n];
}

static kate_int64_t hmsms2ms(int h,int m,int s,int ms)
{
  return h*(kate_int64_t)3600000+m*60000+s*1000+ms;
}

static char *fgets2(char *s,size_t len,FILE *f,int ignore_hash)
{
  char *ret=fgets(s,len,f);
  if (ret) {
    char *ptr=strpbrk(s,"\r"); /* fix windows text files */
    if (ptr) strcpy(ptr,"\n");
    if (!ignore_hash) {
      ptr=strchr(s,'#');
      if (ptr) strcpy(ptr,"\n");
    }
  }
  else *s=0;
  return ret;
}

static int convert_kate(FILE *fin,FILE *fout)
{
  katedesc_in=fin;
  katedesc_out=fout;
  katedesc_restart(katedesc_in);
  nerrors=0;
  katedesc_parse();
  cleanup_lexer();
  if (nerrors>0) return -1;
  return 0;
}

static int is_line_empty(const char *s)
{
  while (*s) {
    if (!strchr(" \t\r\n",*s)) return 0;
    ++s;
  }
  return 1;
}

void update_stream_time(kate_state *k,FILE *fout,kate_int64_t endt)
{
  ogg_packet op;
  kate_int64_t t;
  int ret;
  kate_int64_t step;
  kate_int64_t raw_repeat_threshold=kate_duration_granule(k->ki,repeat_threshold);
  kate_int64_t raw_keepalive_threshold=kate_duration_granule(k->ki,keepalive_threshold);

  /* work out the best step to walk the time segment */
  step=0;
  if (step==0 || (raw_repeat_threshold>0 && raw_repeat_threshold<step))
    step=raw_repeat_threshold;
  if (step==0 || (raw_keepalive_threshold>0 && raw_keepalive_threshold<step))
    step=raw_keepalive_threshold;

  if (step<=0) return;

  for (t=last_stream_time;t<endt;t+=step) {
    /* try repeats first, no keepalives will be needed if we have one */
    if (raw_repeat_threshold>0) {
      while (1) {
        ret=kate_ogg_encode_repeat_raw_times(k,t,raw_repeat_threshold,&op);
        if (ret<0) {
          fprintf(stderr,"Failed encoding repeat at %"PRId64" (%d), continuing anyway\n",t,ret);
          return;
        }
        if (ret==0) break;
        ret=send_packet(fout,&op,t);
        if (ret<0) {
          fprintf(stderr,"Failed sending repeat packet, continuing anyway\n");
          return;
        }
      }
    }
    /* keepalives next */
    if (raw_keepalive_threshold>0 && t-last_stream_time>=raw_keepalive_threshold) {
      ret=kate_ogg_encode_keepalive_raw_times(k,t,&op);
      if (ret<0) {
        fprintf(stderr,"Failed encoding keepalive at %"PRId64" (%d), continuing anyway\n",t,ret);
        return;
      }
      ret=send_packet(fout,&op,t);
      if (ret<0) {
        fprintf(stderr,"Failed sending keepalive packet, continuing anyway\n");
        return;
      }
    }
  }
}

static void remove_last_newline(char *text)
{
  size_t len;

  len=strlen(text);
  if (len>0 && text[len-1]=='\n') text[--len]=0;
}

static int emit_srt_event(FILE *fout,kate_int64_t t0,kate_int64_t t1,const char *text,int allow_srt_markup,int x1,int y1,int x2,int y2)
{
  ogg_packet op;
  size_t len;
  int ret;

  update_stream_time(&k,fout,t0);

  len=strlen(text);
  ret=kate_text_validate(kate_utf8,text,len+1);
  if (ret<0) {
    if (ret==KATE_E_TEXT) {
      fprintf(stderr,"Text is not valid UTF-8: %s\n",text);
    }
    else {
      fprintf(stderr,"Failed to validate text: %d\n",ret);
    }
    return ret;
  }
  if (allow_srt_markup) kate_encode_set_markup_type(&k,kate_markup_simple);
  if (x1!=-INT_MAX && y1!=-INT_MAX && x2!=-INT_MAX && y2!=-INT_MAX) {
    kate_region kr;
    kate_region_init(&kr);
    kr.metric=kate_pixel;
    kr.x=x1;
    kr.y=y1;
    kr.w=x2-x1+1;
    kr.h=y2-y1+1;
    ret=kate_encode_set_region(&k,&kr);
    if (ret<0) {
      fprintf(stderr,"Error setting region: %d\n",ret);
      return ret;
    }
  }
  ret=kate_ogg_encode_text_raw_times(&k,t0,t1,text,len,&op);
  if (ret<0) {
    fprintf(stderr,"Error encoding text: %d\n",ret);
    return ret;
  }
  else {
    ret=send_packet(fout,&op,t0);
    if (ret<0) return ret;
  }
  return ret;
}

static int convert_srt(FILE *fin,FILE *fout,int allow_srt_markup)
{
  enum { need_id, need_timing, need_text };
  int need = need_id;
  int last_seen_id=0;
  int ret;
  int id;
  static char text[4096];
  int h0,m0,s0,ms0,h1,m1,s1,ms1;
  int x1,y1,x2,y2;
  kate_int64_t t0=0,t1=0;
  int line=0;

  ret=flush_headers(fout);
  if (ret<0) return ret;

  fgets2(str,sizeof(str),fin,1);

  if (!memcmp(str,utf16lebom,sizeof(utf16lebom)) || !memcmp(str,utf16bebom,sizeof(utf16bebom))) {
    fprintf(stderr,"This file seems to be encoded in UTF-16, Kate only supports UTF-8\n");
    fprintf(stderr,"You will need to convert it to UTF-8 first (eg, use iconv)\n");
    return -1;
  }

  if (!memcmp(str,utf32lebom,sizeof(utf32lebom)) || !memcmp(str,utf32bebom,sizeof(utf32bebom))) {
    fprintf(stderr,"This file seems to be encoded in UTF-32, Kate only supports UTF-8\n");
    fprintf(stderr,"You will need to convert it to UTF-8 first (eg, use iconv)\n");
    return -1;
  }

  /* kill any utf-8 BOM present */
  if (!memcmp(str,utf8bom,sizeof(utf8bom))) {
    /* printf("UTF-8 BOM found at start, skipped\n"); */
    memmove(str,str+3,strlen(str+3)+1);
  }

  while (!feof(fin) || *str) {
    ++line;
    switch (need) {
      case need_id:
        /* allow more than one blank line between events */
        if (is_line_empty(str)) {
          break;
        }
        ret=sscanf(str,"%d\n",&id);
        if (ret!=1) {
          fprintf(stderr,"Syntax error at line %d: %s\n",line,str);
          return -1;
        }
        if (id!=last_seen_id+1) {
          fprintf(stderr,"Warning at line %d: non consecutive ids: %s\n",line,str);
        }
        last_seen_id=id;
        need=need_timing;
        strcpy(text,"");
        break;
      case need_timing:
        ret=sscanf(str,"%d:%d:%d%*[,.]%d --> %d:%d:%d%*[,.]%d %*[xX]1: %d %*[xX]2: %d %*[yY]1: %d %*[yY]2: %d\n",&h0,&m0,&s0,&ms0,&h1,&m1,&s1,&ms1,&x1,&x2,&y1,&y2);
        if (ret!=12) {
          x1=y1=x2=y2=-INT_MAX;
          ret=sscanf(str,"%d:%d:%d%*[,.]%d --> %d:%d:%d%*[,.]%d\n",&h0,&m0,&s0,&ms0,&h1,&m1,&s1,&ms1);
          if (ret!=8) {
            fprintf(stderr,"Syntax error at line %d: %s\n",line,str);
            return -1;
          }
        }

        if ((h0|m0|s0|ms0)<0 || (h1|m1|s1|ms1)<0) {
          fprintf(stderr,"Bad time specification at line %d: %s\n",line,str);
          return -1;
        }
        t0=hmsms2ms(h0,m0,s0,ms0);
        t1=hmsms2ms(h1,m1,s1,ms1);
        if (t1<t0) {
          fprintf(stderr,"Error at line %d: end time must not be less than start time\n",line);
          return -1;
        }
        if (x2<x1 || y2<y1) {
          fprintf(stderr,"Invalid region coordinates at line %d\n",line);
          return -1;
        }
        need=need_text;
        break;
      case need_text:
        if (is_line_empty(str)) {
          remove_last_newline(text);
          ret=emit_srt_event(fout,t0,t1,text,allow_srt_markup,x1,y1,x2,y2);
          if (ret<0) return ret;
          need=need_id;
        }
        else {
          size_t len=strlen(text);
          strncpy(text+len,str,sizeof(text)-len);
          text[sizeof(text)-1]=0;
        }
        break;
    }
    fgets2(str,sizeof(str),fin,1);
  }

  /* add any text we've accumulated if there's no empty line at the end */
  if (need==need_text && text[0]) {
    fprintf(stderr, "Warning: last event is not followed by an empty line - input might be truncated\n");
    remove_last_newline(text);
    ret=emit_srt_event(fout,t0,t1,text,allow_srt_markup,x1,y1,x2,y2);
    if (ret<0) return ret;
  }

  return 0;
}

static void add_kate_karaoke_tag(kate_motion *km,kate_int64_t dt,const char *str,size_t len,int line)
{
  kate_curve *kc;
  kate_int64_t ptr=-500; /* minus half a second, in milliseconds */
  int ret;
  kate_curve **new_curves;
  kate_float *new_durations;

  if (dt<0) {
    fprintf(stderr, "Error: line %d: lyrics times must not be decreasing\n",line);
    return;
  }

  /* work out how many glyphs we have */
  while (len>0) {
    ret=kate_text_get_character(kate_utf8,&str,&len);
    if (ret<0) {
      fprintf(stderr, "Error: line %d: failed to get UTF-8 glyph from string\n",line);
      return;
    }
    ptr+=1000; /* a second, in milliseconds */
  }
  /* ptr now points to the middle of the glyph we're at */

  kc=(kate_curve*)kate_malloc(sizeof(kate_curve));
  if (kc) {
    kate_curve_init(kc);
    kc->type=kate_curve_static;
    kc->npts=1;
    kc->pts=(kate_float*)kate_malloc(2*sizeof(kate_float));
    if (kc->pts) {
      kc->pts[0]=ptr/(kate_float)1000;
      kc->pts[1]=(kate_float)0;

      new_curves=(kate_curve**)kate_realloc(km->curves,(km->ncurves+1)*sizeof(kate_curve*));
      if (new_curves) km->curves=new_curves;
      new_durations=(kate_float*)kate_realloc(km->durations,(km->ncurves+1)*sizeof(kate_float));
      if (new_durations) km->durations=new_durations;

      if (new_curves && new_durations) {
        km->ncurves++;
        km->curves[km->ncurves-1]=kc;
        km->durations[km->ncurves-1]=dt/(kate_float)1000;
      }
      else {
        fprintf(stderr,"Error: failed to allocate memory to store curve/duration");
        kate_free(kc->pts);
        kate_free(kc);
      }
    }
    else {
      fprintf(stderr,"Error: failed to allocate memory for curve points");
      kate_free(kc);
    }
  }
  else {
    fprintf(stderr,"Error: failed to allocate memory for a kate_curve");
  }
}

static int fraction_to_milliseconds(int fraction,int digits)
{
  while (digits<3) {
    fraction*=10;
    ++digits;
  }
  while (digits>3) {
    fraction/=10;
    --digits;
  }
  return fraction;
}

static kate_motion *process_enhanced_lrc_tags(char *str,kate_int64_t start_time,kate_int64_t end_time,int line)
{
  char *start,*end;
  int ret;
  int m,s,fs;
  kate_motion *km=NULL;
  kate_int64_t current_time=start_time;
  int f0,f1;

  if (!str) return NULL;

  start=str;
  while (1) {
    start=strchr(start,'<');
    if (!start) break;
    end=strchr(start+1,'>');
    if (!end) break;

    /* we found a <> pair, parse it */
    f0=f1=-1;
    ret=sscanf(start,"<%d:%d.%n%d%n>",&m,&s,&f0,&fs,&f1);

    /* remove the <> tag from input to get raw text */
    memmove(start,end+1,strlen(end+1)+1);

    if (ret<3 || (f0|f1)<0 || f1<=f0 || (m|s|fs)<0) {
      fprintf(stderr,"Error: failed to process enhanced LRC tag (%*.*s) - ignored\n",(int)(end-start+1),(int)(end-start+1),start);
    }
    else {
      kate_int64_t tag_time=hmsms2ms(0,m,s,fraction_to_milliseconds(fs,f1-f0));

      /* if this is the first tag in this line, create a kate motion */
      if (!km) {
        km=(kate_motion*)kate_malloc(sizeof(kate_motion));
        if (!km) {
          fprintf(stderr,"Error: failed to allocate memory - enhanced LRC tag will be ignored\n");
        }
        else {
          kate_motion_init(km);
          km->semantics=kate_motion_semantics_glyph_pointer_1;
        }
      }
      /* add to the kate motion */
      if (km) {
        add_kate_karaoke_tag(km,tag_time-current_time,str,start-str,line);
        current_time=tag_time;
      }
    }
  }

  /* if we've found karaoke info, extend the motion to the end time */
  if (km) {
    add_kate_karaoke_tag(km,end_time-current_time,str,strlen(str),line);
  }

  return km;
}

static void add_kate_karaoke_style(kate_info *ki,unsigned char r,unsigned char g,unsigned char b,unsigned char a)
{
    kate_style *ks;
    int ret;

    if (!ki) return;

    ks=(kate_style*)kate_malloc(sizeof(kate_style));
    if (ks) {
      kate_style_init(ks);
      ks->text_color.r = r;
      ks->text_color.g = g;
      ks->text_color.b = b;
      ks->text_color.a = a;
      ret=kate_info_add_style(ki,ks);
      if (ret<0) {
        fprintf(stderr,"WARNING - failed to add Kate karaoke style\n");
      }
    }
    else {
      fprintf(stderr,"WARNING - failed to allocate memory for Kate karaoke style\n");
    }
}

static int convert_lrc(FILE *fin,FILE *fout)
{
  int ret;
  static char text[4096];
  int m,s,fs;
  kate_int64_t t;
  kate_int64_t start_time=-1;
  int offset;
  int line=0;
  fpos_t start;
  int has_karaoke;
  kate_motion *km;
  int f0,f1;
  size_t len;

  fgets2(str,sizeof(str),fin,1);
  ++line;

  if (!memcmp(str,utf16lebom,sizeof(utf16lebom)) || !memcmp(str,utf16bebom,sizeof(utf16bebom))) {
    fprintf(stderr,"This file seems to be encoded in UTF-16, Kate only supports UTF-8\n");
    fprintf(stderr,"You will need to convert it to UTF-8 first (eg, use iconv)\n");
    return -1;
  }

  if (!memcmp(str,utf32lebom,sizeof(utf32lebom)) || !memcmp(str,utf32bebom,sizeof(utf32bebom))) {
    fprintf(stderr,"This file seems to be encoded in UTF-32, Kate only supports UTF-8\n");
    fprintf(stderr,"You will need to convert it to UTF-8 first (eg, use iconv)\n");
    return -1;
  }

  /* kill any utf-8 BOM present */
  if (!memcmp(str,utf8bom,sizeof(utf8bom))) {
    /* printf("UTF-8 BOM found at start, skipped\n"); */
    memmove(str,str+3,strlen(str+3)+1);
  }

  /* skip headers */
  while (!feof(fin)) {
    ret=sscanf(str,"[%d:%d.%d]",&m,&s,&fs);
    if (ret==3) break;
    fgets2(str,sizeof(str),fin,1);
    ++line;
  }

  /* there might be 'enhanced lrc' timing tags - if so, we'll include
     a timing motion for the appropriate events, as well as two styles
     in the headers, but we need to know beforehand if we'll need to
     add those styles, so they can go into headers  */
  if (fgetpos(fin,&start)<0) {
    fprintf(stderr,"Failed to get current file position\n");
    return -1;
  }
  has_karaoke=0;
  strcpy(text,str); /* we'll need to restore str after peeking */
  while (!feof(fin)) {
    ret=sscanf(str,"[%d:%d.%d]%n",&m,&s,&fs,&offset);
    if (ret==3) {
      const char *start=strchr(str+offset,'<');
      if (start) {
        const char *end=strchr(start,'>');
        if (end) {
          /* heuristics say we have enhanced lrc karaoke tags */
          has_karaoke=1;
          break;
        }
      }
    }
    fgets2(str,sizeof(str),fin,1);
  }
  if (fsetpos(fin,&start)<0) {
    fprintf(stderr,"Failed to reset file position\n");
    return -1;
  }
  strcpy(str,text);
  if (has_karaoke) {
    add_kate_karaoke_style(&ki, 255, 255, 255, 255);
    add_kate_karaoke_style(&ki, 255, 128, 128, 255);
  }

  ret=flush_headers(fout);
  if (ret<0) return ret;

  while (!feof(fin) || *str) {
    if (!is_line_empty(str)) {
      f0=f1=-1;
      ret=sscanf(str,"[%d:%d.%n%d%n]%n",&m,&s,&f0,&fs,&f1,&offset);
      if (ret<3 || (f0|f1)<0 || f1<=f0 || (m|s|fs)<0) {
        fprintf(stderr,"Syntax error at line %d: %s\n",line,str);
        return -1;
      }
      t=hmsms2ms(0,m,s,fraction_to_milliseconds(fs,f1-f0));
      if (start_time>=0 && !is_line_empty(text)) {
        ogg_packet op;

        update_stream_time(&k,fout,start_time);

        if (text[strlen(text)-1]=='\n') text[strlen(text)-1]=0;
        km=process_enhanced_lrc_tags(text,start_time,t,line);
        if (km) {
          ret=kate_encode_set_style_index(&k, 0);
          if (ret < 0) {
            fprintf(stderr,"Failed encoding karaoke style - continuing anyway\n");
          }
          ret=kate_encode_set_secondary_style_index(&k, 1);
          if (ret < 0) {
            fprintf(stderr,"Failed encoding karaoke style - continuing anyway\n");
          }
          ret=kate_encode_add_motion(&k,km,1);
          if (ret<0) {
            fprintf(stderr,"Failed to add karaoke motion (%d)\n",ret);
            return ret;
          }
        }
        len=strlen(text);
        ret=kate_text_validate(kate_utf8,text,len+1);
        if (ret<0) {
          if (ret==KATE_E_TEXT) {
            fprintf(stderr,"Text is not valid UTF-8: %s\n",text);
          }
          else {
            fprintf(stderr,"Failed to validate text: %d\n",ret);
          }
          return ret;
        }
        ret=kate_ogg_encode_text_raw_times(&k,start_time,t,text,len,&op);
        if (ret<0) {
          fprintf(stderr,"Failed to add lyrics (%d)\n",ret);
          return ret;
        }
        ret=send_packet(fout,&op,start_time);
        if (ret<0) return ret;
      }
      start_time=t;
      strcpy(text,str+offset);
    }
    fgets2(str,sizeof(str),fin,1);
    ++line;
  }
  return 0;
}

static void print_version(void)
{
  printf("Kate reference encoder - %s\n",kate_get_version_string());
}

static void print_help(const char *argv0)
{
  printf("usage: %s [options] [filename]\n",argv0);
  printf("   -V                  version\n");
  printf("   -h                  help\n");
  printf("   -o <filename>       set output filename\n");
  printf("   -t <type>           set input file type\n");
  printf("       types can be: kate, srt, lrc\n");
  printf("   -l <language>       set stream language\n");
  printf("   -c <category>       set stream category\n");
  printf("   -s <hex number>     set serial number of output stream\n");
  printf("   -R <threshold>      Use repeat packets with given threshold (seconds)\n");
  printf("   -K <threshold>      Use keepalive packets with given threshold (seconds)\n");
  printf("   -C <tag>=<value>    Add comment to the Kate stream\n");
  printf("   -M                  Allow simple markup in SRT files\n");
#ifdef DEBUG
  printf("   -r                  write raw Kate stream (experimental)\n");
#endif
}

#ifdef DEBUG
static void print_rle_stats(void)
{
  int n,total=0;
  extern int kate_rle_stats_overall[8];
  static const char *kate_rle_type_names[8]={
    "empty",
    "basic",
    "delta",
    "stop",
    "start/end",
    "delta/stop",
    "basic/zero",
    "",
  };
  for (n=0;n<8;++n) total+=kate_rle_stats_overall[n];
  if (total) {
    for (n=0;n<8;++n) {
      printf("%s: %d (%.2f%%)\n",kate_rle_type_names[n],kate_rle_stats_overall[n],100.0f*kate_rle_stats_overall[n]/total);
    }
  }
}
#endif

static void set_encoder_comment(kate_comment *kc)
{
  int ret;
  const char *base="kateenc - ";
  const char *version=kate_get_version_string();
  size_t len=strlen(base)+strlen(version)+1;
  char *value=(char*)kate_malloc(len);
  if (!value) {
    fprintf(stderr,"Failed to allocate %lu bytes\n",(unsigned long)len);
    exit(-1);
  }
  sprintf(value,"%s%s",base,version);
  ret=kate_comment_add_tag(kc,"ENCODER",value);
  kate_free(value);
  if (ret<0) {
    fprintf(stderr,"kate_comment_add_tag failed: %d\n",ret);
    exit(-1);
  }
}

int main(int argc,char **argv)
{
  int n,ret,failed=0;
  const char *input_filename=NULL;
  const char *output_filename=NULL;
  const char *input_file_type=NULL;
  uint32_t serial;
  const char *comment;
  const char *arg;
  char *endptr;
  FILE *fin,*fout;
  int allow_srt_markup=0;

  srand(time(NULL)^getpid());
  serial=rand();

  packetno=0;

  ret=kate_comment_init(&kc);
  if (ret<0) {
    fprintf(stderr,"kate_comment_init failed: %d\n",ret);
    exit(-1);
  }
  set_encoder_comment(&kc);
  if (ret<0) {
    fprintf(stderr,"Failed to set encoder tag: %d\n",ret);
    exit(-1);
  }
  ret=kate_info_init(&ki);
  if (ret<0) {
    fprintf(stderr,"kate_comment_init failed: %d\n",ret);
    exit(-1);
  }

  for (n=1;n<argc;++n) {
    if (argv[n][0]=='-') {
      switch (argv[n][1]) {
        case '-':
          if (!strcmp(argv[n],"--version")) {
            print_version();
            exit(0);
          }
          else if (!strcmp(argv[n],"--help")) {
            print_help(argv[0]);
            exit(0);
          }
          else {
            fprintf(stderr,"Invalid option: %s\n",argv[n]);
            exit(-1);
          }
          break;
        case 'V':
          print_version();
          exit(0);
        case 'h':
          print_help(argv[0]);
          exit(0);
        case 'o':
          if (!output_filename) {
            output_filename=eat_arg(argc,argv,&n);
          }
          else {
            fprintf(stderr,"Only one output filename may be given\n");
            exit(-1);
          }
          break;
        case 't':
          if (!input_file_type) {
            input_file_type=eat_arg(argc,argv,&n);
          }
          else {
            fprintf(stderr,"Only one input file type may be given\n");
            exit(-1);
          }
          break;
        case 'l':
          if (!language) {
            language=eat_arg(argc,argv,&n);
          }
          else {
            fprintf(stderr,"Only one language may be given\n");
            exit(-1);
          }
          break;
        case 'c':
          if (!category) {
            category=eat_arg(argc,argv,&n);
          }
          else {
            fprintf(stderr,"Only one category may be given\n");
            exit(-1);
          }
          break;
        case 's':
          serial=strtoul(eat_arg(argc,argv,&n),NULL,16);
          break;
        case 'C':
          comment=eat_arg(argc,argv,&n);
          /* check there's a = sign though */
          if (!strchr(comment,'=')) {
            fprintf(stderr,"comments must be of the form tag=value\n");
            exit(-1);
          }
          ret=kate_comment_add(&kc,comment);
          if (ret<0) {
            fprintf(stderr,"error adding comment \"%s\": %d\n",comment,ret);
            exit(-1);
          }
          break;
        case 'R':
          arg=eat_arg(argc,argv,&n);
          endptr=NULL;
          repeat_threshold=(kate_float)strtod(arg,&endptr);
          if (endptr==arg || *endptr) {
            fprintf(stderr,"error in repeat threshold: %s: should be a floating point value\n",arg);
            exit(-1);
          }
          break;
        case 'K':
          arg=eat_arg(argc,argv,&n);
          endptr=NULL;
          keepalive_threshold=(kate_float)strtod(arg,&endptr);
          if (endptr==arg || *endptr) {
            fprintf(stderr,"error in keepalive threshold: %s: should be a floating point value\n",arg);
            exit(-1);
          }
          break;
        case 'M':
          allow_srt_markup=1;
          break;
#ifdef DEBUG
        case 'r':
          raw=1;
          break;
#endif
        default:
          fprintf(stderr,"Invalid option: %s\n",argv[n]);
          exit(-1);
      }
    }
    else {
      if (!input_filename) {
        input_filename=argv[n];
      }
      else {
        fprintf(stderr,"Only one input filename may be given\n");
        exit(-1);
      }
    }
  }

  if (!input_file_type) {
    fprintf(stderr,"No input file type given\n");
    exit(-1);
  }

  if (!input_filename || !strcmp(input_filename,"-")) {
    fin=stdin;
  }
  else {
    const char *ptr,*end;
    fin=fopen(input_filename,"r");
    if (!fin) {
      fprintf(stderr,"%s: %s\n",input_filename,strerror(errno));
      exit(-1);
    }
    ptr=input_filename;
    end=NULL;
    while (*ptr) {
      if (*ptr=='/' || *ptr=='\\')
        end=ptr;
      ++ptr;
    }
    if (end) {
      /* we found a slash */
      if ((size_t)(end-input_filename+1)>=sizeof(base_path)) {
        fprintf(stderr,"%s too long\n",input_filename);
        exit(-1);
      }
      strncpy(base_path,input_filename,end-input_filename+1);
      base_path[end-input_filename+1]=0; /* just after the last slash */
    }
    else {
      /* we did not found a slash */
      strcpy(base_path,"");
    }
  }

  if (!output_filename || !strcmp(output_filename,"-")) {
#if defined WIN32 || defined _WIN32
    _setmode(_fileno(stdout),_O_BINARY);
#else
#if defined MSDOS || defined __CYGWIN__ || defined __EMX__ || defined OS2 || defined __BORLANDC__
    setmode(fileno(stdout),_O_BINARY);
#endif
#endif
    fout=stdout;
  }
  else {
    fout=fopen(output_filename,"wb");
    if (!fout) {
      fprintf(stderr,"%s: %s\n",output_filename,strerror(errno));
      exit(-1);
    }
  }

  ret=kate_encode_init(&k,&ki);
  if (ret<0) {
    fprintf(stderr,"kate_encode_init failed: %d\n",ret);
    exit(-1);
  }

#ifdef DEBUG
  if (!raw)
#endif
    ogg_stream_init(&os,serial);

  ret=0;
  if (!strcmp(input_file_type,"kate")) {
    ret=convert_kate(fin,fout);
  }
  else if (!strcmp(input_file_type,"srt")) {
    ret=convert_srt(fin,fout,allow_srt_markup);
  }
  else if (!strcmp(input_file_type,"lrc")) {
    ret=convert_lrc(fin,fout);
  }
  else {
    fprintf(stderr,"Invalid format type: %s\n",input_file_type);
    ret=-1;
  }
  if (ret<0) failed=ret;

  if (ret==0) {
    ogg_packet op;
    ret=kate_ogg_encode_finish(&k,-1,&op);
    if (ret<0) {
      fprintf(stderr,"error encoding end packet: %d\n",ret);
      failed=ret;
    }
    else {
      ret=send_packet(fout,&op,-1);
      if (ret<0) {
        fprintf(stderr,"error sending end packet: %d\n",ret);
        failed=ret;
      }
      else {
#ifdef DEBUG
        if (!raw)
#endif
        {
          ret=flush_page(fout);
          if (ret<0) {
            fprintf(stderr,"error flushing page: %d\n",ret);
            failed=ret;
          }
        }
      }
    }
  }

#ifdef DEBUG
  if (!raw)
#endif
    ogg_stream_clear(&os);

  ret=kate_clear(&k);
  if (ret<0) {
    fprintf(stderr,"kate_clear failed: %d\n",ret);
    /* continue anyway */
  }

  ret=kate_info_clear(&ki);
  if (ret<0) {
    fprintf(stderr,"kate_info_clear failed: %d\n",ret);
    /* continue anyway */
  }
  ret=kate_comment_clear(&kc);
  if (ret<0) {
    fprintf(stderr,"kate_comment_clear failed: %d\n",ret);
    /* continue anyway */
  }

  if (fout!=stdout) {
    fclose(fout);
    if (ret<0) {
      int unlink_ret=unlink(output_filename);
      if (unlink_ret<0) {
        fprintf(stderr,"unlink failed (%d) - corrupt file %s will not be deleted\n",ret,output_filename);
      }
    }
  }
  if (fin!=stdin) fclose(fin);

#ifdef DEBUG
  print_rle_stats();
#endif

  return failed?failed:ret;
}
