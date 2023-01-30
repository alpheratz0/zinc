.POSIX:
.PHONY: all clean install uninstall dist

include config.mk

all: xpizarra

xpizarra.o: xpizarra.c
pizarra.o:  pizarra.c
utils.o:    utils.c

xpizarra: xpizarra.o pizarra.o utils.o
	$(CC) $(LDFLAGS) -o xpizarra xpizarra.o pizarra.o utils.o $(LDLIBS)

clean:
	rm -f xpizarra *.o xpizarra-$(VERSION).tar.gz

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f xpizarra $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/xpizarra
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	cp -f xpizarra.1 $(DESTDIR)$(MANPREFIX)/man1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/xpizarra.1

dist: clean
	mkdir -p xpizarra-$(VERSION)
	cp -R COPYING config.mk Makefile README xpizarra.1 xpizarra.c pizarra.c utils.c \
		pizarra.h utils.h xpizarra-$(VERSION)
	tar -cf xpizarra-$(VERSION).tar xpizarra-$(VERSION)
	gzip xpizarra-$(VERSION).tar
	rm -rf xpizarra-$(VERSION)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/xpizarra
	rm -f $(DESTDIR)$(MANPREFIX)/man1/xpizarra.1
