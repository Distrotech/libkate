/* Copyright (C) 2008, 2009 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#define KATE_INTERNAL
#include "kate_internal.h"

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include "kate/kate.h"
#include "kate_meta.h"

static int kate_meta_init(kate_meta *km)
{
  if (!km) return KATE_E_INVALID_PARAMETER;

  km->nmeta=0;
  km->meta=NULL;

  return 0;
}

/**
  \ingroup meta
  Creates and initializes a kate_meta_list structure.
  \param km the structure to initialize
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_meta_create(kate_meta **km)
{
  kate_meta *tmp;
  int ret;

  if (!km) return KATE_E_INVALID_PARAMETER;

  tmp=(kate_meta*)kate_malloc(sizeof(kate_meta));
  if (!tmp) return KATE_E_OUT_OF_MEMORY;

  ret=kate_meta_init(tmp);
  if (ret<0) {
    kate_free(tmp);
    return ret;
  }

  *km=tmp;

  return 0;
}

static int kate_meta_clear(kate_meta *km)
{
  size_t n;

  if (!km) return KATE_E_INVALID_PARAMETER;

  for (n=0;n<km->nmeta;++n) {
    kate_meta_leaf *kml=km->meta+n;
    kate_free(kml->tag);
    kate_free(kml->value);
  }
  kate_free(km->meta);

  return 0;
}

/**
  \ingroup meta
  Destroys a kate_meta structure.
  \param km the structure to destroy
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_meta_destroy(kate_meta *km)
{
  if (!km) return KATE_E_INVALID_PARAMETER;
  kate_meta_clear(km);
  kate_free(km);
  return 0;
}

static int kate_meta_check_tag(const char *tag)
{
  if (!tag || !*tag) return KATE_E_INVALID_PARAMETER;

  while (*tag) {
    int c=*tag++;
    if (c<0x20 || c>0x7d || c=='=') return KATE_E_BAD_TAG;
  }
  return 0;
}

/**
  \ingroup meta
  Adds a tag/value metadata pair to the kate_meta structure.
  The tag must be 7 bit ASCII, and may not contain embedded NULs
  The value is binary data, and dependent on the tag.
  Text values should be UTF-8 and may contain embedded NULs
  \param km the kate_meta structure to add the metadata to
  \param tag the tag for the metadata add
  \param value the value for the metadata add (a stream of len bytes)
  \param len the number of bytes in the value
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_meta_add(kate_meta *km,const char *tag,const char *value,size_t len)
{
  kate_meta_leaf *meta;
  char *mtag,*mvalue;
  int ret;

  if (!km || !tag || !value) return KATE_E_INVALID_PARAMETER;

  ret=kate_check_add_overflow(km->nmeta,1,NULL);
  if (ret<0) return ret;
  ret=kate_check_add_overflow(len,1,NULL);
  if (ret<0) return ret;

  ret=kate_meta_check_tag(tag);
  if (ret<0) return ret;

  meta=(kate_meta_leaf*)kate_checked_realloc(km->meta,(km->nmeta+1),sizeof(kate_meta_leaf));
  if (!meta) return KATE_E_OUT_OF_MEMORY;
  km->meta=meta;

  mtag=kate_malloc(strlen(tag)+1);
  if (!mtag) return KATE_E_OUT_OF_MEMORY;
  strcpy(mtag,tag);
  mvalue=kate_malloc(len);
  if (!mvalue) {
    kate_free(mtag);
    return KATE_E_OUT_OF_MEMORY;
  }
  memcpy(mvalue,value,len);

  km->meta[km->nmeta].tag=mtag;
  km->meta[km->nmeta].value=mvalue;
  km->meta[km->nmeta].len=len;

  ++km->nmeta;

  return 0;
}

/**
  \ingroup meta
  Adds a tag/value metadata pair to the kate_meta structure.
  \param km the kate_meta structure to add the metadata to
  \param tag the tag for the metadata add
  \param value the value for the metadata add (a NUL terminated UTF-8 string)
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_meta_add_string(kate_meta *km,const char *tag,const char *value)
{
  size_t len;
  int ret;

  if (!value) return KATE_E_INVALID_PARAMETER;

  len=strlen(value)+1;
  ret=kate_text_validate(kate_utf8,value,len);
  if (ret<0) return ret;

  return kate_meta_add(km,tag,value,len);
}

/**
  \ingroup meta
  Retrieves the data for a given metadata.
  \param km the kate_meta structure to search in
  \param tag the tag to search for
  \param idx the index of the tag to search for (eg, if a tag is present more than once)
  \param value where to store the value of the tag
  \param len where to store the length (in bytes) of the value
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_meta_query_tag(const kate_meta *km,const char *tag,unsigned int idx,const char **value,size_t *len)
{
  size_t n;

  if (!km || !tag) return KATE_E_INVALID_PARAMETER;

  for (n=0;n<km->nmeta;++n) {
    if (!kate_ascii_strcasecmp(tag,km->meta[n].tag)) {
      if (idx==0) {
        if (value) *value=km->meta[n].value;
        if (len) *len=km->meta[n].len;
        return 0;
      }
      --idx;
    }
  }
  return KATE_E_INVALID_PARAMETER;
}

/**
  \ingroup meta
  Retrieves the data for a given metadata.
  \param km the kate_meta structure to search in
  \param idx the index of the metadata to get data for
  \param tag where to store the tag of the metadata
  \param value where to store the value of the tag
  \param len where to store the length (in bytes) of the value
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_meta_query(const kate_meta *km,unsigned int idx,const char **tag,const char **value,size_t *len)
{
  if (!km || idx>=km->nmeta) return KATE_E_INVALID_PARAMETER;

  if (tag) *tag=km->meta[idx].tag;
  if (value) *value=km->meta[idx].value;
  if (len) *len=km->meta[idx].len;
  return 0;
}

/**
  \ingroup meta
  Returns the number of metadata with the given tag
  \param km the kate_meta structure to search in
  \param tag the tag to search for
  \returns 0 success, no matching tags were found
  \returns >0 success, the value is the number of tags found
  \returns KATE_E_* error
  */
int kate_meta_query_tag_count(const kate_meta *km,const char *tag)
{
  size_t n;
  int count=0;
  int ret;

  if (!km || !tag) return KATE_E_INVALID_PARAMETER;

  ret=kate_meta_check_tag(tag);
  if (ret<0) return ret;

  for (n=0;n<km->nmeta;++n) {
    if (!kate_ascii_strcasecmp(tag,km->meta[n].tag)) ++count;
  }
  return count;
}

/**
  \ingroup meta
  Returns the number of metadata in this structure
  \param km the kate_meta structure to search in
  \returns 0 success, no metadata is present
  \returns >0 success, the value is the number of metadata found
  \returns KATE_E_* error
  */
int kate_meta_query_count(const kate_meta *km)
{
  if (!km) return KATE_E_INVALID_PARAMETER;

  return km->nmeta;
}

/**
  \ingroup meta
  Removes a given metadata pair.
  \param km the kate_meta structure to change
  \param tag the tag to search for, may be NULL to match any tag
  \param idx the index of the metadata
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_meta_remove_tag(kate_meta *km,const char *tag,unsigned int idx)
{
  size_t n;

  if (!km) return KATE_E_INVALID_PARAMETER;

  for (n=0;n<km->nmeta;++n) {
    if (!tag || !kate_ascii_strcasecmp(tag,km->meta[n].tag)) {
      if (idx==0) {
        kate_free(km->meta[n].tag);
        kate_free(km->meta[n].value);
        if (n+1!=km->nmeta) memmove(km->meta+n,km->meta+n+1,(km->nmeta-n-1)*sizeof(kate_meta_leaf));
        --km->nmeta;
        return 0;
      }
      --idx;
    }
  }
  return KATE_E_INVALID_PARAMETER;
}

/**
  \ingroup meta
  Removes a given metadata pair.
  \param km the kate_meta structure to change
  \param idx the index of the metadata
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_meta_remove(kate_meta *km,unsigned int idx)
{
  if (!km || idx>=km->nmeta) return KATE_E_INVALID_PARAMETER;

  kate_free(km->meta[idx].tag);
  kate_free(km->meta[idx].value);
  if (idx+1!=km->nmeta) memmove(km->meta+idx,km->meta+idx+1,(km->nmeta-idx-1)*sizeof(kate_meta_leaf));
  --km->nmeta;
  return 0;
}

/**
  \ingroup meta
  Merges two sets of metadata together
  \param km the kate_meta structure to contain the merged metadata.
  \param km2 the kate_meta structure to merge to km. It will be freed if this call is successful.
  \returns 0 success
  \returns KATE_E_* error
  */
int kate_meta_merge(kate_meta *km,kate_meta *km2)
{
  kate_meta_leaf *tmp;
  size_t count,n;
  int ret;

  if (!km || !km2) return KATE_E_INVALID_PARAMETER;
  if (km2->nmeta==0) return 0;

  ret=kate_check_add_overflow(km->nmeta,km2->nmeta,&count);
  if (ret<0) return ret;
  tmp=(kate_meta_leaf*)kate_checked_realloc(km->meta,count,sizeof(kate_meta_leaf));
  if (!tmp) return KATE_E_OUT_OF_MEMORY;
  
  for (n=0;n<km2->nmeta;++n) {
    tmp[km->nmeta+n]=km2->meta[n];
  }
  kate_free(km2->meta);

  km->meta=tmp;
  km->nmeta+=km2->nmeta;

  kate_free(km2);

  return 0;
}

int kate_meta_create_copy(kate_meta **km,const kate_meta *km2)
{
  kate_meta *tmp;
  kate_meta_leaf *kml;
  size_t n;
  int ret;

  ret=kate_meta_create(&tmp);
  if (ret<0) return ret;
  kml=km2->meta;
  for (n=0;n<km2->nmeta;++n,++kml) {
    ret=kate_meta_add(tmp,kml->tag,kml->value,kml->len);
    if (ret<0) {
      kate_meta_destroy(tmp);
      return ret;
    }
  }
  *km=tmp;
  return 0;
}

