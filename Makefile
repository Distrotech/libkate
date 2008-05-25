CC=gcc
LD=gcc
AR=ar
RANLIB=ranlib
STRIP=strip

OBJDIR=obj
LIBDIR=lib
OGGDIR=built-streams

SPAMMY_WARNINGS=0

CWARNFLAGS_LIGHT:=-W -Wall -Wwrite-strings -Wdeclaration-after-statement -Wcast-qual -Wcast-align \
                  -Winit-self -Wcast-align -pedantic -Wformat=2 -Wunused \
                  -Wstrict-aliasing=2 -Wpointer-arith -Wbad-function-cast -Waggregate-return \
                  -Winline -Wdisabled-optimization

CWARNFLAGS_FULL:=$(CWARNFLAGS_LIGHT)
CWARNFLAGS_FULL+=-Wshadow -Wsign-compare -Wredundant-decls -Wmissing-prototypes -Wundef -Wmissing-declarations

ifeq ($(SPAMMY_WARNINGS),1)
CWARNFLAGS_FULL+=-Wconversion
CWARNFLAGS_LIGHT:=$(CWARNFLAGS_FULL)
endif

CFLAGS=-std=c99 -Iinclude -Isrc

ifeq ($(DEBUG),1)
CFLAGS+=-g -O0 -DDEBUG
STRIP=/bin/true
else
CFLAGS+=-O2
LDFLAGS+=-Wl,-x -Wl,-S -Wl,-O2
STRIPFLAGS=-X #-d -x -X --strip-unneeded
endif

ifeq ($(PROFILE),1)
CFLAGS+=-pg -g
LDFLAGS+=-pg
endif

ifeq ($(PREFIX),)
PREFIX=/usr/local
endif

CFLAGS_STATIC=$(CFLAGS)
CFLAGS_SHARED=$(CFLAGS) -fPIC
CFLAGS+=-MMD -MT "$(basename $@).o $(basename $@).d" -MF "$(basename $@).d"
LDFLAGS+=-L$(LIBDIR)

VERSION=0.1.3
SONAME_MAJOR=0

MODULES=kate kate_info kate_comment kate_granule kate_event \
        kate_motion kate_text kate_tracker kate_fp kate_font \
        kate_encode_state kate_encode \
        kate_decode_state kate_decode \
        kate_packet kate_bitwise kate_rle \
        kate_high

OBJS_STATIC=$(foreach module, $(MODULES),$(OBJDIR)/static/$(module).o)
OBJS_SHARED=$(foreach module, $(MODULES),$(OBJDIR)/shared/$(module).o)

LIBOGGKATE_OBJS_STATIC=$(OBJDIR)/static/kate_ogg.o
LIBOGGKATE_OBJS_SHARED=$(OBJDIR)/shared/kate_ogg.o

STATICLIBS=$(LIBDIR)/liboggkate.a $(LIBDIR)/libkate.a
SHAREDLIBS=$(LIBDIR)/liboggkate.$(VERSION).so $(LIBDIR)/libkate.$(VERSION).so

all: staticlib sharedlib tools #doc
staticlib: $(STATICLIBS)
sharedlib: $(SHAREDLIBS)

LEX=$(shell which flex 2> /dev/null)
YACC=$(shell which bison 2> /dev/null)
DOXYGEN=$(shell which doxygen 2> /dev/null)

.PHONY: tools
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

.PHONY: doc
ifeq ($(DOXYGEN),)
doc:
	@echo doxygen not found, documentation will not be built
else
doc:
	@$(DOXYGEN) doc/kate.doxygen
endif

$(OBJDIR)/static/%.o: src/%.c
	@echo " CC $@"
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS_STATIC) $(CWARNFLAGS_FULL) -o $@ -c $<

$(OBJDIR)/shared/%.o: src/%.c
	@echo " CC $@"
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS_SHARED) $(CWARNFLAGS_FULL) -o $@ -c $<

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

tools/decoder: $(OBJDIR)/decoder.o
	@echo " CC $@"
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $< `PKG_CONFIG_PATH=misc/pkgconfig:${PKG_CONFIG_PATH} pkg-config --libs oggkate`

tools/encoder: $(OBJDIR)/encoder.o $(OBJDIR)/katedesc.tab.o $(OBJDIR)/lex.katedesc.o $(OBJDIR)/kpng.o
	@echo " CC $@"
	@mkdir -p $(dir $@)
	@$(CC) $(LDFLAGS) -o $@ $^ -lpng `PKG_CONFIG_PATH=misc/pkgconfig:${PKG_CONFIG_PATH} pkg-config --libs oggkate`

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

.PHONY: clean
clean:
	rm -f $(OBJS_STATIC) $(OBJS_STATIC:.o=.d)
	rm -f $(LIBOGGKATE_OBJS_STATIC) $(OBJS_STATIC:.o=.d)
	rm -f $(OBJS_SHARED) $(OBJS_SHARED:.o=.d)
	rm -f $(LIBOGGKATE_OBJS_SHARED) $(OBJS_SHARED:.o=.d)
	rm -f $(LIBDIR)/libkate.a $(LIBDIR)/liboggkate.a
	rm -f $(LIBDIR)/libkate.$(VERSION).so $(LIBDIR)/liboggkate.$(VERSION).so
	rm -f $(LIBDIR)/libkate.so $(LIBDIR)/liboggkate.so
	rm -f $(LIBDIR)/libkate.so.$(SONAME_MAJOR) $(LIBDIR)/liboggkate.so.$(SONAME_MAJOR)
	rm -f tools/encoder
	rm -f tools/decoder
	rm -f $(OBJDIR)/*.[od]
	rm -f tools/katedesc.tab.[ch]
	rm -f tools/lex.katedesc.c

.PHONY: install
install: staticlib sharedlib
	mkdir -p $(PREFIX)/include/kate
	cp include/kate/kate.h include/kate/oggkate.h $(PREFIX)/include/kate/
	mkdir -p $(PREFIX)/lib
	cp $(LIBDIR)/libkate.a $(LIBDIR)/liboggkate.a $(PREFIX)/lib/
	cp $(LIBDIR)/libkate.$(VERSION).so $(LIBDIR)/liboggkate.$(VERSION).so $(PREFIX)/lib/
	cp -P $(LIBDIR)/libkate.so $(LIBDIR)/liboggkate.so $(PREFIX)/lib/
	-cp -P $(LIBDIR)/libkate.so.$(SONAME_MAJOR) $(LIBDIR)/liboggkate.so.$(SONAME_MAJOR) $(PREFIX)/lib/
	-/sbin/ldconfig -n $(PREFIX)/lib/
	mkdir -p $(PREFIX)/lib/pkgconfig
	cat misc/pkgconfig/kate.pc\
           | awk -v px="$(PREFIX)" '/^prefix=/ {print "prefix="px; next} {print}' \
           > $(PREFIX)/lib/pkgconfig/kate.pc
	cat misc/pkgconfig/oggkate.pc | \
           awk -v px="$(PREFIX)" '/^prefix=/ {print "prefix="px; next} {print}' \
           > $(PREFIX)/lib/pkgconfig/oggkate.pc

.PHONY: uninstall
uninstall:
	-rm -f $(PREFIX)/lib/libkate.a $(PREFIX)/lib/liboggkate.a
	-rm -f $(PREFIX)/lib/libkate.$(VERSION).so $(PREFIX)/lib/liboggkate.$(VERSION).so
	-rm -f $(PREFIX)/lib/libkate.so $(PREFIX)/lib/liboggkate.so
	-rm -f $(PREFIX)/lib/libkate.so.$(SONAME_MAJOR) $(PREFIX)/lib/liboggkate.so.$(SONAME_MAJOR)
	-rm -f $(PREFIX)/include/kate/kate.h
	-rm -f $(PREFIX)/include/kate/oggkate.h
	-rmdir $(PREFIX)/include/kate
	-rm -f $(PREFIX)/lib/pkgconfig/kate.pc $(PREFIX)/lib/pkgconfig/oggkate.pc

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
	cp README INSTALL COPYING AUTHORS ChangeLog Makefile $(distname)
	cp -R misc $(distname)
	tar cvfz $(distname).tar.gz --owner=0 --group=0 --exclude=CVS --exclude=.cvsignore $(distname)
	rm -fr $(distname)

ogg_merger:=$(shell which oggzmerge 2> /dev/null | sed -e 's,oggzmerge$$,oggzmerge,')
ifeq ($(ogg_merger),)
ogg_merger:=$(shell which oggmerge 2> /dev/null | sed -e 's,oggmerge$$,oggmerge -q,')
endif
video_theora_ogg:=$(shell ls video.theora.ogg 2> /dev/null)

.PRECIOUS: $(OGGDIR)/%.kate.ogg
$(OGGDIR)/%.kate.ogg: examples/kate/%.kate tools/encoder
	@echo " Building Kate stream from $<"
	@mkdir -p $(dir $@)
	@./tools/encoder -s `echo $< | md5sum | cut -b1-8` -l x-foo -t kate -o $(OGGDIR)/$(notdir $<).ogg $<

$(OGGDIR)/%.ogg: $(OGGDIR)/%.kate.ogg video.theora.ogg tools/encoder
ifneq ($(ogg_merger),)
	@echo " Merging video with Kate stream from $<"
	@$(ogg_merger) -o $@ $(OGGDIR)/$(notdir $<) video.theora.ogg
else
	echo "Building $@ needs either oggmerge or oggzmerge"
endif

video.theora.ogg:
	@echo To be able to merge Kate streams to a sample video, you need to link a Theora file
	@echo to video.theora.ogg in the main Kate directory.
	@echo A plain darkish background at 640x480 should be good
	@echo If no such file is found, Kate streams will be built as single lone streams.

STREAMS=bspline kate empty demo minimal karaoke unicode path bom markup font utf8test z style png periodic granule
ifneq ($(video_theora_ogg),)
streams: $(foreach stream, $(STREAMS), $(OGGDIR)/$(notdir $(basename $(stream))).ogg)
else
streams: $(foreach stream, $(STREAMS), $(OGGDIR)/$(notdir $(basename $(stream))).kate.ogg) \
         video.theora.ogg
endif

tmp_ogg1:="kate-check-1.kate.ogg"
tmp_ogg2:="kate-check-2.kate.ogg"
tmp_kate1:="kate-check-1.kate"
tmp_kate2:="kate-check-2.kate"
valgrind:=$(shell which valgrind 2> /dev/null | sed -e 's,valgrind$$,valgrind -q --leak-check=full --show-reachable=yes,')
oggzdiff:=$(shell which oggzdiff 2> /dev/null | sed -e 's,oggzdiff$$,oggzdiff -U 16,')
ifeq ($(oggzdiff),)
oggzdiff=cmp
endif

.PHONY: check
check: tools/decoder tools/encoder
	@echo " Checking Kate namespace"
	@! nm -a $(STATICLIBS) $(SHAREDLIBS) \
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
	  $(valgrind) ./tools/encoder -s 0 -t kate -o $(tmp_ogg1) examples/kate/$(stream).kate && \
	  $(valgrind) ./tools/decoder -o $(tmp_kate1) $(tmp_ogg1) && \
	  $(valgrind) ./tools/encoder -s 0 -t kate -o $(tmp_ogg2) $(tmp_kate1) && \
	  $(oggzdiff) $(tmp_ogg1) $(tmp_ogg2) && \
	  $(valgrind) ./tools/decoder -o $(tmp_kate2) $(tmp_ogg2) && \
	  cmp $(tmp_kate1) $(tmp_kate2) && \
	) rm -f $(tmp_ogg1) $(tmp_ogg2) $(tmp_kate1) $(tmp_kate2)

tools/encoder tools/decoder: $(LIBDIR)/liboggkate.a $(LIBDIR)/libkate.a

ifneq '$(findstring clean,$(MAKECMDGOALS))' 'clean'
-include obj/*.d obj/*/*.d
endif

