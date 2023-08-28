.POSIX:
.PHONY: all clean install uninstall dist

include config.mk

OBJ=\
	src/zinc.o \
	src/pizarra.o \
	src/picker.o \
	src/utils.o \
	src/history.o

all: zinc

zinc: $(OBJ)
	$(CC) $(LDFLAGS) -o zinc $(OBJ) $(LDLIBS)

clean:
	rm -f zinc $(OBJ) zinc-$(VERSION).tar.gz

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f zinc $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/zinc
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	cp -f zinc.1 $(DESTDIR)$(MANPREFIX)/man1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/zinc.1

dist: clean
	mkdir -p zinc-$(VERSION)
	cp -R COPYING config.mk Makefile README zinc.1 src include \
		zinc-$(VERSION)
	tar -cf zinc-$(VERSION).tar zinc-$(VERSION)
	gzip zinc-$(VERSION).tar
	rm -rf zinc-$(VERSION)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/zinc
	rm -f $(DESTDIR)$(MANPREFIX)/man1/zinc.1
