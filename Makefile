.POSIX:
.PHONY: all clean install uninstall dist

include config.mk

all: xyne

xyne.o: xyne.c
pizarra.o:  pizarra.c
utils.o:    utils.c

xyne: xyne.o pizarra.o utils.o
	$(CC) $(LDFLAGS) -o xyne xyne.o pizarra.o utils.o $(LDLIBS)

clean:
	rm -f xyne *.o xyne-$(VERSION).tar.gz

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f xyne $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/xyne
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	cp -f xyne.1 $(DESTDIR)$(MANPREFIX)/man1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/xyne.1

dist: clean
	mkdir -p xyne-$(VERSION)
	cp -R COPYING config.mk Makefile README xyne.1 xyne.c pizarra.c utils.c \
		pizarra.h utils.h xyne-$(VERSION)
	tar -cf xyne-$(VERSION).tar xyne-$(VERSION)
	gzip xyne-$(VERSION).tar
	rm -rf xyne-$(VERSION)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/xyne
	rm -f $(DESTDIR)$(MANPREFIX)/man1/xyne.1
