/* Copyright (C) 2008, 2009 Vincent Penquerc'h.
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
#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif
#include <ogg/ogg.h>
#include "kate/oggkate.h"
#include "kstream.h"
#include "kread.h"
#include "kutil.h"
#include "kstrings.h"
#include "kstats.h"

typedef struct {
  kate_stream_set kate_streams;
  int n_streams;

  katalyzer_stats **stats;
  size_t nstats;
} ogg_parser_data;

typedef enum {
  klt_misc,
  klt_error,
  klt_packet,
  klt_container,
  klt_timing,
  klt_text,
  klt_event,
  klt_dump,
  klt_stats,
} katalyzer_log_type;

static const struct {
  katalyzer_log_type type;
  char id;
  int by_default;
  const char *tag;
  const char *desc;
} log_types_info[]={
  { klt_misc,      '-', 1, "misc",  "All the rest" },
  { klt_error,     'e', 1, "error", "Errors" },
  { klt_packet,    'p', 1, "pack",  "Packet information" },
  { klt_container, 'c', 0, "cont",  "Container specific information (eg, Ogg)" },
  { klt_timing,    'T', 1, "time",  "Timing information" },
  { klt_text,      't', 1, "text",  "Text information" },
  { klt_event,     'v', 0, "event", "Event information" },
  { klt_dump,      'd', 0, "dump",  "Dump pages and packets" },
  { klt_stats,     'S', 0, "stats", "Statistics about the Kate stream" },
};
static char *log_types=NULL;

static int is_log_type_enabled(katalyzer_log_type type)
{
  return (strchr(log_types,log_types_info[type].id)!=NULL);
}

static void print_log_header(const kate_stream *ks,katalyzer_log_type type)
{
  printf("[%-5s] ",log_types_info[type].tag);
  if (ks) {
    printf("[%08x] ",(int)ks->os.serialno);
  }
  else {
    printf("[--------] ");
  }
}

static void kprintf(const kate_stream *ks,katalyzer_log_type type,const char *format,...)
{
  va_list ap;

  if (!is_log_type_enabled(type)) return;

  print_log_header(ks,type);
  va_start(ap,format);
  vprintf(format,ap);
  va_end(ap);
  fflush(stdout);
}

static const char *packet_type_to_string(unsigned char type)
{
  switch (type) {
    case 0x80: return "ID header";
    case 0x81: return "Vorbis comment header";
    case 0x82: return "regions header";
    case 0x83: return "styles header";
    case 0x84: return "curves header";
    case 0x85: return "motions header";
    case 0x86: return "palettes header";
    case 0x87: return "bitmaps header";
    case 0x88: return "font ranges";

    case 0x00: return "text data";
    case 0x01: return "keepalive";
    case 0x02: return "repeat";
    case 0x7f: return "eos packet";

    default: return "unknown";
  }
}

static void katalyzer_dump_data(kate_stream *ks,katalyzer_log_type type,const char *header,const unsigned char *data,size_t len)
{
  size_t offset=0;

  if (!strchr(log_types,log_types_info[type].id)) return;

  print_log_header(ks,type);
  printf("%s (%lu bytes):\n",header?header:"data",(unsigned long)len);

  while (len>0) {
    size_t n,limit;
    print_log_header(ks,type);
    printf("%08lx:  ",(unsigned long)offset);
    limit=len>16?16:len;
    for (n=0;n<16;++n) {
      if (n<limit)
        printf("%02x ",data[n]);
      else
        printf("   ");
      if (n==7) printf("  ");
    }
    printf("    ");
    for (n=0;n<limit;++n) printf("%c",isprint(data[n])?data[n]:'.');
    printf("\n");
    len-=limit;
    offset+=limit;
    data+=limit;
  }
}

static void katalyzer_on_ogg_packet(kate_stream *ks,ogg_packet *op,int is_kate)
{
  char stype[64];

  if (is_kate && op->bytes>0) {
    snprintf(stype,sizeof(stype),", type %02x (%s)",op->packet[0],packet_type_to_string(op->packet[0]));
  }
  else {
    strcpy(stype,"");
  }
  kprintf(ks,klt_packet,"packet %"PRId64", %ld bytes%s\n",
      op->packetno,
      (long)op->bytes,
      stype);

  katalyzer_dump_data(ks,klt_dump,"Packet data",op->packet,op->bytes);
}

static size_t get_num_glyphs(const kate_event *ev)
{
  size_t nglyphs=0;
  const char *text=ev->text;
  size_t rlen0=ev->len0;
  while (kate_text_get_character(ev->text_encoding,&text,&rlen0)>0)
    ++nglyphs;
  return nglyphs;
}

static void katalyzer_on_event(kate_stream *ks,const kate_event *ev)
{
  int ret;

  kprintf(ks,klt_misc,"An event was found\n");

  kprintf(ks,klt_timing,"event start: %f (%"PRId64")\n",ev->start_time,ev->start);
  kprintf(ks,klt_timing,"event end: %f\n",ev->end_time);
  kprintf(ks,klt_timing,"event duration: %f (%"PRId64")\n",ev->end_time-ev->start_time,ev->duration);
  kprintf(ks,klt_timing,"event backlink: %f (%"PRId64")\n",kate_granule_duration(ev->ki,ev->backlink),ev->backlink);

  kprintf(ks,klt_text,"Text encoding %d (%s)\n",ev->text_encoding,encoding2text(ev->text_encoding));
  kprintf(ks,klt_text,"Text language \"%s\" (%s)\n",ev->language?ev->language:ev->ki->language,ev->language?"overridden":"default");
  kprintf(ks,klt_text,"Text directionality %d (%s)\n",ev->text_directionality,directionality2text(ev->text_directionality));

  ret=kate_text_validate(ev->text_encoding,ev->text,ev->len0);
  if (ret<0) {
    kprintf(ks,klt_error,"Text length %lu bytes, invalid\n",(unsigned long)ev->len);
  }
  else {
    kprintf(ks,klt_text,"Text length %lu bytes, %lu glyphs\n",(unsigned long)ev->len,(unsigned long)get_num_glyphs(ev));
  }

  katalyzer_dump_data(ks,klt_text,"Event text",(const unsigned char*)ev->text,ev->len);
}

static void add_stats(ogg_parser_data *opd,kate_stream *ks)
{
  katalyzer_stats *stats=katalyzer_stats_create(ks);
  if (stats) {
    opd->stats=(katalyzer_stats**)kate_realloc(opd->stats,(opd->nstats+1)*sizeof(katalyzer_stats*));
    if (opd->stats) {
      opd->stats[opd->nstats++]=stats;
    }
  }
}

static katalyzer_stats *find_stats(ogg_parser_data *opd,const kate_stream *ks)
{
  size_t n;

  for (n=0;n<opd->nstats;++n) {
    katalyzer_stats *stats=opd->stats[n];
    if (katalyzer_stats_matches(stats,ks)) {
      return stats;
    }
  }

  return NULL;
}

static void add_to_stats(ogg_parser_data *opd,kate_stream *ks,ogg_packet *op)
{
  katalyzer_stats *stats=find_stats(opd,ks);
  if (stats) {
    kate_packet kp;
    kate_packet_wrap(&kp,op->bytes,op->packet);
    katalyzer_stats_add_packet(stats,&kp);
  }
}

static int ogg_parser_on_page(kate_uintptr_t data,long offset,ogg_page *og)
{
  ogg_parser_data *opd=(ogg_parser_data*)data;
  ogg_packet op;
  int ret;
  kate_stream *ks;
  int is_kate;

  /* add the page in the correct stream */
  if (ogg_page_bos(og)) {
    add_kate_stream(&opd->kate_streams,og,opd->n_streams++);
    ks=find_kate_stream_for_page(&opd->kate_streams,og);
    if (ks) add_stats(opd,ks);
  }
  else {
    ks=find_kate_stream_for_page(&opd->kate_streams,og);
  }

  if (ks) {
    kprintf(NULL,klt_container,"Ogg page at offset %lx, granpos %016"PRIx64", %d bytes, %d packets on this page\n",
        offset,ogg_page_granulepos(og),
        og->header_len+og->body_len,ogg_page_packets(og));

    katalyzer_dump_data(ks,klt_dump,"Page header",og->header,og->header_len);
    katalyzer_dump_data(ks,klt_dump,"Page body",og->body,og->body_len);

    while ((ret=ogg_stream_packetout(&ks->os,&op))) {
      if (ret<0) {
        kprintf(ks,klt_error,"Hole in data\n");
        continue;
      }
      is_kate=(ks->init>=kstream_header_info);
      if (!is_kate) {
        kate_packet kp;
        kate_packet_wrap(&kp,op.bytes,op.packet);
        if (kate_decode_is_idheader(&kp)) is_kate=1;
      }
      katalyzer_on_ogg_packet(ks,&op,is_kate);

      add_to_stats(opd,ks,&op);

      if (ks->init<kstream_data) {
        ret=kate_ogg_decode_headerin(&ks->ki,&ks->kc,&op);
        if (ret>=0) {
          /* we found a Kate bitstream */
          if (op.packetno==0) {
            kprintf(ks,klt_misc,"Bitstream looks like Kate\n");
            ks->init=kstream_header_info;
          }
          if (ret>0) {
            /* we're done parsing headers, go for data */
            kprintf(ks,klt_misc,"Bitstream is Kate (bitstream v%d.%d, language \"%s\" category \"%s\", encoding %d (%s))\n",
              ks->ki.bitstream_version_major,ks->ki.bitstream_version_minor,
              ks->ki.language,
              ks->ki.category,
              ks->ki.text_encoding,
              encoding2text(ks->ki.text_encoding));

            ret=kate_decode_init(&ks->k,&ks->ki);
            if (ret<0) {
              kprintf(ks,klt_error,"kate_decode_init failed: %d\n",ret);
              exit(-1);
            }
            ks->init=kstream_data;
            kprintf(ks,klt_misc,"headers parsed\n");
          }
        }
        else {
          if (ret==KATE_E_NOT_KATE) {
            kprintf(ks,klt_misc,"Bitstream is not Kate\n");
          }
          else {
            kprintf(ks,klt_error,"kate_decode_headerin: packetno %"PRId64": %d\n",op.packetno,ret);
            ks->ret=ret;
          }
          clear_and_remove_kate_stream(ks,&opd->kate_streams);
          break;
        }
      }
      else {
        ret=kate_ogg_decode_packetin(&ks->k,&op);
        if (ret<0) {
          kprintf(ks,klt_error,"error in kate_decode_packetin: %d\n",ret);
          ks->ret=ret;
        }
        else if (ret>0) {
          /* we're done */
          kprintf(ks,klt_misc,"eof packet\n");
          break;
        }
        else {
          const kate_event *ev=NULL;
          ret=kate_decode_eventout(&ks->k,&ev);
          if (ret<0) {
            kprintf(ks,klt_error,"error in kate_decode_eventout: %d\n",ret);
            ks->ret=ret;
          }
          else if (ret>0) {
            kprintf(ks,klt_event,"No event to go with this packet\n");
          }
          else if (ret==0) {
            katalyzer_on_event(ks,ev);
          }
        }
      }
    }
  }

  return 0;
}

static int ogg_parser_on_hole(kate_uintptr_t data,long offset,long size)
{
  (void)data;
  kprintf(NULL,klt_error,"Hole in data at offset %lx: %ld bytes\n",offset,size);
  return 0;
}

static void katalyzer_output_stats(const katalyzer_stats *stats)
{
  const kate_stream *ks=stats->ks;
  int n;

  kprintf(ks,klt_stats,"Statistics:\n");
  kprintf(ks,klt_stats,"Stream: %"PRId64" bytes, %"PRId64" packets\n",stats->stream_length,stats->n_packets_total);
  for (n=0;n<256;++n) {
    int type=n^0x80;
    if (stats->n_packets[type]==0) continue;
    kprintf(ks,klt_stats,"Packet type %02x: %"PRId64" packets, size %"PRId64" - %"PRId64" (acc %"PRId64", %.2f%% total)\n",
        type,stats->n_packets[type],
        stats->min_size[type],stats->max_size[type],
        stats->total_size[type],stats->total_size[type]*100.0f/stats->stream_length);
  }
}

static void katalyzer_output_stats_for_stream(ogg_parser_data *opd,kate_stream *ks)
{
  const katalyzer_stats *stats=find_stats(opd,ks);
  if (stats) {
    katalyzer_output_stats(stats);
  }
}

static void output_all_stats(ogg_parser_data *opd)
{
  size_t n;

  for (n=0;n<opd->nstats;++n) {
    const katalyzer_stats *stats=opd->stats[n];
    const kate_stream *ks=stats->ks;
    int is_kate=(ks->init>=kstream_header_info);
    if (is_kate) {
      katalyzer_output_stats(opd->stats[n]);
    }
  }
}

static int type_sorter(const void *e1,const void *e2)
{
  const char *c1=(const char*)e1;
  const char *c2=(const char*)e2;
  return *c1-*c2;
}

static void add_log_types(char **log_types,const char *new_log_types)
{
  char *ptr,*w,prevc=0;

  if (*log_types) {
    *log_types=(char*)kate_realloc(*log_types,strlen(*log_types)+strlen(new_log_types)+1);
  }
  else {
    *log_types=(char*)kate_malloc(strlen(new_log_types)+1);
    if (*log_types) **log_types=0;
  }

  if (!*log_types) {
    fprintf(stderr,"Failed to allocate memory for log types\n");
    exit(-1);
  }
  strcat(*log_types,new_log_types);

  /* now sort them */
  qsort(*log_types,strlen(*log_types),1,&type_sorter);

  /* uniq */
  ptr=w=*log_types;
  while (*ptr) {
    if (*ptr!=prevc) *w++=*ptr;
    prevc=*ptr++;
  }
  *w=0;
}

static char *get_default_log_types(void)
{
  size_t n,idx;
  char *types=(char*)kate_malloc(sizeof(log_types_info)/sizeof(log_types_info[0])+1);
  if (!types) {
    fprintf(stderr,"Failed to allocate memory for default log types\n");
    exit(-1);
  }
  for (n=0,idx=0;n<sizeof(log_types_info)/sizeof(log_types_info[0]);++n) {
    if (log_types_info[n].by_default) {
      types[idx++]=log_types_info[n].id;
    }
  }
  types[idx]=0;
  return types;
}

static char *get_all_log_types(void)
{
  size_t n;
  char *types=(char*)kate_malloc(sizeof(log_types_info)/sizeof(log_types_info[0])+1);
  if (!types) {
    fprintf(stderr,"Failed to allocate memory for all log types\n");
    exit(-1);
  }
  for (n=0;n<sizeof(log_types_info)/sizeof(log_types_info[0]);++n) {
    types[n]=log_types_info[n].id;
  }
  types[n]=0;
  return types;
}

static void init_ogg_parser_data(ogg_parser_data *opd)
{
  init_kate_stream_set(&opd->kate_streams);
  opd->n_streams=0;
  opd->stats=NULL;
  opd->nstats=0;
}

static void clear_ogg_parser_data(ogg_parser_data *opd)
{
  size_t n;

  clear_kate_stream_set(&opd->kate_streams);
  for (n=0;n<opd->nstats;++n) katalyzer_stats_destroy(opd->stats[n]);
  kate_free(opd->stats);
}

static void print_version(void)
{
  printf("Katalyzer, a Kate stream analyzer - %s\n",kate_get_version_string());
}

static void print_help(const char *argv0)
{
  size_t n;

  printf("usage: %s [options] [filename]\n",argv0);
  printf("   -V                  version\n");
  printf("   -h                  help\n");
  printf("   -l <types>          select information to log\n");
  printf("   -l!                 log everything\n");
  for (n=0;n<sizeof(log_types_info)/sizeof(log_types_info[0]);++n) {
    printf("       %c        %s (default %s)\n",
        log_types_info[n].id,
        log_types_info[n].desc,
        log_types_info[n].by_default?"on":"off");
  }
}

int main(int argc,char **argv)
{
  size_t bytes_read;
  int ret=-1;
  const char *input_filename=NULL;
  const char *new_log_types;
  FILE *fin;
  int arg;
  char signature[64]; /* matches the size of the Kate ID header */
  size_t signature_size;
  char *buffer=NULL;
  ogg_int64_t bytes;
  int headers_written=0;
  ogg_parser_data opd;
  ogg_parser_funcs opf;

  for (arg=1;arg<argc;++arg) {
    if (argv[arg][0]=='-') {
      switch (argv[arg][1]) {
        case '-':
          if (!strcmp(argv[arg],"--version")) {
            print_version();
            exit(0);
          }
          else if (!strcmp(argv[arg],"--help")) {
            print_help(argv[0]);
            exit(0);
          }
          else {
            fprintf(stderr,"Invalid option: %s\n",argv[arg]);
            exit(-1);
          }
          break;
        case 'V':
          print_version();
          exit(0);
        case 'h':
          print_version();
          print_help(argv[0]);
          exit(0);
        case 'l':
          switch (argv[arg][2]) {
            case '!':
              if (log_types) kate_free(log_types);
              log_types=get_all_log_types();
              break;
            case 0:
              new_log_types=eat_arg(argc,argv,&arg);
              add_log_types(&log_types,new_log_types);
              break;
            default:
              add_log_types(&log_types,argv[arg]+2);
              break;
          }
          break;
        default:
          fprintf(stderr,"Invalid option: %s\n",argv[arg]);
          exit(-1);
      }
    }
    else {
      if (!input_filename) {
        input_filename=argv[arg];
      }
      else {
        fprintf(stderr,"Only one input filename may be converted at a time\n");
        exit(-1);
      }
    }
  }

  /* defaults if no log types are specified */
  if (!log_types) log_types=get_default_log_types();

  fin=open_and_probe_stream(input_filename);
  if (!fin) exit(-1);

  /* first, read the first few bytes to know if we have a raw Kate stream
     or a Kate-in-Ogg stream */
  bytes_read=fread(signature,1,sizeof(signature),fin);
  if (bytes_read!=sizeof(signature)) {
    /* A Kate stream's first packet is 64 bytes, so this cannot be one */
    fprintf(stderr,"Failed to read first %lu bytes of stream\n",(unsigned long)sizeof(signature));
    exit(-1);
  }

  if (!memcmp(signature,"\200kate\0\0\0",8)) {
    /* raw Kate stream */
    kate_state k;
    int ret;

    bytes=64;
    buffer=(char*)kate_malloc(bytes);
    if (!buffer) {
      fprintf(stderr,"failed to allocate %"PRId64" bytes\n",bytes);
      exit(-1);
    }
    memcpy(buffer,signature,bytes);

    ret=kate_high_decode_init(&k);
    if (ret<0) {
      fprintf(stderr,"failed to init raw kate packet decoding (%d)\n",ret);
      exit(-1);
    }

    while (1) {
      const kate_event *ev=NULL;
      kate_packet kp;
      kate_packet_wrap(&kp,bytes,buffer);
      ret=kate_high_decode_packetin(&k,&kp,&ev);
      if (ret<0) {
        fprintf(stderr,"failed to decode raw kate packet (%d)\n",ret);
        exit(-1);
      }
      if (k.ki->probe<0 && !headers_written) {
        kprintf(NULL,klt_misc,"headers parsed\n");
        headers_written=1;
      }
      if (ret>0) {
        kprintf(NULL,klt_misc,"eos\n");
        break; /* last packet decoded */
      }
      if (ev) {
        kprintf(NULL,klt_misc,"event found\n");
      }

      /* all subsequent packets are prefixed with 64 bits (signed) of the packet length in bytes */
      ret=read_raw_size_and_packet(fin,&buffer,&bytes);
      if (ret<0) exit(-1);
    }

    ret=kate_high_decode_clear(&k);
    if (ret<0) {
      fprintf(stderr,"kate_high_decode_clear failed: %d\n",ret);
      /* continue anyway */
    }
    kate_free(buffer);
  }
  else {
    signature_size=bytes_read;

    init_ogg_parser_data(&opd);

    opf.on_page=&ogg_parser_on_page;
    opf.on_hole=&ogg_parser_on_hole;
    ret=parse_ogg_stream(fin,signature,signature_size,opf,(kate_uintptr_t)&opd);

    output_all_stats(&opd);
    clear_ogg_parser_data(&opd);
  }

  if (log_types) kate_free(log_types);
  if (fin!=stdin) fclose(fin);

  return 0;
}
