/*
  This shows the steps necessary to encode a Kate stream.
  For clarity, error checking is omitted.
  For simplicity, the input is assumed to be a single Kate stream.
  Usually, Kate streams will be multiplexed with other streams (either
  other Kate streams, or audio, or video). In these cases, the Ogg
  part will have to be more complex, but the Kate specific code will
  be the same.
  */

#include <stdio.h>
#include <string.h>

/* All the libkate API is available from the main kate header file: */

#include <kate/oggkate.h>

/*
   This function returns the next packet, reading data as necessary
  */

static int get_packet(ogg_sync_state *oy,ogg_stream_state *os,int *init,ogg_packet *op)
{
  char *buffer;
  size_t bytes;
  ogg_page og;

  /* try to get a packet from the stream */
  if (*init && ogg_stream_packetout(os,op)) return 0;

  /* read data and feed the pages to ogg */
  buffer=ogg_sync_buffer(oy,4096);
  bytes=fread(buffer,1,4096,stdin);
  if (bytes==0) return 1; /* we're done */
  ogg_sync_wrote(oy,bytes);
  while (ogg_sync_pageout(oy,&og)>0) {
    if (ogg_page_bos(&og)) {
      ogg_stream_init(os,ogg_page_serialno(&og));
      *init=1;
    }
    ogg_stream_pagein(os,&og);
  }

  /* try again with the new data */
  return get_packet(oy,os,init,op);
}

int main()
{
  ogg_sync_state oy;
  ogg_stream_state os;
  int init=0;
  ogg_packet op;
  kate_state k;
  kate_info ki;
  kate_comment kc;
  const kate_event *ev;

  /* we initialize ogg and kate info/comment structures */
  ogg_sync_init(&oy);
  kate_info_init(&ki);
  kate_comment_init(&kc);

  /*
    First, read the headers, which must appear first in a Kate stream. When
    kate_decode_header returns a positive number, all headers have been seen
    and we're ready to decode data.
    */
  do {
    get_packet(&oy,&os,&init,&op);
  } while (kate_ogg_decode_headerin(&ki,&kc,&op)==0);

  /*
    We now have all the information we need from the headers, so we can
    initialize kate for decoding
    */
  kate_decode_init(&k,&ki);

  /*
    We can now read data, until kate_decode_packetin returns a positive
    number, signaling the end of the stream
    */
  while (1) {
    if (get_packet(&oy,&os,&init,&op)) break;
    if (kate_ogg_decode_packetin(&k,&op)>0) break;

    /* we may have an event (eg, text) */
    if (kate_decode_eventout(&k,&ev)==0) {
      printf("Kate stream has text: %s\n",ev->text);
    }
  }

  /* That's it, we can now cleanup */
  ogg_stream_clear(&os);
  ogg_sync_clear(&oy);

  kate_clear(&k);
  kate_info_clear(&ki);
  kate_comment_clear(&kc);

  return 0;
}

