# A simple makefile for compiling and installing the tools 
# (as they do not use fancy libraries, autotools would be overkill for now).

CFLAGS ?= -Os -Wall -g

all:
	CFLAGS="$(CFLAGS)" $(MAKE) -C src

clean:
	$(MAKE) -C src clean

distclean:
	$(MAKE) -C src distclean

install:
	mkdir -p $(DESTDIR)/usr/bin
	install src/cpuload $(DESTDIR)/usr/bin/
	install src/memload $(DESTDIR)/usr/bin/
	install src/swpload $(DESTDIR)/usr/bin/
	install scripts/flash_eater $(DESTDIR)/usr/bin/
	install scripts/ioload $(DESTDIR)/usr/bin
	install scripts/run_secs $(DESTDIR)/usr/bin/
	install -d $(DESTDIR)/usr/share/man/man1
	cp -a doc/man/*.1 $(DESTDIR)/usr/share/man/man1
