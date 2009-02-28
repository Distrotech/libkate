LIBTOOLFLAGS=--silent

AM_CPPFLAGS=-I $(top_srcdir)/src -I $(top_srcdir)/include

LIBKATE=../lib/libkate.la

TESTS_ENVIRONMENT=$(LIBTOOL) -dlopen $(LIBKATE) --mode=execute $(VALGRIND)

TESTS=check_sizes language text overflow

noinst_PROGRAMS=$(TESTS)

check_sizes_SOURCES=check_sizes.c
check_sizes_LDADD=$(LIBKATE)

language_SOURCES=language.c
language_LDADD=$(LIBKATE)

text_SOURCES=text.c
text_LDADD=$(LIBKATE)

overflow_SOURCES=overflow.c
overflow_LDADD=$(LIBKATE)

STREAMS=@STREAMS@

tmp_ogg1:="$(top_builddir)/kate-check-1.kate.ogg"
tmp_ogg2:="$(top_builddir)/kate-check-2.kate.ogg"
tmp_kate1:="$(top_builddir)/kate-check-1.kate"
tmp_kate2:="$(top_builddir)/kate-check-2.kate"

KATEENC_OPTS=-s 0 -c test -l en -K 5 -R 15

check-local:
	@echo " Checking Kate namespace"
	@! @NM@ lib/.libs/*.{a,so} \
	 | $(GREP) "^[0-9a-z]\{8\} [A-MO-Z] [^\.]" \
	 | $(GREP) -vE " (_DYNAMIC|_init|_fini|_edata|_end|__bss_start)$$" \
	 | $(GREP) -vE " _(rest|save)[fg]pr_[0-9]*(|_x)$$" \
	 | $(GREP) -v "^.\{11\}kate_"
	@echo " Checking memory allocation routines"
	@! $(GREP) -EHn '[^_](malloc|realloc|free|calloc|memalign)\(' \
           $(top_srcdir)/src/*.[ch] $(top_srcdir)/include/kate/*.h \
           $(top_srcdir)/tools/katedesc.h \
	   $(top_srcdir)/tools/kateenc.c $(top_srcdir)/tools/katedec.c \
	   $(top_srcdir)/tools/*.[ly] \
	 | $(GREP) -v "^include/kate/kate.h:#define kate_"
	@echo " Checking forgotten debug traces"
	@! $(GREP) -EHn '[^sn]printf' \
	  $(top_srcdir)/src/*.[ch] $(top_srcdir)/include/kate/*.h \
	  $(top_srcdir)/tools/katedesc.h \
	| $(GREP) .
if HAVE_OGG
	@$(foreach stream, $(STREAMS), \
	  echo " Checking Kate stream $(stream)" && \
	  $(TESTS_ENVIRONMENT) $(top_builddir)/tools/kateenc $(KATEENC_OPTS) -t kate -o $(tmp_ogg1) $(top_srcdir)/examples/kate/$(stream).kate && \
	  $(TESTS_ENVIRONMENT) $(top_builddir)/tools/katedec -o $(tmp_kate1) $(tmp_ogg1) && \
	  $(TESTS_ENVIRONMENT) $(top_builddir)/tools/kateenc $(KATEENC_OPTS) -t kate -o $(tmp_ogg2) $(tmp_kate1) && \
	  $(OGG_DIFF) $(tmp_ogg1) $(tmp_ogg2) && \
	  $(TESTS_ENVIRONMENT) $(top_builddir)/tools/katedec -o $(tmp_kate2) $(tmp_ogg2) && \
	  cmp $(tmp_kate1) $(tmp_kate2) && \
	) rm -f $(tmp_ogg1) $(tmp_ogg2) $(tmp_kate1) $(tmp_kate2)
endif

