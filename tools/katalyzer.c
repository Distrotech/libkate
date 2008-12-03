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
#include <ogg/ogg.h>
#include "kate/oggkate.h"
#include "kate_internal.h"
#include "kstream.h"
#include "kread.h"
#include "kutil.h"

typedef struct {
  kate_stream_set kate_streams;
  int n_streams;

  int verbose;
  const char *output_filename;
} ogg_parser_data;

static void kprintf(kate_stream *ks,const char *format,...)
{
  va_list ap;
  va_start(ap,format);
  if (ks) {
    printf("[%08x] ",(int)ks->os.serialno);
  }
  else {
    printf("[--------] ");
  }
  vprintf(format,ap);
  va_end(ap);
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
    case 0x7f: return "eos packet";

    default: return "unknown";
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
  kprintf(ks,"ogg packet %lld, %ld bytes%s\n",
      (long long)op->packetno,
      (long)op->bytes,
      stype);
}

static int ogg_parser_on_page(kate_uintptr_t data,ogg_page *og)
{
  ogg_parser_data *opd=(ogg_parser_data*)data;
  ogg_packet op;
  int eos=0;
  int ret;

  if (ogg_page_bos(og)) {
    add_kate_stream(&opd->kate_streams,og,opd->n_streams++);
  };

  kate_stream *ks=find_kate_stream_for_page(&opd->kate_streams,og);
  if (ks) {
    ogg_int64_t granpos=ogg_page_granulepos(og);
    kprintf(NULL,"Ogg page, %d bytes, %d packets on this page\n",og->header_len+og->body_len,ogg_page_packets(og));
    while (ogg_stream_packetout(&ks->os,&op)) {
      int is_kate=(ks->init>=kstream_header_info);
      if (!is_kate) {
        kate_packet kp;
        kate_packet_wrap(&kp,op.bytes,op.packet);
        if (kate_decode_is_idheader(&kp)) is_kate=1;
      }
      katalyzer_on_ogg_packet(ks,&op,is_kate);
      if (ks->init<kstream_data) {
        ret=kate_ogg_decode_headerin(&ks->ki,&ks->kc,&op);
        if (ret>=0) {
          /* we found a Kate bitstream */
          if (op.packetno==0) {
            kprintf(ks,"Bitstream looks like Kate\n");
            ks->init=kstream_header_info;
          }
          if (ret>0) {
            /* we're done parsing headers, go for data */
            kprintf(ks,"Bitstream is Kate (bitstream v%d.%d, language \"%s\" category \"%s\", encoding %d (%s))\n",
              ks->ki.bitstream_version_major,ks->ki.bitstream_version_minor,
              ks->ki.language,
              ks->ki.category,
              ks->ki.text_encoding,
              ks->ki.text_encoding==kate_utf8?"UTF-8":"unknown");

            ret=kate_decode_init(&ks->k,&ks->ki);
            if (ret<0) {
              kprintf(ks,"kate_decode_init failed: %d\n",ret);
              exit(-1);
            }
            ks->init=kstream_data;
            kprintf(ks,"headers parsed\n");
          }
        }
        else {
          if (ret==KATE_E_NOT_KATE) {
            kprintf(ks,"Bitstream is not Kate\n");
          }
          else {
            kprintf(ks,"kate_decode_headerin: packetno %lld: %d\n",(long long)op.packetno,ret);
            ks->ret=ret;
          }
          clear_and_remove_kate_stream(ks,&opd->kate_streams);
        }
      }
      else {
        ret=kate_ogg_decode_packetin(&ks->k,&op);
        if (ret<0) {
          kprintf(ks,"error in kate_decode_packetin: %d\n",ret);
          ks->ret=ret;
        }
        else if (ret>0) {
          /* we're done */
          kprintf(ks,"eof packet\n");
          eos=1;
          break;
        }
        else {
          const kate_event *ev=NULL;
          ret=kate_decode_eventout(&ks->k,&ev);
          if (ret<0) {
            kprintf(ks,"error in kate_decode_eventout: %d\n",ret);
            ks->ret=ret;
          }
          else if (ret>0) {
            /* printf("No event to go with this packet\n"); */
          }
          else if (ret==0) {
            kprintf(ks,"event found\n");
            ++ks->event_index;
          }
        }
      }
    }
  }

  return 0;
}

static void print_version(void)
{
  printf("Katalyzer, a Kate stream analyzer - %s\n",kate_get_version_string());
}

int main(int argc,char **argv)
{
  size_t bytes_read;
  int ret=-1;
  const char *input_filename=NULL;
  const char *output_filename=NULL;
  FILE *fin;
  int verbose=0;
  int arg;
  char signature[64]; /* matches the size of the Kate ID header */
  size_t signature_size;
  int raw;
  char *buffer=NULL;
  ogg_int64_t bytes;
  int headers_written=0;
  ogg_parser_data opd;
  ogg_parser_funcs opf;

  for (arg=1;arg<argc;++arg) {
    if (argv[arg][0]=='-') {
      switch (argv[arg][1]) {
        case 'V':
          print_version();
          exit(0);
        case 'h':
          print_version();
          printf("usage: %s [options] [filename]\n",argv[0]);
          printf("   -V                  version\n");
          printf("   -v                  verbose\n");
          printf("   -h                  help\n");
          printf("   -o <filename>       set output filename\n");
          exit(0);
        case 'o':
          if (!output_filename) {
            output_filename=eat_arg(argc,argv,&arg);
          }
          else {
            fprintf(stderr,"Only one output filename may be converted at a time\n");
            exit(-1);
          }
          break;
        case 'v':
          ++verbose;
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

  fin=open_and_probe_stream(input_filename);
  if (!fin) exit(-1);

  /* first, read the first few bytes to know if we have a raw Kate stream
     or a Kate-in-Ogg stream */
  bytes_read=fread(signature,1,sizeof(signature),fin);
  if (bytes_read!=sizeof(signature)) {
    /* A Kate stream's first packet is 64 bytes, so this cannot be one */
    fprintf(stderr,"Failed to read first %zu bytes of stream\n",sizeof(signature));
    exit(-1);
  }

  if (!memcmp(signature,"\200kate\0\0\0",8)) {
    /* raw Kate stream */
    kate_state k;
    FILE *fout;
    int event_index=0;
    int ret;

    bytes=64;
    buffer=(char*)kate_malloc(bytes);
    if (!buffer) {
      fprintf(stderr,"failed to allocate %lld bytes\n",(long long)bytes);
      exit(-1);
    }
    memcpy(buffer,signature,bytes);

    if (!output_filename || !strcmp(output_filename,"-")) {
      fout=stdout;
    }
    else {
      char *filename=get_filename(output_filename,NULL,NULL);
      fout=fopen(filename,"w");
      if (!fout) {
        fprintf(stderr,"%s: %s\n",filename,strerror(errno));
        exit(-1);
      }
      kate_free(filename);
    }

    raw=1;

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
        kprintf(NULL,"headers parsed\n");
        headers_written=1;
      }
      if (ret>0) {
        kprintf(NULL,"eos\n");
        break; /* last packet decoded */
      }
      if (ev) {
        kprintf(NULL,"event found\n");
        ++event_index;
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

    if (fout!=stdout) fclose(fout);
  }
  else {
    raw=0;
    signature_size=bytes_read;

    init_kate_stream_set(&opd.kate_streams);
    opd.n_streams=0;
    opd.verbose=verbose;
    opd.output_filename=output_filename;

    opf.on_page=&ogg_parser_on_page;
    ret=parse_ogg_stream(fin,signature,signature_size,opf,(kate_uintptr_t)&opd);

    clear_kate_stream_set(&opd.kate_streams);
  }

  if (fin!=stdin) fclose(fin);

  return 0;
}
