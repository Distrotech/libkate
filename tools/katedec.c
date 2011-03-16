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
#include <ctype.h>
#include <ogg/ogg.h>
#include "kate/oggkate.h"
#include "kate_internal.h"
#include "ksrt.h"
#include "klrc.h"
#include "kkate.h"
#include "kfuzz.h"
#include "kstream.h"
#include "kread.h"
#include "kstrings.h"
#include "kutil.h"

typedef struct {
  kate_stream_set kate_streams;
  int n_streams;

  const char *output_filename;
  int verbose;
#ifdef DEBUG
  int fuzz;
  unsigned long fuzz_seed;
#endif

  void (*write_start_function)(FILE*);
  void (*write_end_function)(FILE*);
  void (*write_headers_function)(FILE*,const kate_info*,const kate_comment*);
  void (*write_event_function)(FILE*,void*,const kate_event*,ogg_int64_t);
  void *(*new_event_function_data)(void);
  void (*free_event_function_data)(void*);
} ogg_parser_data;

static int ogg_parser_on_page(kate_uintptr_t data,long offset,ogg_page *og)
{
  ogg_parser_data *opd=(ogg_parser_data*)data;
  ogg_packet op;
  int ret;
  kate_stream *ks;

  /* check for hole */
  if (!og) return 0;

  /* add the page in the correct stream */
  if (ogg_page_bos(og)) {
    add_kate_stream(&opd->kate_streams,og,opd->n_streams++);
    ks=find_kate_stream_for_page(&opd->kate_streams,og);
    if (opd->new_event_function_data) ks->data=(*opd->new_event_function_data)();
    if (opd->verbose) {
      printf("found BOS page, serial %08x\n",ogg_page_serialno(og));
    }
  }
  else {
    ks=find_kate_stream_for_page(&opd->kate_streams,og);
  }

  if (ks) {
    while ((ret=ogg_stream_packetout(&ks->os,&op))) {
      if (ret<0) {
        fprintf(stderr,"Hole in data\n");
        break;
      }

      if (ks->init<kstream_data) {
        ret=kate_ogg_decode_headerin(&ks->ki,&ks->kc,&op);
        if (ret>=0) {
          /* we found a Kate bitstream */
          if (op.packetno==0) {
            if (opd->verbose) printf("Bitstream looks like Kate\n");
            ks->init=kstream_header_info;
          }
          if (ret>0) {
            /* we're done parsing headers, go for data */
            if (opd->verbose) {
              printf("Bitstream is Kate (bitstream v%d.%d, language \"%s\" category \"%s\", encoding %d (%s))\n",
                ks->ki.bitstream_version_major,ks->ki.bitstream_version_minor,
                ks->ki.language,
                ks->ki.category,
                ks->ki.text_encoding,
                encoding2text(ks->ki.text_encoding));
            }

            ret=kate_decode_init(&ks->k,&ks->ki);
            if (ret<0) {
              fprintf(stderr,"kate_decode_init failed: %d\n",ret);
              exit(-1);
            }
            ks->init=kstream_data;

            if (!opd->output_filename || !strcmp(opd->output_filename,"-")) {
              if (find_kate_stream_for_file(&opd->kate_streams,stdout)) {
                fprintf(stderr,"Cannot write two Kate streams to the same output, new Kate stream will be ignored\n");
                clear_and_remove_kate_stream(ks,&opd->kate_streams);
                break;
              }
              else {
                ks->fout=stdout;
              }
            }
            else {
              ks->filename=get_filename(opd->output_filename,ks,&opd->kate_streams);
              if (ks->filename) {
                ks->fout=fopen(ks->filename,"w");
                if (!ks->fout) {
                  fprintf(stderr,"%s: %s\n",ks->filename,strerror(errno));
                  exit(-1);
                }
              }
              else {
                /* get_filename returns NULL when it can't generate a filename because no format field
                was given in the output filename and we have more than one Kate stream */
                fprintf(stderr,"Cannot write two Kate streams to the same output, new Kate stream will be ignored\n");
                clear_and_remove_kate_stream(ks,&opd->kate_streams);
                break;
              }
            }

            if (ks) {
              if (opd->write_start_function) (*opd->write_start_function)(ks->fout);

              if (opd->write_headers_function) (*opd->write_headers_function)(ks->fout,&ks->ki,&ks->kc);
            }
          }
        }
        else {
          if (ret==KATE_E_NOT_KATE) {
            /* Bitstream is not Kate, ignore */
          }
          else {
            fprintf(stderr,"kate_decode_headerin: packetno %"PRId64": %d\n",op.packetno,ret);
            ks->ret=ret;
          }
          clear_and_remove_kate_stream(ks,&opd->kate_streams);
          break;
        }
      }
      else {
        ret=kate_ogg_decode_packetin(&ks->k,&op);
        if (ret<0) {
          fprintf(stderr,"error in kate_decode_packetin: %d\n",ret);
          ks->ret=ret;
        }
        else if (ret>0) {
          /* we're done */
          if (opd->write_end_function) (*opd->write_end_function)(ks->fout);
          ks->init=kstream_eos;
        }
        else {
          const kate_event *ev=NULL;
          ret=kate_decode_eventout(&ks->k,&ev);
          if (ret<0) {
            fprintf(stderr,"error in kate_decode_eventout: %d\n",ret);
            ks->ret=ret;
          }
          else if (ret==0) {
            if (opd->write_event_function) {
              (*opd->write_event_function)(ks->fout,ks->data,ev,ogg_page_granulepos(og));
            }
          }
        }
      }
    }
  }

  return 0;
}

static void print_version(void)
{
  printf("Kate reference decoder - %s\n",kate_get_version_string());
}

static void print_help(const char *argv0)
{
  printf("usage: %s [options] [filename]\n",argv0);
  printf("   -V                  version\n");
  printf("   -v                  verbose\n");
  printf("   -h                  help\n");
  printf("   -o <filename>       set output filename\n");
  printf("   -t <type>           set output format (kate (default), srt, lrc)\n");
#ifdef DEBUG
  printf("   -B                  write some bitmaps in /tmp (debug)\n");
  printf("   -f <number>         fuzz testing with given seed (debug)\n");
#endif
}

int main(int argc,char **argv)
{
  size_t bytes_read;
  int ret=-1;
  const char *input_filename=NULL;
  const char *output_filename=NULL;
  const char *output_filename_type=NULL;
  FILE *fin;
  int verbose=0;
  int arg;
  char signature[64]; /* matches the size of the Kate ID header */
  size_t signature_size;
#ifdef DEBUG
  int fuzz=0;
  unsigned long fuzz_seed=0;
#endif
  ogg_parser_data opd;
  ogg_parser_funcs opf;
  size_t n;

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
          print_help(argv[0]);
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
        case 't':
          if (!output_filename_type) {
            output_filename_type=eat_arg(argc,argv,&arg);
          }
          else {
            fprintf(stderr,"Only one output type may be given\n");
            exit(-1);
          }
          break;
#ifdef DEBUG
        case 'B':
          write_bitmaps=1;
          break;
        case 'f':
          fuzz=1;
          fuzz_seed=strtoul(eat_arg(argc,argv,&arg),NULL,10);
          break;
#endif
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

  if (!output_filename_type || !strcmp(output_filename_type,"kate")) {
    opd.write_start_function=&write_kate_start;
    opd.write_headers_function=&write_kate_headers;
    opd.write_end_function=&write_kate_end;
    opd.write_event_function=&write_kate_event;
    opd.new_event_function_data=NULL;
    opd.free_event_function_data=NULL;
  }
  else if (!strcmp(output_filename_type,"srt")) {
    opd.write_start_function=NULL;
    opd.write_headers_function=NULL;
    opd.write_end_function=NULL;
    opd.write_event_function=&write_srt_event;
    opd.new_event_function_data=&new_srt_data;
    opd.free_event_function_data=&free_srt_data;
  }
  else if (!strcmp(output_filename_type,"lrc")) {
    opd.write_start_function=NULL;
    opd.write_headers_function=NULL;
    opd.write_end_function=&write_lrc_end;
    opd.write_event_function=&write_lrc_event;
    opd.new_event_function_data=&new_lrc_data;
    opd.free_event_function_data=&free_lrc_data;
  }
  else {
    fprintf(stderr,"Invalid format type: %s\n",output_filename_type);
    exit(-1);
  }

  fin=open_and_probe_stream(input_filename);
  if (!fin) exit(-1);

#ifdef DEBUG
  /* first, read the first few bytes to know if we have a raw Kate stream
     or a Kate-in-Ogg stream */
  bytes_read=fread(signature,1,sizeof(signature),fin);
  if (bytes_read!=sizeof(signature)) {
    /* A Kate stream's first packet is 64 bytes, so this cannot be one */
    fprintf(stderr,"Failed to read first %lu bytes of stream\n",(unsigned long)sizeof(signature));
    exit(-1);
  }
#else
  bytes_read=0;
#endif

#ifdef DEBUG
  if (!memcmp(signature,"\200kate\0\0\0",8)) {
    /* raw Kate stream */
    kate_state k;
    FILE *fout;
    int ret;
    char *buffer=NULL;
    ogg_int64_t bytes;
    int headers_written=0;
    void *write_event_function_data=NULL;

    if (opd.new_event_function_data) write_event_function_data=(*opd.new_event_function_data)();

    bytes=64;
    buffer=(char*)kate_malloc(bytes);
    if (!buffer) {
      fprintf(stderr,"failed to allocate %"PRId64" bytes\n",bytes);
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

    ret=kate_high_decode_init(&k);
    if (ret<0) {
      fprintf(stderr,"failed to init raw kate packet decoding (%d)\n",ret);
      exit(-1);
    }

    while (1) {
      const kate_event *ev=NULL;
      kate_packet kp;
      kate_packet_wrap(&kp,bytes,buffer);
#ifdef DEBUG
      if (fuzz) fuzz_kate_packet(&fuzz_seed,&kp);
#endif
      ret=kate_high_decode_packetin(&k,&kp,&ev);
      if (ret<0) {
        fprintf(stderr,"failed to decode raw kate packet (%d)\n",ret);
        exit(-1);
      }
      if (k.ki->probe<0 && !headers_written) {
        if (opd.write_start_function) (*opd.write_start_function)(fout);
        if (opd.write_headers_function) (*opd.write_headers_function)(fout,k.ki,NULL);
        headers_written=1;
      }
      if (ret>0) {
        if (opd.write_end_function) (*opd.write_end_function)(fout);
        break; /* last packet decoded */
      }
      if (ev) {
        if (opd.write_event_function) {
          (*opd.write_event_function)(fout,write_event_function_data,ev,-1);
        }
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

    if (opd.free_event_function_data) {
      (*opd.free_event_function_data)(write_event_function_data);
    }

    if (fout!=stdout) fclose(fout);
  }
  else
#endif
  {
    signature_size=bytes_read;

    init_kate_stream_set(&opd.kate_streams);
    opd.n_streams=0;
    opd.verbose=verbose;
#ifdef DEBUG
    opd.fuzz=fuzz;
    opd.fuzz_seed=fuzz_seed;
#endif
    opd.output_filename=output_filename;

    opf.on_page=&ogg_parser_on_page;
    opf.on_hole=NULL;
    ret=parse_ogg_stream(fin,signature,signature_size,opf,(kate_uintptr_t)&opd);

    /* Force EOS on truncated streams */
    for (n=0;n<opd.kate_streams.n_kate_streams;++n) {
      kate_stream *ks=opd.kate_streams.kate_streams[n];
      if (ks->init!=kstream_eos) {
        if (opd.write_end_function) (*opd.write_end_function)(ks->fout);
        ks->init=kstream_eos;
      }
    }

    /* Clear user data */
    for (n=0;n<opd.kate_streams.n_kate_streams;++n) {
      kate_stream *ks=opd.kate_streams.kate_streams[n];
      if (opd.free_event_function_data) (*opd.free_event_function_data)(ks->data);
    }

    clear_kate_stream_set(&opd.kate_streams);
  }

  if (fin!=stdin) fclose(fin);

  return 0;
}
