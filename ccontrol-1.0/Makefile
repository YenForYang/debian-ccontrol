CC=icc
CFLAGS=-O3 -Wall -g -O2 -fdebug-prefix-map=/tmp/ccontrol/ccontrol-1.0=. -Wformat -march=native -pipe -fstack-security-check -Wformat-security -Wdate-time -D_FORTIFY_SOURCE=2 -Wl,-z,relro -Wl,--as-needed,--gc-sections,--relax -fdiagnostics-color=always
C_FILES:=ccontrol.c ccontrol-parse.c ccontrol-lock.c ccontrol-identify.c stdrusty.c
GPERF_C_FILES:=extensions.c keywords.c
# Everything depends on makefile, headers, gperf files
ALLFILES=$(C_FILES) $(GPERF_C_FILES) Makefile stdrusty.h ccontrol.h
BINDIR=/usr/bin
LIBDIR=/usr/lib/ccontrol
DATADIR=/usr/share/ccontrol
MANDIR=/usr/share/man
XMLTO=/usr/bin/xmlto
ASCIIDOC=/usr/bin/asciidoc
VALGRIND=
VERSION=1.0

LINKS=$(foreach l,gcc cc g++ c++ ld make,$(prefix)$(LIBDIR)/$l)

ccontrol: $(ALLFILES) ccontrol.1
	$(CC) $(CFLAGS) -DVERSION='"$(VERSION) (David Gibson)"' -o $@ $(C_FILES)

Makefile: Makefile.in configure config.status
	./configure --reconfigure

config.status:
	@echo Please rerun ./configure.; exit 1

ccontrol_test: $(ALLFILES)
	$(CC) $(CFLAGS) -DVERSION='"$(VERSION) (David Gibson)"' -DTESTING -o $@ $(C_FILES)

$(GPERF_C_FILES): %.c: %.gperf
	gperf $< > $@

install: installbin installlib installman installgui

$(prefix)$(BINDIR) $(prefix)$(MANDIR)/man1 $(prefix)$(LIBDIR) $(prefix)$(DATADIR):
	mkdir -p $@

installgui: $(prefix)$(DATADIR)/ccontrol-key.png $(prefix)$(BINDIR)/gccontrol.py

$(prefix)$(DATADIR)/ccontrol-key.png: gui/ccontrol-key.png $(prefix)$(DATADIR) 
	cp $< $@

$(prefix)$(BINDIR)/gccontrol.py: gui/gccontrol.py $(prefix)$(BINDIR) 
	cp $< $@

installman: $(prefix)$(MANDIR)/man1 $(prefix)$(MANDIR)/man1/ccontrol.1 $(prefix)$(MANDIR)/man1/ccontrol-init.1

installbin: $(prefix)$(BINDIR)/ccontrol $(prefix)$(BINDIR)/ccontrol-init

$(prefix)$(BINDIR)/ccontrol: ccontrol $(prefix)$(BINDIR)
	cp $< $@.tmp && mv $@.tmp $@

$(prefix)$(BINDIR)/ccontrol-init: ccontrol-init
	cp $< $@

installlib: $(LINKS)

$(LINKS): %: ccontrol $(prefix)$(LIBDIR)
	ln -sf $(BINDIR)/ccontrol $@

$(prefix)$(MANDIR)/man1/ccontrol.1: ccontrol.1
	cp $< $@

$(prefix)$(MANDIR)/man1/ccontrol-init.1: ccontrol-init.1
	cp $< $@

uninstall:
	rm -f $(prefix)$(BINDIR)/ccontrol $(prefix)$(BINDIR)/ccontrol-init $(LINKS) $(prefix)$(MANDIR)/man1/ccontrol.1 $(prefix)$(MANDIR)/man1/ccontrol-init.1

check: ccontrol_test
	@unset MAKEFLAGS; testsuite/test.sh --valgrind=$(VALGRIND)

dist: clean
	rm -f Makefile config.status
	DIR=`basename \`pwd\``; cd .. && ln -sf $$DIR ccontrol-$(VERSION) && tar -c -v -j --exclude='*~' --exclude='*.orig' --exclude='*.rej' -f ccontrol-$(VERSION).tar.bz2 ccontrol-$(VERSION)/* ccontrol-$(VERSION)/.??* && rm -f ccontrol-$(VERSION)

clean:
	rm -f *.o ccontrol ccontrol_test

%: %.xml
	$(XMLTO) man $*.xml

%.xml: %.txt
	$(ASCIIDOC) -d manpage -b docbook $*.txt

distclean: clean
	rm -f Makefile config.status ccontrol.1 ccontrol-init.1
