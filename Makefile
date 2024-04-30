.POSIX:
.SUFFIXES:

CC = cc
VERSION = 1.0
SERVER = zsm
CLIENT = zsmc 
MANPAGE = $(TARGET).1
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man/man1

# Flags
LDFLAGS = $(shell pkg-config --libs libsodium libnotify ncurses)
CFLAGS = -O3 -mtune=native -march=native -pipe -g -std=c99 -Wno-pointer-sign -Wpedantic -Wall -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600 $(shell pkg-config --cflags libsodium libnotify ncurses) -lpthread

SERVERSRC = src/server/*.c
CLIENTSRC = src/client/*.c
LIBSRC = lib/*.c
INCLUDE = -Iinclude/

$(SERVER): $(SERVERSRC) $(LIBSRC)
	$(CC) $(SERVERSRC) $(LIBSRC) $(INCLUDE) -o $@ $(CFLAGS) $(LDFLAGS)

$(CLIENT): $(CLIENTSRC) $(LIBSRC)
	$(CC) $(CLIENTSRC) $(LIBSRC) $(INCLUDE) -o $@ $(CFLAGS) $(LDFLAGS)

dist:
	mkdir -p $(TARGET)-$(VERSION)
	cp -R README.md $(MANPAGE) $(TARGET) $(TARGET)-$(VERSION)
	tar -cf $(TARGET)-$(VERSION).tar $(TARGET)-$(VERSION)
	gzip $(TARGET)-$(VERSION).tar
	rm -rf $(TARGET)-$(VERSION)

install: $(TARGET)
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(MANDIR)
	cp -p $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	chmod 755 $(DESTDIR)$(BINDIR)/$(TARGET)
	cp -p $(MANPAGE) $(DESTDIR)$(MANDIR)/$(MANPAGE)
	chmod 644 $(DESTDIR)$(MANDIR)/$(MANPAGE)

uninstall:
	$(RM) $(DESTDIR)$(BINDIR)/$(TARGET)
	$(RM) $(DESTDIR)$(MANDIR)/$(MANPAGE)

clean:
	$(RM) $(TARGET)

all: $(TARGET)

.PHONY: all dist install uninstall clean
