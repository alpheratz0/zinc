.POSIX:
.PHONY: all clean install uninstall dist

include config.mk

all: zinc

zinc.o:     zinc.c
pizarra.o:  pizarra.c
picker.o:   picker.c
utils.o:    utils.c
history.o:  history.c

zinc: zinc.o pizarra.o picker.o utils.o history.o
	$(CC) $(LDFLAGS) -o zinc zinc.o pizarra.o picker.o utils.o history.o $(LDLIBS)

clean:
	rm -f zinc *.o zinc-$(VERSION).tar.gz

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f zinc $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/zinc
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	cp -f zinc.1 $(DESTDIR)$(MANPREFIX)/man1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/zinc.1

dist: clean
	mkdir -p zinc-$(VERSION)
	cp -R COPYING config.mk Makefile README zinc.1 zinc.c pizarra.c picker.c \
		utils.c history.c pizarra.h picker.h utils.h history.h zinc-$(VERSION)
	tar -cf zinc-$(VERSION).tar zinc-$(VERSION)
	gzip zinc-$(VERSION).tar
	rm -rf zinc-$(VERSION)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/zinc
	rm -f $(DESTDIR)$(MANPREFIX)/man1/zinc.1
