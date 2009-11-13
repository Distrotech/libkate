/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#define KATE_INTERNAL
#include "kate_internal.h"

#include "kate/kate.h"

/**
  \ingroup granule
  Returns the granule shift for the given stream
  \param ki the kate_info structure describing the granule encoding setup
  \returns >=0 the granule shift
  \returns KATE_E_* error
  */
int kate_granule_shift(const kate_info *ki)
{
  if (!ki) return KATE_E_INVALID_PARAMETER;
  return ki->granule_shift;
}

/**
  \ingroup granule
  Converts a base/offset pair in seconds to a granule position
  \param ki the kate_info structure describing the granule encoding setup
  \param base the base time in seconds to convert
  \param offset the time offset in seconds to convert
  \returns >=0 the granule corresponding to the base/offset pair
  \returns KATE_E_* error
  */
kate_int64_t kate_time_granule(const kate_info *ki,kate_float base,kate_float offset)
{
  kate_int64_t gbase,goffset;
  kate_float actual_base,actual_offset;

  if (!ki) return KATE_E_INVALID_PARAMETER;
  if (base<0 || offset<0) return KATE_E_INVALID_PARAMETER;

  gbase=(kate_int64_t)((base*ki->gps_numerator)/ki->gps_denominator);
  actual_base=gbase*(kate_float)ki->gps_denominator/ki->gps_numerator;
  gbase=(kate_int64_t)((actual_base*ki->gps_numerator)/ki->gps_denominator+(kate_float)0.5);
  actual_offset=offset+(base-actual_base);
  if (actual_offset<0) actual_offset=0;
  goffset=(kate_int64_t)((actual_offset*ki->gps_numerator)/ki->gps_denominator+(kate_float)0.5);
  /* check if the values fit with the granule encoding parameters */
  if (gbase>=(((kate_int64_t)1)<<(63-ki->granule_shift))-1) return KATE_E_LIMIT;
  if (goffset>=(((kate_int64_t)1)<<ki->granule_shift)-1) return KATE_E_LIMIT;
  return (gbase<<ki->granule_shift)|goffset;
}

/**
  \ingroup granule
  Converts a granule position to a base+offset time representation in seconds.
  \param ki the kate_info structure describing the granule encoding setup
  \param granulepos the granulepos to convert to a time representation
  \param base a pointer where to store the base part of the time corresponding to the granulepos
  \param offset a pointer where to store the offset part of the time corresponding to the granulepos
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_granule_split_time(const kate_info *ki,kate_int64_t granulepos,kate_float *base,kate_float *offset)
{
  kate_int64_t gbase,goffset;

  if (!ki || !base || !offset) return KATE_E_INVALID_PARAMETER;
  if (granulepos<0) return KATE_E_INVALID_PARAMETER;

  gbase=granulepos>>ki->granule_shift;
  goffset=granulepos-(gbase<<ki->granule_shift);
  *base=gbase*(kate_float)ki->gps_denominator/ki->gps_numerator;
  *offset=goffset*(kate_float)ki->gps_denominator/ki->gps_numerator;

  return 0;
}

/**
  \ingroup granule
  Converts a granule position to a time in seconds.
  \param ki the kate_info structure describing the granule encoding setup
  \param granulepos the granulepos to convert to a time
  \returns >=0 the time corresponding to the granulepos
  \returns KATE_E_* error
  */
kate_float kate_granule_time(const kate_info *ki,kate_int64_t granulepos)
{
  kate_float base,offset;
  int ret;

  ret=kate_granule_split_time(ki,granulepos,&base,&offset);
  if (ret<0) return (kate_float)ret;
  return base+offset;
}

/**
  \ingroup granule
  Converts a duration to a granule duration
  \param ki the kate_info structure describing the granule encoding setup
  \param duration the duration in seconds to convert to a granulepos offset
  \returns >=0 the time corresponding to the granulepos
  \returns KATE_E_* error
  */
kate_int64_t kate_duration_granule(const kate_info *ki,kate_float duration)
{
  kate_int64_t granule;

  if (!ki) return KATE_E_INVALID_PARAMETER;
  if (duration<0) return KATE_E_INVALID_PARAMETER;

  granule=(kate_int64_t)((duration*ki->gps_numerator)/ki->gps_denominator+(kate_float)0.5);
  if (granule<0) return KATE_E_BAD_GRANULE;
  return granule;
}

/**
  \ingroup granule
  Converts a granule offset position to a time in seconds.
  \param ki the kate_info structure describing the granule encoding setup
  \param duration the granulepos offset to convert to a time
  \returns >=0 the time corresponding to the granulepos offset
  \returns KATE_E_* error
  */
kate_float kate_granule_duration(const kate_info *ki,kate_int64_t duration)
{
  if (!ki) return KATE_E_INVALID_PARAMETER;
  if (duration<0) return KATE_E_INVALID_PARAMETER;

  return duration*(kate_float)ki->gps_denominator/ki->gps_numerator;
}

