#ifndef KATE_kate_bitwise_h_GUARD
#define KATE_kate_bitwise_h_GUARD

#include "kate_internal.h"
#include "kate/kate.h"

typedef struct {
  long endbyte;
  int  endbit;

  unsigned char *buffer;
  unsigned char *ptr;
  long storage;
} kate_pack_buffer;

void kate_pack_writeinit(kate_pack_buffer *b) kate_internal;
void kate_pack_writetrunc(kate_pack_buffer *b,long bits) kate_internal;
void kate_pack_write(kate_pack_buffer *b,unsigned long value,int bits) kate_internal;
void kate_pack_writealign(kate_pack_buffer *b) kate_internal;
void kate_pack_writecopy(kate_pack_buffer *b,void *source,long bits) kate_internal;
void kate_pack_reset(kate_pack_buffer *b) kate_internal;
void kate_pack_writeclear(kate_pack_buffer *b) kate_internal;
void kate_pack_readinit(kate_pack_buffer *b,unsigned char *buf,int bytes) kate_internal;
long kate_pack_look(kate_pack_buffer *b,int bits) kate_internal;
long kate_pack_look1(kate_pack_buffer *b) kate_internal;
void kate_pack_adv(kate_pack_buffer *b,int bits) kate_internal;
void kate_pack_adv1(kate_pack_buffer *b) kate_internal;
long kate_pack_read(kate_pack_buffer *b,int bits) kate_internal;
long kate_pack_read1(kate_pack_buffer *b) kate_internal;
long kate_pack_bytes(kate_pack_buffer *b) kate_internal;
long kate_pack_bits(kate_pack_buffer *b) kate_internal;
unsigned char *kate_pack_get_buffer(kate_pack_buffer *b) kate_internal;

long kate_pack_readable_bits(kate_pack_buffer *b) kate_internal;

#endif
