/*
  This shows how to use the high level decoding API to easily decode
  a Kate stream.
  For clarity, error checking is omitted.
  For simplicity, the input is assumed to be a single Kate stream.
  Usually, Kate streams will be multiplexed with other streams (either
  other Kate streams, or audio, or video). In these cases, the Ogg
  part will have to be more complex, but the Kate specific code will
  be the same.
  */

#include <stdio.h>
#include <string.h>
#include <ogg/ogg.h>
#include "common.h"

/* All the libkate API is available from the main kate header file: */

#include <kate/kate.h>

int main()
{
  ogg_sync_state oy;
  ogg_stream_state os;
  int init=0;
  ogg_packet op;
  kate_state k;
  const kate_event *ev;
  kate_packet kp;

  /* for the benefit of windows, which mangles data otherwise */
  set_binary_file(stdin);

  /* we initialize ogg and the kate state  */
  ogg_sync_init(&oy);
  int ret=kate_high_decode_init(&k);

  /*
    We then read packets and feed them to the libkate high level API. When
    kate_high_decode_packetin returns a positive number, this signals the
    end of the stream.
    */
  while (1) {
    if (get_packet(&oy,&os,&init,&op)) break;
    kate_packet_wrap(&kp,op.bytes,op.packet);
    if (kate_high_decode_packetin(&k,&kp,&ev)>0) break;
    /* if the event is non NULL, we have an event */
    if (ev) {
      printf("Kate stream has text: %s\n",ev->text);
    }
  }

  /* That's it, we can now cleanup */
  ogg_stream_clear(&os);
  ogg_sync_clear(&oy);

  kate_high_decode_clear(&k);

  return 0;
}

