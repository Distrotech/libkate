/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#define KATE_INTERNAL
#include "kate_internal.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include "kate_meta.h"
#include "kate/kate.h"

static kate_float kate_catmull_rom(kate_float t,const kate_float *pts,int k0,int k1,int k2,int k3)
{
  const kate_float t2=t*t,t3=t2*t;

  return (
    (2*pts[k1*2])
    + t * (pts[k2*2]-pts[k0*2])
    + t2 * (-pts[k3*2]+4*pts[k2*2]-5*pts[k1*2]+2*pts[k0*2])
    + t3 * (pts[k3*2]-3*pts[k2*2]+3*pts[k1*2]-pts[k0*2])
  )/2;
}

static kate_float kate_bezier_cubic(kate_float t,const kate_float *pts)
{
  const kate_float t2=t*t,t3=t2*t,it=1-t,it2=it*it,it3=it2*it;

  return it3*pts[0*2]
         +3*t*it2*pts[1*2]
         +3*t2*it*pts[2*2]
         +t3*pts[3*2];
}

static kate_float kate_bspline(kate_float t,const kate_float *pts,int k0,int k1,int k2,int k3)
{
  const kate_float t2=t*t,t3=t2*t,it=1-t,it2=it*it,it3=it2*it;

  return (
           it3*pts[k0*2]
           +(3*t3-6*t2+4)*pts[k1*2]
           +(-3*t3+3*t2+3*t+1)*pts[k2*2]
           +t3*pts[k3*2]
         )/6;
}

/**
  Returns the point defined by the given curve at the given time.
  t will be between 0 and 1
  \param kc the curve to get the point from
  \param t the time at which the point should be taken (between 0 and motion duration)
  \param x a pointer to the first coordinate of the computed point (may be NULL)
  \param y a pointer to the second coordinate of the computed point (may be NULL)
  \returns 0 success
  \returns 1 no point at this time in the curve
  \returns KATE_E_* error
  */
int kate_curve_get_point(const kate_curve *kc,kate_float t,kate_float *x,kate_float *y)
{
  int nsegs,n;
  kate_float T,t0,t1;

  if (!kc) return KATE_E_INVALID_PARAMETER;
  if (t<(kate_float)-0.001 || t>(kate_float)1.001) return KATE_E_INVALID_PARAMETER;
  if (t<0) t=0;
  if (t>1) t=1;
  /* x and y may be NULL */

  switch (kc->type) {
    case kate_curve_none:
      /* the motion can be discontinuous */
      return 1;

    case kate_curve_static:
      if (x) *x=kc->pts[0];
      if (y) *y=kc->pts[1];
      return 0;

    case kate_curve_linear:
      /* find the segment we're on */
      nsegs=kc->npts-1;
      if (nsegs<1) return KATE_E_INIT;
      n=t*nsegs;
      /* ensure it doesn't overflow */
      if (n<0) n=0;
      if (n>=nsegs) n=nsegs-1;
      /* get a 0-1 t on this segment */
      t0=n/(kate_float)nsegs;
      t1=(n+1)/(kate_float)nsegs;
      T=(t-t0)/(t1-t0);
      if (x) *x=T*kc->pts[(n+1)*2+0]+(1-T)*kc->pts[n*2+0];
      if (y) *y=T*kc->pts[(n+1)*2+1]+(1-T)*kc->pts[n*2+1];
      return 0;

    case kate_curve_catmull_rom_spline:
      /* find the segment we're on */
      nsegs=kc->npts-1; /* one segment less than points */
      if (nsegs<1) return KATE_E_INIT;
      n=t*nsegs;
      /* ensure it doesn't overflow */
      if (n<0) n=0;
      if (n>=nsegs) n=nsegs-1;
      /* get a 0-1 t on this segment */
      t0=n/(kate_float)nsegs;
      t1=(n+1)/(kate_float)nsegs;
      T=(t-t0)/(t1-t0);
      {
        int k0=n-1;
        int k1=n;
        int k2=n+1;
        int k3=n+2;
        if (n==0) k0=n;
        if (n==nsegs-1) k3=n+1;
        if (x) *x=kate_catmull_rom(T,kc->pts,k0,k1,k2,k3);
        if (y) *y=kate_catmull_rom(T,kc->pts+1,k0,k1,k2,k3);
      }
      return 0;

    case kate_curve_bezier_cubic_spline:
      /* find the segment we're on */
      if (kc->npts<4) return KATE_E_INIT;
      if ((kc->npts-1)%3) return KATE_E_INIT;
      nsegs=(kc->npts-1)/3; /* 4 control points per segment, with one shared */
      n=t*nsegs;
      /* ensure it doesn't overflow */
      if (n<0) n=0;
      if (n>=nsegs) n=nsegs-1;
      /* get a 0-1 t on this segment */
      t0=n/(kate_float)nsegs;
      t1=(n+1)/(kate_float)nsegs;
      T=(t-t0)/(t1-t0);
      if (x) *x=kate_bezier_cubic(T,kc->pts+2*n*3);
      if (y) *y=kate_bezier_cubic(T,kc->pts+2*n*3+1);
      return 0;

    case kate_curve_bspline:
      if (kc->npts<1) return KATE_E_INIT;
      /* find the segment we're on */
      nsegs=kc->npts+3; /* more segments than points, end knots are 3-repeated */
      if (nsegs<1) return KATE_E_INIT;
      n=t*nsegs;
      /* ensure it doesn't overflow */
      if (n<0) n=0;
      if (n>=nsegs) n=nsegs-1;
      /* get a 0-1 t on this segment */
      t0=n/(kate_float)nsegs;
      t1=(n+1)/(kate_float)nsegs;
      T=(t-t0)/(t1-t0);
      {
        int k0=n-3;
        int k1=k0+1;
        int k2=k0+2;
        int k3=k0+3;
#define clamp_knot(k) do { if (k<0) k=0; if (k>=(int)kc->npts) k=kc->npts-1; } while(0)
        clamp_knot(k0);
        clamp_knot(k1);
        clamp_knot(k2);
        clamp_knot(k3);
#undef clamp_knot
        if (x) *x=kate_bspline(T,kc->pts,k0,k1,k2,k3);
        if (y) *y=kate_bspline(T,kc->pts+1,k0,k1,k2,k3);
      }
      return 0;

    default:
      return KATE_E_INVALID_PARAMETER;
  }
}

/**
  Returns the point defined by the given motion at the given time.
  t will be between 0 and the duration of the motion
  \param km the motion to get the point from
  \param duration the duration the motion spans
  \param t the time at which the point should be taken (between 0 and motion duration)
  \param x a pointer to the first coordinate of the computed point (may be NULL)
  \param y a pointer to the second coordinate of the computed point (may be NULL)
  \returns 0 success
  \returns 1 no point at this time in the motion
  \returns KATE_E_* error
  */
int kate_motion_get_point(const kate_motion *km,kate_float duration,kate_float t,kate_float *x,kate_float *y)
{
  size_t n;
  kate_float motion_duration;

  if (!km) return KATE_E_INVALID_PARAMETER;
  if (duration<0) return KATE_E_INVALID_PARAMETER;
  if (t<0 || t>duration) return KATE_E_INVALID_PARAMETER;
  /* x and y may be NULL */

  motion_duration=0;
  do {
    for (n=0;n<km->ncurves;++n) {
      kate_float curve_duration=km->durations[n];
      if (curve_duration<0.0) curve_duration=-curve_duration*duration;
      if (t<=curve_duration) {
        /* TODO: div by zero possible here */
        return kate_curve_get_point(km->curves[n],t/curve_duration,x,y);
      }
      t-=curve_duration;
      motion_duration+=curve_duration;
    }
    /* if the motion is periodic, we loop forever */
    if (km->periodic) {
      /* we now know the full duration, so we can remap t into 0..duration so next loop
         will hit the point */
      int loops=t/motion_duration;
      t-=loops*motion_duration;
    }
  } while (km->periodic);

  /* t out of range */
  return KATE_E_INVALID_PARAMETER;
}

/**
  Destroys a list of motions.
  \param ki the kate_info structure containing the list of predefined motions
  \param motions the list of motions to destroy
  \param destroy a list of booleans specifying which motions in the list to destroy
  \param nmotions the number of motions in the list
  \param force if zero, motions found in the predefined motion list will not be destroyed
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_motion_destroy(const kate_info *ki,kate_motion **motions,const int *destroy,size_t nmotions,int force)
{
  size_t n,l;
  kate_motion *km;

  if (!ki || !motions) return KATE_E_INVALID_PARAMETER;

  for (n=0;n<nmotions;++n) {
    km=motions[n];
    if (!km) continue; /* may have holes if referred by index */
    if (destroy && !destroy[n]) continue;
    if (force || kate_find_motion(ki,km)<0) {
      if (km->curves) {
        for (l=0;l<km->ncurves;++l) {
          kate_curve *kc=km->curves[l];
          if (kate_find_curve(ki,kc)<0) {
            kate_free(kc->pts);
            kate_free(kc);
          }
        }
        kate_free(km->curves);
      }
      if (km->durations) kate_free(km->durations);
      if (km->meta) kate_meta_destroy(km->meta);
      kate_free(km);
    }
  }
  kate_free(motions);

  return 0;
}

