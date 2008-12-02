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
#include "kate/oggkate.h"
#include "kate_internal.h"
#include "ksrt.h"
#include "klrc.h"
#include "kkate.h"
#include "kfuzz.h"
#include "kstream.h"
#include "kread.h"
#include "kutil.h"

<<<<<<< HEAD:tools/katedec.c
enum { uninitialized, header_info, header_comment, data };

typedef struct {
  ogg_stream_state os;
  kate_state k;
  kate_info ki;
  kate_comment kc;
  int init;
  char *filename;
  FILE *fout;
  unsigned int stream_index;
  int event_index;
  int ret;
} kate_stream;

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

static char *get_filename(const char *basename,const kate_stream *ks,const kate_stream *streams,size_t nstreams)
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
        if (!is_ok_for_filename(ks->ki.category)) {
          fprintf(stderr,"Category '%s' not suitable for using in a filename, using 'INVCAT'",ks->ki.category);
          fcat(&filename,"INVCAT");
        }
        else {
          fcat(&filename,ks->ki.category);
        }
        break;
      case 'l':
        if (!is_ok_for_filename(ks->ki.language)) {
          fprintf(stderr,"Language '%s' not suitable for using in a filename, using 'INVLANG'",ks->ki.language);
          fcat(&filename,"INVLANG");
        }
        else {
          fcat(&filename,ks->ki.language);
        }
        break;
      case 's':
        snprintf(tmp,sizeof(tmp),"%08x",(ogg_uint32_t)ks->os.serialno);
        fcat(&filename,tmp);
        break;
      case 'i':
        snprintf(tmp,sizeof(tmp),"%d",ks->stream_index);
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

=======
>>>>>>> split kate_stream stuff into its own file, and refactor some of it:tools/katedec.c
static void print_version(void)
{
  printf("Kate reference decoder - %s\n",kate_get_version_string());
}

int main(int argc,char **argv)
{
  size_t bytes_read;
  int ret=-1;
  int eos=0;
  const char *input_filename=NULL;
  const char *output_filename=NULL;
  const char *output_filename_type=NULL;
  FILE *fin;
  int verbose=0;
  int arg;
  char signature[64]; /* matches the size of the Kate ID header */
  size_t signature_size;
  int raw;
  char *buffer=NULL;
  ogg_int64_t bytes;
  int headers_written=0;
  int fuzz=0;
  unsigned long fuzz_seed=0;
  unsigned int n_streams=0;
  kate_stream_set kate_streams;
  ogg_sync_state oy;
  ogg_page og;
  ogg_packet op;
  struct lrc_data lrc_data;
  void (*write_start_function)(FILE*);
  void (*write_end_function)(FILE*);
  void (*write_headers_function)(FILE*,const kate_info*,const kate_comment*);
  void (*write_event_function)(FILE*,void*,const kate_event*,ogg_int64_t,int);
  void *write_event_function_data=NULL;

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
          printf("   -t <type>           set output format (kate (default), srt, lrc)\n");
          printf("   -B                  write some bitmaps in /tmp (debug)\n");
          printf("   -f <number>         fuzz testing with given seed (debug)\n");
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
        case 'B':
          write_bitmaps=1;
          break;
        case 'f':
          fuzz=1;
          fuzz_seed=strtoul(eat_arg(argc,argv,&arg),NULL,10);
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
    write_start_function=&write_kate_start;
    write_headers_function=&write_kate_headers;
    write_end_function=&write_kate_end;
    write_event_function=&write_kate_event;
    write_event_function_data=NULL;
  }
  else if (!strcmp(output_filename_type,"srt")) {
    write_start_function=NULL;
    write_headers_function=NULL;
    write_end_function=NULL;
    write_event_function=&write_srt_event;
    write_event_function_data=NULL;
  }
  else if (!strcmp(output_filename_type,"lrc")) {
    write_start_function=NULL;
    write_headers_function=NULL;
    write_end_function=&write_lrc_end;
    write_event_function=&write_lrc_event;
    write_event_function_data=&lrc_data;
    lrc_data.last_event_end_time=(kate_float)-1.0;
  }
  else {
    fprintf(stderr,"Invalid format type: %s\n",output_filename_type);
    exit(-1);
  }

  init_kate_stream_set(&kate_streams);

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
      char *filename=get_filename(output_filename,NULL,&kate_streams);
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
      if (fuzz) fuzz_kate_packet(&fuzz_seed,&kp);
      ret=kate_high_decode_packetin(&k,&kp,&ev);
      if (ret<0) {
        fprintf(stderr,"failed to decode raw kate packet (%d)\n",ret);
        exit(-1);
      }
      if (k.ki->probe<0 && !headers_written) {
        if (write_start_function) (*write_start_function)(fout);
        if (write_headers_function) (*write_headers_function)(fout,k.ki,NULL);
        headers_written=1;
      }
      if (ret>0) {
        if (write_end_function) (*write_end_function)(fout);
        break; /* last packet decoded */
      }
      if (ev) {
        if (write_event_function) {
          (*write_event_function)(fout,write_event_function_data,ev,-1,event_index);
        }
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
    /* we'll assume we're embedded in Ogg */
    raw=0;
    signature_size=bytes_read;
    ogg_sync_init(&oy);

    while (1) {
      buffer=ogg_sync_buffer(&oy,4096);
      if (!buffer) {
        fprintf(stderr,"Failed to get sync buffer\n");
        break;
      }
      if (signature_size>0) {
        memcpy(buffer,signature,signature_size);
        signature_size=0;
      }
      else {
        bytes_read=fread(buffer,1,4096,fin);
      }
      if (bytes_read==0) {
        eos=1;
        break;
      }
      ogg_sync_wrote(&oy,bytes_read);

      while (ogg_sync_pageout(&oy,&og)>0) {
        if (ogg_page_bos(&og)) {
          add_kate_stream(&kate_streams,&og,n_streams++);
        };

        kate_stream *ks=find_kate_stream_for_page(&kate_streams,&og);
        if (ks) {
          ogg_int64_t granpos=ogg_page_granulepos(&og);
          while (ogg_stream_packetout(&ks->os,&op)) {
            if (verbose>=2) printf("Got packet: %ld bytes\n",op.bytes);
            if (ks->init<kstream_data) {
              ret=kate_ogg_decode_headerin(&ks->ki,&ks->kc,&op);
              if (ret>=0) {
                /* we found a Kate bitstream */
                if (op.packetno==0) {
                  if (verbose>=1) printf("Bitstream %08x looks like Kate\n",ogg_page_serialno(&og));
                }
                if (ret>0) {
                  /* we're done parsing headers, go for data */
                  if (verbose>=1) {
                    printf("Bitstream %08x is Kate (\"%s\" \"%s\", encoding %d)\n",
                      ogg_page_serialno(&og),
                      ks->ki.language,
                      ks->ki.category,
                      ks->ki.text_encoding);
                  }
                  if (!output_filename || !strcmp(output_filename,"-")) {
                    const kate_stream *other=find_kate_stream_for_file(&kate_streams,stdout);
                    if (other) {
                      fprintf(stderr,"Cannot write two Kate streams to the same output, new Kate stream will be ignored\n");
                      break;
                    }
                    ks->fout=stdout;
                  }
                  else {
                    ks->filename=get_filename(output_filename,ks,&kate_streams);
                    if (!ks->filename) {
                      /* get_filename returns NULL when it can't generate a filename because no format field
                         was given in the output filename and we have more than one Kate stream */
                      fprintf(stderr,"Cannot write two Kate streams to the same output, new Kate stream will be ignored\n");
                      break;
                    }
                    ks->fout=fopen(ks->filename,"w");
                    if (!ks->fout) {
                      fprintf(stderr,"%s: %s\n",ks->filename,strerror(errno));
                      exit(-1);
                    }
                  }
                  if (write_start_function) (*write_start_function)(ks->fout);

                  ret=kate_decode_init(&ks->k,&ks->ki);
                  if (ret<0) {
                    fprintf(stderr,"kate_decode_init failed: %d\n",ret);
                    exit(-1);
                  }
                  ks->init=kstream_data;
                  if (write_headers_function) (*write_headers_function)(ks->fout,&ks->ki,&ks->kc);
                }
              }
              else {
                if (ret!=KATE_E_NOT_KATE) {
                  fprintf(stderr,"kate_decode_headerin: packetno %lld: %d\n",(long long)op.packetno,ret);
                  ks->ret=ret;
                }
                clear_and_remove_kate_stream(ks,&kate_streams);
              }
            }
            else {
              if (fuzz) fuzz_ogg_packet(&fuzz_seed,&op);
              ret=kate_ogg_decode_packetin(&ks->k,&op);
              if (ret<0) {
                fprintf(stderr,"error in kate_decode_packetin: %d\n",ret);
                ks->ret=ret;
              }
              else if (ret>0) {
                /* we're done */
                if (write_end_function) (*write_end_function)(ks->fout);
                eos=1;
                break;
              }
              else {
                const kate_event *ev=NULL;
                ret=kate_decode_eventout(&ks->k,&ev);
                if (ret<0) {
                  fprintf(stderr,"error in kate_decode_eventout: %d\n",ret);
                  ks->ret=ret;
                }
                else if (ret>0) {
                  /* printf("No event to go with this packet\n"); */
                }
                else if (ret==0) {
                  if (write_event_function) {
                    (*write_event_function)(ks->fout,write_event_function_data,ev,granpos,ks->event_index);
                  }
                  ++ks->event_index;
                }
              }
            }
          }
        }
      }

      //if (eos) break;
    }

    clear_kate_stream_set(&kate_streams);

    ogg_sync_clear(&oy);
  }

  if (fin!=stdin) fclose(fin);

  return 0;
}
