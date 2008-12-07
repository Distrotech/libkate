/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>
#include "kate/kate.h"
#include "kstream.h"
#include "kstats.h"

void katalyzer_stats_init(katalyzer_stats *stats,kate_stream *ks)
{
  memset(stats,0,sizeof(*stats));
  stats->ks=ks;
}

katalyzer_stats *katalyzer_stats_create(kate_stream *ks)
{
  katalyzer_stats *stats=(katalyzer_stats*)kate_malloc(sizeof(katalyzer_stats));
  if (!stats) return NULL;
  katalyzer_stats_init(stats,ks);
  return stats;
}

void katalyzer_stats_destroy(katalyzer_stats *stats)
{
  kate_free(stats);
}

void katalyzer_stats_add_packet(katalyzer_stats *stats,kate_packet *kp)
{
  unsigned char type=((const unsigned char*)kp->data)[0];

  stats->stream_length+=kp->nbytes;
  ++stats->n_packets_total;
  ++stats->n_packets[type];
  if (stats->n_packets[type]==1) {
    stats->min_size[type]=kp->nbytes;
    stats->max_size[type]=kp->nbytes;
  }
  else {
    if (stats->min_size[type]>kp->nbytes)
      stats->min_size[type]=kp->nbytes;
    if (stats->max_size[type]<kp->nbytes)
      stats->max_size[type]=kp->nbytes;
  }
  stats->total_size[type]+=kp->nbytes;
}

int katalyzer_stats_matches(const katalyzer_stats *stats,const kate_stream *ks)
{
  return ks==stats->ks;
}

