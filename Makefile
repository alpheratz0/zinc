.POSIX:
.PHONY: all clean install uninstall dist

include config.mk

all: sinca

sinca.o:    sinca.c
pizarra.o:  pizarra.c
utils.o:    utils.c

sinca: sinca.o pizarra.o utils.o
	$(CC) $(LDFLAGS) -o sinca sinca.o pizarra.o utils.o $(LDLIBS)

clean:
	rm -f sinca *.o sinca-$(VERSION).tar.gz

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f sinca $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/sinca
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	cp -f sinca.1 $(DESTDIR)$(MANPREFIX)/man1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/sinca.1

dist: clean
	mkdir -p sinca-$(VERSION)
	cp -R COPYING config.mk Makefile README sinca.1 sinca.c pizarra.c utils.c \
		pizarra.h utils.h sinca-$(VERSION)
	tar -cf sinca-$(VERSION).tar sinca-$(VERSION)
	gzip sinca-$(VERSION).tar
	rm -rf sinca-$(VERSION)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/sinca
	rm -f $(DESTDIR)$(MANPREFIX)/man1/sinca.1
