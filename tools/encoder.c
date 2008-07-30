/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#if !defined WIN32 && !defined _WIN32
#include <unistd.h>
#else
#include <io.h>
#include <fcntl.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <ogg/ogg.h>
#include <kate/oggkate.h>
#include "katedesc.h"

/* #define HAVE_LEX_DESTROY */

extern int katedesc_parse(void);
extern int katedesc_restart(FILE*);
extern FILE *katedesc_in;
extern FILE *katedesc_out;
extern int nerrors;

#ifdef HAVE_LEX_DESTROY
extern int katedesc_lex_destroy(void);
#endif

ogg_stream_state os;
ogg_packet op;
kate_state k;
kate_info ki;
kate_comment kc;
ogg_int64_t packetno;
char base_path[4096]="";

static char str[4096];
static int headers_written=0;
static int raw=0;
static unsigned char utf8bom[3]={0xef,0xbb,0xbf};
static unsigned char utf16lebom[2]={0xff,0xfe};
static unsigned char utf16bebom[2]={0xfe,0xff};
static unsigned char utf32lebom[4]={0xff,0xfe,0x00,0x00};
static unsigned char utf32bebom[4]={0x00,0x00,0xfe,0xff};

static const char *language=NULL;

static void flush_page(FILE *f)
{
  ogg_page og;
  while (1) {
    int ret=ogg_stream_flush(&os,&og);
    if (ret==0) break;
    fwrite(og.header,1,og.header_len,f);
    fwrite(og.body,1,og.body_len,f);
  }
}

static void poll_page(FILE *f)
{
  /* discontinuous codec, so we want to flush pages every time,
     as Ogg keeps granules per page, not per packet */
  flush_page(f);
}

void write_headers(FILE *f)
{
  while (1) {
    int ret=kate_ogg_encode_headers(&k,&kc,&op);
    if (ret<0) {
      fprintf(stderr,"error encoding headers: %d\n",ret);
      break;
    }
    if (ret>0) break; /* we're done */

    if (raw) {
      send_packet(f);
    }
    else {
      ogg_stream_packetin(&os,&op);
      ogg_packet_clear(&op);
    }

    /* place all headers on the same page, it keeps oggmerge happy */
    /* poll_page(f); */
  }

  flush_page(f);
}

void send_packet(FILE *f)
{
  if (raw) {
    if (op.packetno>0) {
      ogg_int64_t bytes=op.bytes;
      fwrite(&bytes,1,8,f);
    }
    fwrite(op.packet,1,op.bytes,f);
  }
  else {
    ogg_stream_packetin(&os,&op);
    ogg_packet_clear(&op);
    poll_page(f);
  }
}

void cancel_packet(void)
{
  ogg_packet_clear(&op);
}

static void flush_headers(FILE *f)
{
  if (!headers_written) {
    write_headers(f);
    headers_written=1;
  }
}

static const char *eat_arg(int argc,char **argv,int *n)
{
  if (*n==argc-1) {
    fprintf(stderr,"%s needs an argument\n",argv[*n]);
    exit(-1);
  }
  return argv[++*n];
}

static kate_float hmsms2s(int h,int m,int s,int ms)
{
  return h*3600+m*60+s+ms/(kate_float)1000.0;
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
  return ret;
}

static int convert_kate(FILE *fin,FILE *fout)
{
  katedesc_in=fin;
  katedesc_out=fout;
  katedesc_restart(katedesc_in);
  nerrors=0;
  katedesc_parse();
#ifdef HAVE_LEX_DESTROY
  katedesc_lex_destroy();
#endif
  if (nerrors>0) return -1;
  return 0;
}

static int convert_srt(FILE *fin,FILE *fout)
{
  enum { need_id, need_timing, need_text };
  int need = need_id;
  int last_seen_id=0;
  int ret;
  int id;
  static char text[4096];
  int h0,m0,s0,ms0,h1,m1,s1,ms1;
  kate_float t0=0.0,t1=0.0;
  int line=0;

  flush_headers(fout);

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

  while (!feof(fin)) {
    ++line;
    switch (need) {
      case need_id:
        ret=sscanf(str,"%d\n",&id);
        if (ret!=1) {
          fprintf(stderr,"Syntax error at line %d: %s\n",line,str);
          return -1;
        }
        if (id!=last_seen_id+1) {
          fprintf(stderr,"Error at line %d: non consecutive ids: %s\n",line,str);
          return -1;
        }
        last_seen_id=id;
        need=need_timing;
        strcpy(text,"");
        break;
      case need_timing:
        ret=sscanf(str,"%d:%d:%d%*[,.]%d --> %d:%d:%d%*[,.]%d\n",&h0,&m0,&s0,&ms0,&h1,&m1,&s1,&ms1);
        if (ret!=8) {
          fprintf(stderr,"Syntax error at line %d: %s\n",line,str);
          return -1;
        }
        else {
          t0=hmsms2s(h0,m0,s0,ms0);
          t1=hmsms2s(h1,m1,s1,ms1);
        }
        need=need_text;
        break;
      case need_text:
        if (*str=='\n') {
          if (text[strlen(text)-1]=='\n') text[strlen(text)-1]=0;
          kate_ogg_encode_text(&k,t0,t1,text,strlen(text),&op);
          send_packet(fout);
          need=need_id;
        }
        else {
          strcat(text,str);
        }
        break;
    }
    fgets2(str,sizeof(str),fin,1);
  }
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

static int convert_lrc(FILE *fin,FILE *fout)
{
  int ret;
  static char text[4096];
  unsigned int m,s,ms;
  double t,start_time=-1.0;
  int offset;
  int line=0;

  flush_headers(fout);

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

  /* skip headers */
  while (!feof(fin)) {
    ret=sscanf(str,"[%u:%u.%u]",&m,&s,&ms);
    if (ret==3) break;
    ++line;
    fgets2(str,sizeof(str),fin,1);
  }

  while (!feof(fin)) {
    ++line;
    ret=sscanf(str,"[%u:%u.%u]%n\n",&m,&s,&ms,&offset);
    if (ret!=3) {
      fprintf(stderr,"Syntax error at line %d: %s\n",line,str);
      return -1;
    }
    t=hmsms2s(0,m,s,ms);
    if (start_time>0.0 && !is_line_empty(text)) {
      if (text[strlen(text)-1]=='\n') text[strlen(text)-1]=0;
      kate_ogg_encode_text(&k,start_time,t,text,strlen(text),&op);
      send_packet(fout);
    }
    start_time=t;
    strcpy(text,str+offset);
    fgets2(str,sizeof(str),fin,1);
  }
  return 0;
}

static void print_version(void)
{
  printf("Kate reference encoder - %s\n",kate_get_version_string());
}

static void print_rle_stats(void)
{
#ifdef DEBUG
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
#endif
}

int main(int argc,char **argv)
{
  int n,ret;
  const char *input_filename=NULL;
  const char *output_filename=NULL;
  const char *output_filename_type=NULL;
  uint32_t serial;
  FILE *fin,*fout;

  srand(time(NULL)^getpid());
  serial=rand();

  packetno=0;

  for (n=1;n<argc;++n) {
    if (argv[n][0]=='-') {
      switch (argv[n][1]) {
        case 'V':
          print_version();
          break;
        case 'h':
          print_version();
          printf("usage: %s [options] [filename]\n",argv[0]);
          printf("   -V                  version\n");
          printf("   -h                  help\n");
          printf("   -o <filename>       set output filename\n");
          printf("   -t <type>           set input filename type\n");
          printf("       types can be: kate, srt, lrc\n");
          printf("   -l <language>       set input filename language\n");
          printf("   -s <hex number>     set serial number of output stream\n");
          printf("   -r                  write raw Kate stream\n");
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
          if (!output_filename_type) {
            output_filename_type=eat_arg(argc,argv,&n);
          }
          else {
            fprintf(stderr,"Only one output type may be given\n");
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
        case 's':
          serial=strtoul(eat_arg(argc,argv,&n),NULL,16);
          break;
        case 'r':
          raw=1;
          break;
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

  if (!output_filename_type) {
    fprintf(stderr,"No output filename type given\n");
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
    ptr=end=input_filename;
    while (*ptr) {
      if (*ptr=='/' || *ptr=='\\')
        end=ptr;
      ++ptr;
    }
    if ((size_t)(end-input_filename+1)>=sizeof(base_path)) {
      fprintf(stderr,"%s too long\n",input_filename);
      exit(-1);
    }
    strncpy(base_path,input_filename,end-input_filename+1);
    base_path[end-input_filename+1]=0; /* just after the last slash */
  }

  if (!output_filename || !strcmp(output_filename,"-")) {
#if defined WIN32 || defined _WIN32
    _setmode(_fileno(stdout),_O_BINARY);
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

  kate_comment_init(&kc);
  kate_info_init(&ki);

  kate_encode_init(&k,&ki);

  if (language) kate_info_set_language(&ki,language);

  if (!raw) ogg_stream_init(&os,serial);

  ret=0;
  if (!strcmp(output_filename_type,"kate")) {
    ret=convert_kate(fin,fout);
  }
  else if (!strcmp(output_filename_type,"srt")) {
    ret=convert_srt(fin,fout);
  }
  else if (!strcmp(output_filename_type,"lrc")) {
    ret=convert_lrc(fin,fout);
  }
  else {
    fprintf(stderr,"Invalid format type: %s\n",output_filename_type);
    ret=-1;
  }

  if (ret==0) {
    ret=kate_ogg_encode_finish(&k,-1,&op);
    if (ret<0) {
      fprintf(stderr,"error encoding end packet: %d\n",ret);
    }
    send_packet(fout);
    if (!raw) {
      flush_page(fout);
    }
  }


  if (!raw) ogg_stream_clear(&os);
  kate_clear(&k);

  kate_info_clear(&ki);
  kate_comment_clear(&kc);

  if (fout!=stdout) {
    fclose(fout);
    if (ret<0) unlink(output_filename);
  }
  if (fin!=stdin) fclose(fin);

  print_rle_stats();

  return ret;
}
