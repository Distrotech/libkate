LIBTOOLFLAGS=--silent

AM_CPPFLAGS=-I $(top_srcdir)/src -I $(top_srcdir)/include

LIBKATE=../lib/libkate.la

TESTS=check_sizes language

noinst_PROGRAMS=$(TESTS)

check_sizes_SOURCES=check_sizes.c
check_sizes_LDADD=$(LIBKATE)

language_SOURCES=language.c
language_LDADD=$(LIBKATE)

