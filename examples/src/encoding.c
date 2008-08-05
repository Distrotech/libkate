/*
  This shows the steps necessary to encode a Kate stream.
  For clarity, error checking is omitted.
  */

#include <stdio.h>
#include <string.h>
#include <ogg/ogg.h>
#include "common.h"

/* All the libkate API is available from the main kate header file: */

#include <kate/oggkate.h>

/*
  We want to control when Ogg pages are output, as Kate is a discontinuous
  codec, so we don't know when the next event will happen, hence the need
  to create a page for every event.
  */

static void flush_page(ogg_stream_state *os)
{
  ogg_page og;
  while (1) {
    int ret=ogg_stream_flush(os,&og);
    if (ret==0) break;
    fwrite(og.header,1,og.header_len,stdout);
    fwrite(og.body,1,og.body_len,stdout);
  }
}

int main()
{
  /* We need an Ogg stream to write to */

  ogg_stream_state os;
  ogg_packet op;

  /*
    First, a kate_info structure needs to be created and setup for the stream to create.
    A kate_comment structure also has to be created.
    Information from both of these will get encoded into the stream headers.
    Last, we also need a kate_state structure, which will be initialized later.
    */

  kate_info ki;
  kate_comment kc;
  kate_state k;

  kate_info_init(&ki);
  kate_comment_init(&kc);

  /*
    The most important part of the kate_info structure on encoding is the granule
    encoding information, which describes how granules and time are mapped.
    Here, we map one granule to one millisecond.
   */

  ki.granule_shift=32;
  ki.gps_numerator=1000;
  ki.gps_denominator=1;

  /* With that done, we can initialize libkate for encoding, and initialize libogg as well: */

  kate_encode_init(&k,&ki);
  ogg_stream_init(&os,0x12345678);

  /* for the benefit of windows, which mangles data otherwise */
  set_binary_file(stdout);

  /*
    Before you can create events, headers need to be sent. Here, we'll just send
    the headers directly, but you will usually want to add regions, styles, etc to
    the headers before doing so:
    */

  while (kate_ogg_encode_headers(&k,&kc,&op)==0) {
    ogg_stream_packetin(&os,&op);
    ogg_packet_clear(&op);
  }
  flush_page(&os);

  /*
    Events can now be created, and we'll just create and send a single one here,
    starting at time 10 seconds, and stopping at time 15 seconds.
    */

#define text "Hello, world!"
  kate_ogg_encode_text(&k,10.0,15.0,text,strlen(text)+1,&op);
  ogg_stream_packetin(&os,&op);
  ogg_packet_clear(&op);
  flush_page(&os);

  /*
    When we're done, we can tell libkate so an "end of stream" packet will be generated,
    and clear the resources we've been using:
    */

  kate_ogg_encode_finish(&k,-1,&op);
  ogg_stream_packetin(&os,&op);
  ogg_packet_clear(&op);
  flush_page(&os);
  ogg_stream_clear(&os);
  kate_clear(&k);
  kate_info_clear(&ki);
  kate_comment_clear(&kc);

  /*
    That's it, we now have created a full kate stream. You may now want to decode it,
    or multiplex it with a Theora video, etc.
    */

  return 0;
}

