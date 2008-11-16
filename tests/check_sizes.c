#include <stdint.h>
#include <stdio.h>
#include "kate/kate.h"

static void ksz_print(const char *type,size_t sz)
{
  printf("sizeof %-24s   %6zu      ",type,sz);
}

#define KSZA(type,sz) \
  do { \
    ksz_print(#type,sizeof(type)); \
    if (sz!=sizeof(type)) { \
      printf("ERROR (expected %zu)",sz); \
      ret=1; \
    } \
    else { \
      printf("OK"); \
    } \
    printf("\n"); \
  } while(0)

#if LONG_MAX==9223372036854775807L
#define KSZ(type,sz32,sz64) KSZA(type,sz64)
#else
#define KSZ(type,sz32,sz64) KSZA(type,sz32)
#endif

int main()
{
  int ret=0;
  KSZ(kate_region,52,80);
  KSZ(kate_color,4,4);
  KSZ(kate_style,96,136);
  KSZ(kate_curve,32,64);
  KSZ(kate_motion,48,80);
  KSZ(kate_palette,16,32);
  KSZ(kate_bitmap,40,56);
  KSZ(kate_font_range,32,56);
  KSZ(kate_font_mapping,32,64);
  KSZ(kate_info,216,400);
  KSZ(kate_state,32,64);
  KSZ(kate_comment,16,32);
  KSZ(kate_event,176,312);
  KSZ(kate_tracker,336,464);
  KSZ(kate_packet,8,16);
  return ret;
}

