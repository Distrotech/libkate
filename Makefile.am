OGGDIR=built-streams
kateincdir=$(includedir)/kate
katepcdir=$(libdir)/pkgconfig

lib_LTLIBRARIES=lib/libkate.la
kateinc_HEADERS=include/kate/kate.h
katepc_DATA=misc/pkgconfig/kate.pc

if HAVE_OGG
lib_LTLIBRARIES+=lib/liboggkate.la
kateinc_HEADERS+=include/kate/oggkate.h
katepc_DATA+=misc/pkgconfig/oggkate.pc
endif

LIBTOOLFLAGS=--silent

AM_CPPFLAGS=-I $(top_srcdir)/src -I $(top_srcdir)/include

lib_libkate_la_SOURCES=\
  src/kate.c src/kate_info.c src/kate_comment.c src/kate_granule.c src/kate_event.c \
  src/kate_motion.c src/kate_text.c src/kate_tracker.c src/kate_fp.c src/kate_font.c \
  src/kate_encode_state.c src/kate_encode.c \
  src/kate_decode_state.c src/kate_decode.c \
  src/kate_packet.c src/kate_bitwise.c src/kate_rle.c \
  src/kate_high.c

lib_liboggkate_la_SOURCES=\
  src/kate_ogg.c

noinst_HEADERS=\
  src/kate_internal.h src/kate_config.h \
  src/kate_encode_state.h src/kate_decode_state.h \
  src/kate_bitwise.h src/kate_fp.h src/kate_rle.h \
  tools/kpng.h

lib_libkate_la_CFLAGS=@CWARNFLAGS_FULL@
lib_libkate_la_LDFLAGS=-version-info @LIBKATE_SHARED_VERSION@
lib_liboggkate_la_CFLAGS=@CWARNFLAGS_FULL@
lib_liboggkate_la_LDFLAGS=-version-info @LIBOGGKATE_SHARED_VERSION@

lib_liboggkate_la_LIBADD=@OGG_LIBS@

if HAVE_OGG
tools_kateenc_SOURCES=tools/kateenc.c tools/lex.katedesc.c tools/katedesc.tab.c tools/kpng.c
tools_katedec_SOURCES=tools/katedec.c
tools_kateenc_LDADD=lib/liboggkate.la lib/libkate.la @OGG_LIBS@ @PNG_LIBS@ @LEXLIB@
tools_katedec_LDADD=lib/liboggkate.la lib/libkate.la @OGG_LIBS@
tools_kateenc_CFLAGS=@CWARNFLAGS_LIGHT@
tools_katedec_CFLAGS=@CWARNFLAGS_LIGHT@
bin_PROGRAMS=tools/katedec
noinst_HEADERS+=tools/katedesc.h
if HAVE_PNG
bin_PROGRAMS+=tools/kateenc
endif
endif

install-data-local: doc
	$(mkinstalldirs) $(DESTDIR)$(docdir)
	for dir in doc/html doc/latex; do \
	  if test -d $$dir; then \
	    b=`basename $$dir`; \
	    $(mkinstalldirs) $(DESTDIR)$(docdir)/$$b; \
	    for f in $$dir/*; do \
	      $(INSTALL_DATA) $$f $(DESTDIR)$(docdir)/$$b; \
            done \
	  fi \
	done

uninstall-local:
	rm -rf $(DESTDIR)$(docdir)

clean-local:
	if test -d doc/html; then rm -rf doc/html; fi
	if test -d doc/latex; then rm -rf doc/latex; fi

tools/lex.katedesc.c: tools/katedesc.l
	@$(LEX) -i -Pkatedesc_ -o$@ $<

tools/katedesc.tab.c: tools/katedesc.y
	@$(YACC) -v -b katedesc_ -p katedesc_ -d -o $@ $<

am__tar = ${AMTAR} chof - --owner=0 --group=0 --exclude=CVS --exclude=.cvsignore "$$tardir"

.PHONY: doc
if HAVE_DOXYGEN
doc: doc/kate.doxygen
	doxygen doc/kate.doxygen
else
doc:
	echo "doxygen not found, cannot build docs"
	/bin/false
endif

dist-hook: doc
	mkdir -p $(distdir)/doc
	cp -fR doc/* $(distdir)/doc
	rm -f $(distdir)/doc/kate.doxygen

STREAMS=bspline kate empty demo minimal karaoke unicode path bom markup font utf8test z style png periodic granule

tmp_ogg1:="kate-check-1.kate.ogg"
tmp_ogg2:="kate-check-2.kate.ogg"
tmp_kate1:="kate-check-1.kate"
tmp_kate2:="kate-check-2.kate"

check-local: tools/kateenc tools/katedec
	@echo " Checking Kate namespace"
	@! /usr/bin/nm -a lib/.libs/*.a lib/.libs/*.so \
         | $(GREP) "^[0-9a-z]\{8\} [A-T] [^\.]" \
	 | $(GREP) -vE " (_DYNAMIC|_init|_fini|_edata|_end|__bss_start)$$" \
         | $(GREP) -v "^.\{11\}kate_"
	@echo " Checking memory allocation routines"
	@! $(GREP) -E '[^_](malloc|realloc|free|calloc|memalign)\(' \
           src/* include/kate/* \
           tools/katedesc.h tools/kateenc.c tools/katedec.c tools/*.[ly] \
	 | $(GREP) -v "^include/kate/kate.h:#define kate_"
	@echo " Checking forgotten debug traces"
	@! $(GREP) -E '[^sn]printf' \
	  src/* include/kate/* tools/katedesc.h \
	| $(GREP) .
if HAVE_OGG
	@$(foreach stream, $(STREAMS), \
	  echo " Checking Kate stream $(stream)" && \
	  $(LIBTOOL) --mode=execute $(VALGRIND) ./tools/kateenc -s 0 -t kate -o $(tmp_ogg1) $(top_srcdir)/examples/kate/$(stream).kate && \
	  $(LIBTOOL) --mode=execute $(VALGRIND) ./tools/katedec -o $(tmp_kate1) $(tmp_ogg1) && \
	  $(LIBTOOL) --mode=execute $(VALGRIND) ./tools/kateenc -s 0 -t kate -o $(tmp_ogg2) $(tmp_kate1) && \
	  $(OGG_DIFF) $(tmp_ogg1) $(tmp_ogg2) && \
	  $(LIBTOOL) --mode=execute $(VALGRIND) ./tools/katedec -o $(tmp_kate2) $(tmp_ogg2) && \
	  cmp $(tmp_kate1) $(tmp_kate2) && \
	) rm -f $(tmp_ogg1) $(tmp_ogg2) $(tmp_kate1) $(tmp_kate2)
endif

if HAVE_OGG

video_theora_ogg:=$(shell ls video.theora.ogg 2> /dev/null || ls video.theora.ogv 2> /dev/null)

.PRECIOUS: $(OGGDIR)/%.kate.ogg
$(OGGDIR)/%.kate.ogg: examples/kate/%.kate tools/kateenc
	@echo " Building Kate stream from $<"
	@mkdir -p $(dir $@)
	@$(LIBTOOL) --mode=execute ./tools/kateenc -s `echo $< | md5sum | cut -b1-8` -l x-foo -t kate -o $(OGGDIR)/$(notdir $<).ogg $<

$(OGGDIR)/%.ogg: $(OGGDIR)/%.kate.ogg tools/kateenc
if HAVE_OGG_MERGE
	@if test "x${video_theora_ogg}" = "x"; then \
	  echo "video.theora.ogv not found, Kate streams will not be merged with video."; \
	else \
	  echo " Merging video with Kate stream from $<"; \
	  $(OGG_MERGE) -o $@ $(OGGDIR)/$(notdir $<) $(video_theora_ogg); \
	fi
else
	echo "Building $@ needs either oggmerge or oggz-merge"
endif

streams: $(foreach stream, $(STREAMS), $(OGGDIR)/$(notdir $(basename $(stream))).ogg)

else
streams:
	@echo "libogg not found, make streams needs it"

endif

EXTRA_DIST=\
  README INSTALL AUTHORS COPYING THANKS ChangeLog \
  tools/katedesc.l tools/katedesc.y \
  tools/lex.katedesc.c tools/katedesc.tab.c tools/katedesc.tab.h \
  misc/pkgconfig/kate.pc.in misc/pkgconfig/kate-uninstalled.pc.in \
  misc/pkgconfig/oggkate.pc.in misc/pkgconfig/oggkate-uninstalled.pc.in \
  examples \
  diffs \
  contrib

