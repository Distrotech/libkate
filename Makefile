#
# Several variables may be set to change the behavior of this Makefile:
#  LIBQUAL: name of the lib directory to use for installed libraries  (default: "lib")
#  PREFIX: where to install libraries, headers, etc (default: "/usr/local")
#  DESTDIR: a directory where the prefix tree is to be installed (default: "")
#  C89: if set to 1, the code will build in C89 mode (default: "")
#  DEBUG: if set to 1, the code will build with debug information (default: "")
#  PROFILE: if set to 1, the code will build with profile information (default: "")
#  SPAMMY_WARNINGS: if set to 1, the code will build with more warnings flags (default: "")
#  CC: name of the command to compile
#  LD: name of the command to link
#  AR: name of the command to create an archive
#  RANLIB: name of the command to generate an archive index
#  STRIP: name of the command to strip a library
#  LIBTOOL: name of the libtool script
#  RM: name of the command to remove a file
#  RMDIR: name of the command to remove a directory
#

CC=gcc
LD=gcc
AR=ar
RANLIB=ranlib
STRIP=strip
LIBTOOL=libtool
RM=/bin/rm
RMDIR=/bin/rmdir

RM_F=$(RM) -f
RM_FR=$(RM) -fr

OBJDIR=obj
LIBDIR=lib
OGGDIR=built-streams
LIBQUAL=lib

SPAMMY_WARNINGS=0

CWARNFLAGS_LIGHT:=-W -Wall -Wdeclaration-after-statement -Wcast-qual -Wcast-align \
                  -Winit-self -Wcast-align -pedantic -Wformat=2 -Wunused \
                  -Wstrict-aliasing=2 -Wpointer-arith -Wbad-function-cast -Waggregate-return

CWARNFLAGS_FULL:=$(CWARNFLAGS_LIGHT)
CWARNFLAGS_FULL+=-Wshadow -Wsign-compare -Wredundant-decls -Wmissing-prototypes -Wundef -Wmissing-declarations

ifeq ($(SPAMMY_WARNINGS),1)
CWARNFLAGS_FULL+=-Wconversion
CWARNFLAGS_LIGHT:=$(CWARNFLAGS_FULL)
endif

LIBTOOL_OPTS=--silent --tag=CC

BUILT_CFLAGS+=-Iinclude -Isrc

ifeq ($(C89),1)
BUILT_CFLAGS+=-std=c89
CWARNFLAGS_LIGHT+=-Wwrite-strings
else
BUILT_CFLAGS+=-std=c99
endif

ifeq ($(DEBUG),1)
BUILT_CFLAGS+=-g -O0 -DDEBUG
STRIP=/bin/true
else
CWARNFLAGS_LIGHT+=-Winline -Wdisabled-optimization
BUILT_CFLAGS+=-O2
BUILT_LDFLAGS+=-Wl,-x -Wl,-S -Wl,-O2
STRIPFLAGS=-X -d -x -X --strip-unneeded
endif

ifeq ($(PROFILE),1)
BUILT_CFLAGS+=-pg -g
BUILT_LDFLAGS+=-pg
endif

ifeq ($(PREFIX),)
PREFIX=/usr/local
endif

BUILT_CFLAGS+=-MMD -MT "$(basename $@).o $(basename $@).d" -MF "$(basename $@).d"
BUILT_LDFLAGS+=-L$(LIBDIR)

CFLAGS+=$(BUILT_CFLAGS)
CFLAGS_STATIC=$(CFLAGS)
CFLAGS_SHARED=$(CFLAGS) -fPIC -DPIC
LDFLAGS+=$(BUILT_LDFLAGS)

VERSION=0.1.7
LIBVER_CURRENT=1
LIBVER_REVISION=0
LIBVER_AGE=0
SONAME_MAJOR=0
LIBTOOL_VERSION_INFO=$(LIBVER_CURRENT):$(LIBVER_REVISION):$(LIBVER_AGE)

all: lib #doc

MODULES=kate kate_info kate_comment kate_granule kate_event \
        kate_motion kate_text kate_tracker kate_fp kate_font \
        kate_encode_state kate_encode \
        kate_decode_state kate_decode \
        kate_packet kate_bitwise kate_rle \
        kate_high

OBJS_STATIC=$(foreach module, $(MODULES),$(OBJDIR)/static/$(module).o)
OBJS_SHARED=$(foreach module, $(MODULES),$(OBJDIR)/shared/$(module).o)
OBJS_LIBTOOL=$(foreach module, $(MODULES),$(OBJDIR)/libtool/$(module).lo)

LIBOGGKATE_OBJS_STATIC=$(OBJDIR)/static/kate_ogg.o
LIBOGGKATE_OBJS_SHARED=$(OBJDIR)/shared/kate_ogg.o
LIBOGGKATE_OBJS_LIBTOOL=$(OBJDIR)/libtool/kate_ogg.lo

DECODER_OBJS=$(OBJDIR)/decoder.o
ENCODER_OBJS=$(OBJDIR)/encoder.o $(OBJDIR)/katedesc.tab.o $(OBJDIR)/lex.katedesc.o $(OBJDIR)/kpng.o

LEX=$(shell which flex 2> /dev/null)
YACC=$(shell which bison 2> /dev/null)
DOXYGEN=$(shell which doxygen 2> /dev/null)

OGGERR:=$(shell echo -e "\#include <ogg/ogg.h>\nint main() { ogg_page og; return ogg_page_serialno(&og); }" | $(CC) -xc -o /dev/null - -logg -lm -lc 2>&1)

.PHONY: tools
ifneq ($(OGGERR),)
tools:
	@echo libogg not found, liboggkate and tools will not be built
else
ifeq ($(LEX),)
tools:
	@echo lex not found, tools will not be built
else
ifeq ($(YACC),)
tools:
	@echo yacc not found, tools will not be built
else
tools: tools/encoder tools/decoder
endif
endif
endif

.PHONY: doc
ifeq ($(DOXYGEN),)
doc:
	@echo doxygen not found, documentation will not be built
else
doc:
	@$(DOXYGEN) doc/kate.doxygen
endif

STATICLIBS:=$(LIBDIR)/libkate.a
SHAREDLIBS:=$(LIBDIR)/libkate.$(VERSION).so
LIBTOOLLIBS:=$(LIBDIR)/libkate.la

ifeq ($(OGGERR),)
STATICLIBS+=$(LIBDIR)/liboggkate.a
SHAREDLIBS+=$(LIBDIR)/liboggkate.$(VERSION).so
LIBTOOLLIBS+=$(LIBDIR)/liboggkate.la
endif

staticlib: $(STATICLIBS)
sharedlib: $(SHAREDLIBS)
libtoollib: $(LIBTOOLLIBS)

#lib: staticlib sharedlib libtoollib
lib: libtoollib


$(OBJDIR)/static/%.o: src/%.c
	@echo " CC $@"
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS_STATIC) $(CWARNFLAGS_FULL) -o $@ -c $<

$(OBJDIR)/shared/%.o: src/%.c
	@echo " CC $@"
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS_SHARED) $(CWARNFLAGS_FULL) -o $@ -c $<

$(OBJDIR)/libtool/%.lo: src/%.c
	@echo " CC $@"
	@mkdir -p $(dir $@)
	@$(LIBTOOL) $(LIBTOOL_OPTS) --mode=compile $(CC) $(CFLAGS) $(CWARNFLAGS_FULL) -o $@ -c $<

$(OBJDIR)/%.o: tools/%.c
	@echo " CC $@"
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS_STATIC) $(CWARNFLAGS_LIGHT) -o $@ -c $<

tools/lex.katedesc.c: tools/katedesc.l tools/katedesc.tab.c
	@echo " LEX $@"
	@mkdir -p $(dir $@)
	@$(LEX) -i -Pkatedesc_ -o$@ $<

tools/katedesc.tab.c: tools/katedesc.y
	@echo " YACC $@"
	@mkdir -p $(dir $@)
	@$(YACC) -v -b katedesc_ -p katedesc_ -d -o $@ $<

tools/decoder: $(DECODER_OBJS)
	@echo " CC $@"
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $(DECODER_OBJS) `PKG_CONFIG_PATH=misc/pkgconfig:${PKG_CONFIG_PATH} pkg-config --libs oggkate`

tools/encoder: $(ENCODER_OBJS)
	@echo " CC $@"
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $(ENCODER_OBJS) -lpng `PKG_CONFIG_PATH=misc/pkgconfig:${PKG_CONFIG_PATH} pkg-config --libs oggkate`

$(LIBDIR)/libkate.a: $(OBJS_STATIC)
	@echo " AR $@"
	@mkdir -p $(dir $@)
	@$(AR) cr $@ $^
	@$(RANLIB) $@
	@$(STRIP) $(STRIPFLAGS) $@

$(LIBDIR)/liboggkate.a: $(LIBOGGKATE_OBJS_STATIC)
	@echo " AR $@"
	@mkdir -p $(dir $@)
	@$(AR) cr $@ $^
	@$(RANLIB) $@
	@$(STRIP) $(STRIPFLAGS) $@

$(LIBDIR)/libkate.$(VERSION).so: $(OBJS_SHARED)
	@echo " LD $@"
	@mkdir -p $(dir $@)
	@$(LD) -shared -o $@ -Wl,-soname -Wl,libkate.so.$(SONAME_MAJOR) $^
	@$(STRIP) $(STRIPFLAGS) $@
	@/sbin/ldconfig -n $(LIBDIR)
	@ln -fs libkate.so.$(SONAME_MAJOR) lib/libkate.so

$(LIBDIR)/liboggkate.$(VERSION).so: $(LIBOGGKATE_OBJS_SHARED)
	@echo " LD $@"
	@mkdir -p $(dir $@)
	@$(LD) -shared -o $@ -Wl,-soname -Wl,liboggkate.so.$(SONAME_MAJOR) $^
	@$(STRIP) $(STRIPFLAGS) $@
	@/sbin/ldconfig -n $(LIBDIR)
	@ln -fs liboggkate.so.$(SONAME_MAJOR) lib/liboggkate.so

$(LIBDIR)/libkate.la: $(OBJS_LIBTOOL)
	@echo " AR $@"
	@mkdir -p $(dir $@)
	@$(LIBTOOL) $(LIBTOOL_OPTS) --mode=link $(LD) $(LDFLAGS) -rpath $(PREFIX)/lib -version-info $(LIBTOOL_VERSION_INFO) -o $@ $^

$(LIBDIR)/liboggkate.la: $(LIBOGGKATE_OBJS_LIBTOOL)
	@echo " AR $@"
	@mkdir -p $(dir $@)
	@$(LIBTOOL) $(LIBTOOL_OPTS) --mode=link $(LD) $(LDFLAGS) -rpath $(PREFIX)/lib -version-info $(LIBTOOL_VERSION_INFO) -o $@ $^

.PHONY: clean
clean:
	$(RM_F) $(OBJS_STATIC) $(OBJS_STATIC:.o=.d)
	$(RM_F) $(LIBOGGKATE_OBJS_STATIC) $(LIBOGGKATE_OBJS_STATIC:.o=.d)
	$(RM_F) $(OBJS_SHARED) $(OBJS_SHARED:.o=.d)
	$(RM_F) $(LIBOGGKATE_OBJS_SHARED) $(LIBOGGKATE_OBJS_SHARED:.o=.d)
	$(LIBTOOL) $(LIBTOOL_OPTS) --mode=clean $(RM_F) $(OBJS_LIBTOOL) $(OBJS_LIBTOOL:.o=.d)
	$(LIBTOOL) $(LIBTOOL_OPTS) --mode=clean $(RM_F) $(LIBOGGKATE_OBJS_LIBTOOL) $(OBJS_LIBTOOL:.o=.d)
	$(RM_F) $(LIBDIR)/libkate.a $(LIBDIR)/liboggkate.a
	$(RM_F) $(LIBDIR)/libkate.$(VERSION).so $(LIBDIR)/liboggkate.$(VERSION).so
	$(RM_F) $(LIBDIR)/libkate.so $(LIBDIR)/liboggkate.so
	$(RM_F) $(LIBDIR)/libkate.so.$(SONAME_MAJOR) $(LIBDIR)/liboggkate.so.$(SONAME_MAJOR)
	$(LIBTOOL) $(LIBTOOL_OPTS) --mode=clean $(RM_F) $(LIBDIR)/libkate.la $(LIBDIR)/liboggkate.la
	$(RM_F) tools/encoder
	$(RM_F) tools/decoder
	$(RM_F) $(OBJDIR)/*.[od]
	$(RM_F) tools/katedesc.tab.[ch]
	$(RM_F) tools/lex.katedesc.c

.PHONY: install
install: lib
	mkdir -p $(DESTDIR)$(PREFIX)/include/kate
	mkdir -p $(DESTDIR)$(PREFIX)/$(LIBQUAL)
	cp include/kate/kate.h $(DESTDIR)$(PREFIX)/include/kate/
	-cp $(LIBDIR)/libkate.a $(DESTDIR)$(PREFIX)/$(LIBQUAL)/
	-cp $(LIBDIR)/libkate.$(VERSION).so $(DESTDIR)$(PREFIX)/$(LIBQUAL)/
	-cp -P $(LIBDIR)/libkate.so $(DESTDIR)$(PREFIX)/$(LIBQUAL)/
	-cp -P $(LIBDIR)/libkate.so.$(SONAME_MAJOR) $(DESTDIR)$(PREFIX)/$(LIBQUAL)/
	-$(LIBTOOL) $(LIBTOOL_OPTS) --mode=install cp $(LIBDIR)/libkate.la $(DESTDIR)$(PREFIX)/$(LIBQUAL)/
	-$(LIBTOOL) $(LIBTOOL_OPTS) --mode=finish $(DESTDIR)$(PREFIX)/$(LIBQUAL)
	mkdir -p $(DESTDIR)$(PREFIX)/$(LIBQUAL)/pkgconfig
	cat misc/pkgconfig/kate.pc\
           | awk -v px="$(PREFIX)" '/^prefix=/ {print "prefix="px; next} {print}' \
           > $(DESTDIR)$(PREFIX)/$(LIBQUAL)/pkgconfig/kate.pc
ifeq ($(OGGERR),)
	cp include/kate/oggkate.h $(DESTDIR)$(PREFIX)/include/kate/
	-cp $(LIBDIR)/liboggkate.a $(DESTDIR)$(PREFIX)/$(LIBQUAL)/
	-cp $(LIBDIR)/liboggkate.$(VERSION).so $(DESTDIR)$(PREFIX)/$(LIBQUAL)/
	-cp -P $(LIBDIR)/liboggkate.so $(DESTDIR)$(PREFIX)/$(LIBQUAL)/
	-cp -P $(LIBDIR)/liboggkate.so.$(SONAME_MAJOR) $(DESTDIR)$(PREFIX)/$(LIBQUAL)/
	-$(LIBTOOL) $(LIBTOOL_OPTS) --mode=install cp $(LIBDIR)/liboggkate.la $(DESTDIR)$(PREFIX)/$(LIBQUAL)/
	-$(LIBTOOL) $(LIBTOOL_OPTS) --mode=finish $(DESTDIR)$(PREFIX)/$(LIBQUAL)
	cat misc/pkgconfig/oggkate.pc | \
           awk -v px="$(PREFIX)" '/^prefix=/ {print "prefix="px; next} {print}' \
           > $(DESTDIR)$(PREFIX)/$(LIBQUAL)/pkgconfig/oggkate.pc
endif
	-/sbin/ldconfig -n $(DESTDIR)$(PREFIX)/$(LIBQUAL)/

.PHONY: uninstall
uninstall:
	-$(RM_F) $(DESTDIR)$(PREFIX)/$(LIBQUAL)/libkate.a $(DESTDIR)$(PREFIX)/$(LIBQUAL)/liboggkate.a
	-$(RM_F) $(DESTDIR)$(PREFIX)/$(LIBQUAL)/libkate.$(VERSION).so $(DESTDIR)$(PREFIX)/$(LIBQUAL)/liboggkate.$(VERSION).so
	-$(RM_F) $(DESTDIR)$(PREFIX)/$(LIBQUAL)/libkate.so $(DESTDIR)$(PREFIX)/$(LIBQUAL)/liboggkate.so
	-$(RM_F) $(DESTDIR)$(PREFIX)/$(LIBQUAL)/libkate.so.$(SONAME_MAJOR) $(DESTDIR)$(PREFIX)/$(LIBQUAL)/liboggkate.so.$(SONAME_MAJOR)
	-$(LIBTOOL) $(LIBTOOL_OPTS) --mode=uninstall $(RM_F) $(DESTDIR)$(PREFIX)/$(LIBQUAL)/libkate.la $(DESTDIR)$(PREFIX)/$(LIBQUAL)/liboggkate.la
	-$(RM_F) $(DESTDIR)$(PREFIX)/include/kate/kate.h
	-$(RM_F) $(DESTDIR)$(PREFIX)/include/kate/oggkate.h
	-$(RMDIR) $(DESTDIR)$(PREFIX)/include/kate
	-$(RM_F) $(DESTDIR)$(PREFIX)/$(LIBQUAL)/pkgconfig/kate.pc $(DESTDIR)$(PREFIX)/$(LIBQUAL)/pkgconfig/oggkate.pc

distname:=libkate-$(VERSION)
.PHONY: dist
dist:
	mkdir -p $(distname)
	cp -R src include examples diffs $(distname)
	cp -R doc $(distname)
	#mkdir -p $(distname)/doc
	#for f in doc/*; do if [ "$$f" != "doc/html" -a "$$f" != "doc/latex" ]; then cp -R "$$f" $(distname)/doc; fi; done
	mkdir -p $(distname)/tools
	cp -R tools/{en,de}coder.c tools/kpng.[hc] $(distname)/tools
	cp -R tools/katedesc.[lyh] $(distname)/tools
	cp tools/katedesc.tab.[ch] tools/lex.katedesc.c $(distname)/tools
	cp -R tools/kpng.[ch] $(distname)/tools
	cp README INSTALL COPYING AUTHORS THANKS ChangeLog Makefile $(distname)
	cp -R misc $(distname)
	cp -R contrib $(distname)
	tar cvfz $(distname).tar.gz --owner=0 --group=0 --exclude=CVS --exclude=.cvsignore $(distname)
	rm -fr $(distname)

ogg_merger:=$(shell which oggz-merge 2> /dev/null | sed -e 's,oggz-merge$$,oggz-merge,')
ifeq ($(ogg_merger),)
ogg_merger:=$(shell which oggz-merge 2> /dev/null | sed -e 's,oggzmerge$$,oggzmerge,')
endif
ifeq ($(ogg_merger),)
ogg_merger:=$(shell which oggmerge 2> /dev/null | sed -e 's,oggmerge$$,oggmerge -q,')
endif
video_theora_ogg:=$(shell ls video.theora.ogg 2> /dev/null)
ifeq ($(video_theora_ogg),)
video_theora_ogg:=$(shell ls video.theora.ogv 2> /dev/null)
endif

ldsodir=LD_LIBRARY_PATH=$(LIBDIR):$(LD_LIBRARY_PATH)

.PRECIOUS: $(OGGDIR)/%.kate.ogg
$(OGGDIR)/%.kate.ogg: examples/kate/%.kate tools/encoder
	@echo " Building Kate stream from $<"
	@mkdir -p $(dir $@)
	@$(ldsodir) ./tools/encoder -s `echo $< | md5sum | cut -b1-8` -l x-foo -t kate -o $(OGGDIR)/$(notdir $<).ogg $<

$(OGGDIR)/%.ogg: $(OGGDIR)/%.kate.ogg $(video_theora_ogg) tools/encoder
ifneq ($(ogg_merger),)
	@echo " Merging video with Kate stream from $<"
	@$(ogg_merger) -o $@ $(OGGDIR)/$(notdir $<) $(video_theora_ogg)
else
	echo "Building $@ needs either oggmerge or oggz-merge"
endif

$(video_theora_ogg):
	@echo To be able to merge Kate streams to a sample video, you need to link a Theora file
	@echo to video.theora.ogg or video.theora.ogv in the main Kate directory.
	@echo A plain darkish background at 640x480 should be good
	@echo If no such file is found, Kate streams will be built as single lone streams.

STREAMS=bspline kate empty demo minimal karaoke unicode path bom markup font utf8test z style png periodic granule
ifneq ($(OGGERR),)
streams:
	@echo "libogg not found, make streams needs it"
else
ifneq ($(video_theora_ogg),)
streams: $(foreach stream, $(STREAMS), $(OGGDIR)/$(notdir $(basename $(stream))).ogg)
else
streams: $(foreach stream, $(STREAMS), $(OGGDIR)/$(notdir $(basename $(stream))).kate.ogg) \
         $(video_theora_ogg)
endif
endif

tmp_ogg1:="kate-check-1.kate.ogg"
tmp_ogg2:="kate-check-2.kate.ogg"
tmp_kate1:="kate-check-1.kate"
tmp_kate2:="kate-check-2.kate"
valgrind:=$(shell which valgrind 2> /dev/null | sed -e 's,valgrind$$,valgrind -q --leak-check=full --show-reachable=yes,')
oggzdiff:=$(shell which oggz-diff 2> /dev/null | sed -e 's,oggz-diff$$,oggz-diff -U 16,')
ifeq ($(oggzdiff),)
oggzdiff:=$(shell which oggzdiff 2> /dev/null | sed -e 's,oggzdiff$$,oggzdiff -U 16,')
endif
ifeq ($(oggzdiff),)
oggzdiff=cmp
endif

.PHONY: check
ifneq ($(OGGERR),)
check:
	@echo "libogg not found, make check needs it"
else
check: tools/decoder tools/encoder
	@echo " Checking Kate namespace"
	@! nm -a $(STATICLIBS) $(SHAREDLIBS) $(LIBTOOLLIBS) \
         | grep "^[0-9a-z]\{8\} [A-T] [^\.]" \
	 | grep -vE " (_DYNAMIC|_init|_fini|_edata|_end|__bss_start)$$" \
         | grep -v "^.\{11\}kate_"
	@echo " Checking memory allocation routines"
	@! grep -E '[^_](malloc|realloc|free|calloc|memalign)\(' \
           src/* include/kate/* \
           tools/katedesc.h tools/encoder.c tools/decoder.c tools/*.[ly] \
	 | grep -v "^include/kate/kate.h:#define kate_"
	@echo " Checking forgotten debug traces"
	@! grep -E '[^sn]printf' \
	  src/* include/kate/* tools/katedesc.h \
	| grep .
	@$(foreach stream, $(STREAMS), \
	  echo " Checking Kate stream $(stream)" && \
	  $(ldsodir) $(valgrind) ./tools/encoder -s 0 -t kate -o $(tmp_ogg1) examples/kate/$(stream).kate && \
	  $(ldsodir) $(valgrind) ./tools/decoder -o $(tmp_kate1) $(tmp_ogg1) && \
	  $(ldsodir) $(valgrind) ./tools/encoder -s 0 -t kate -o $(tmp_ogg2) $(tmp_kate1) && \
	  $(oggzdiff) $(tmp_ogg1) $(tmp_ogg2) && \
	  $(ldsodir) $(valgrind) ./tools/decoder -o $(tmp_kate2) $(tmp_ogg2) && \
	  cmp $(tmp_kate1) $(tmp_kate2) && \
	) rm -f $(tmp_ogg1) $(tmp_ogg2) $(tmp_kate1) $(tmp_kate2)
endif

tools/encoder tools/decoder: $(LIBDIR)/liboggkate.a $(LIBDIR)/libkate.a

ifneq '$(findstring clean,$(MAKECMDGOALS))' 'clean'
-include obj/*.d obj/*/*.d
endif

