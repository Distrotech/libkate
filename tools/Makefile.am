LIBTOOLFLAGS=--silent

AM_CPPFLAGS=-I $(top_srcdir)/src -I $(top_srcdir)/include

noinst_HEADERS=\
  kpng.h

if HAVE_OGG
kateenc_SOURCES=kateenc.c kate_lexer.l kate_parser.y
if HAVE_PNG
kateenc_SOURCES+=kpng.c
endif
katedec_SOURCES=katedec.c
kateenc_LDADD=../lib/liboggkate.la ../lib/libkate.la @OGG_LIBS@ @PNG_LIBS@ @LEXLIB@ -lc
katedec_LDADD=../lib/liboggkate.la ../lib/libkate.la @OGG_LIBS@ -lc
kateenc_CFLAGS=@CWARNFLAGS_LIGHT@ @CFLAGS_FORTIFY_SOURCE@ @OGG_CFLAGS@ @PNG_CFLAGS@
katedec_CFLAGS=@CWARNFLAGS_LIGHT@ @CFLAGS_FORTIFY_SOURCE@ @OGG_CFLAGS@
bin_PROGRAMS=katedec kateenc
BUILT_SOURCES=kate_parser.h
endif

AM_LFLAGS=-i -Pkatedesc_ -o$(LEX_OUTPUT_ROOT).c
AM_YFLAGS=-v -p katedesc_ -d

EXTRA_DIST=\
  katedesc.h

