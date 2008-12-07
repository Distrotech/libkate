/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#ifndef KATE_kstats_h_GUARD
#define KATE_kstats_h_GUARD

#include "kate/kate.h"

struct kate_stream;

typedef struct {
  kate_stream *ks;
  kate_uint64_t stream_length;
  kate_uint64_t n_packets_total;
  kate_uint64_t n_packets[256];
  kate_uint64_t min_size[256];
  kate_uint64_t max_size[256];
  kate_uint64_t total_size[256];
} katalyzer_stats;

extern katalyzer_stats *katalyzer_stats_create(struct kate_stream *ks);
extern void katalyzer_stats_destroy(katalyzer_stats *stats);
extern void katalyzer_stats_add_packet(katalyzer_stats *stats,kate_packet *kp);
extern int katalyzer_stats_matches(const katalyzer_stats *stats,const kate_stream *ks);

#endif

