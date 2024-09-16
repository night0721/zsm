.POSIX:
.SUFFIXES:

CC = cc
VERSION = 1.0
SERVER = zmr
CLIENT = zen
TARGET = zsm
MANPAGE = $(TARGET).1
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man/man1

# Flags
LDFLAGS = $(shell pkg-config --libs libsodium libnotify ncurses sqlite3)
CFLAGS = -O3 -mtune=native -march=native -pipe -g -std=c99 -Wno-pointer-sign -pedantic -Wall -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600 $(shell pkg-config --cflags libsodium libnotify ncurses sqlite3) -lpthread

SERVERSRC = src/server/*.c
CLIENTSRC = src/client/*.c
LIBSRC = lib/*.c
INCLUDE = -Iinclude/

all: $(SERVER) $(CLIENT)

$(SERVER): $(SERVERSRC) $(LIBSRC)
	mkdir -p bin
	$(CC) $(SERVERSRC) $(LIBSRC) $(INCLUDE) -o bin/$@ $(CFLAGS) $(LDFLAGS)

$(CLIENT): $(CLIENTSRC) $(LIBSRC)
	mkdir -p bin
	$(CC) $(CLIENTSRC) $(LIBSRC) $(INCLUDE) -o bin/$@ $(CFLAGS) $(LDFLAGS)

dist:
	mkdir -p $(TARGET)-$(VERSION)
	cp -R README.md $(MANPAGE) $(SERVER) $(CLIENT) $(TARGET)-$(VERSION)
	tar -cf $(TARGET)-$(VERSION).tar $(TARGET)-$(VERSION)
	gzip $(TARGET)-$(VERSION).tar
	rm -rf $(TARGET)-$(VERSION)

install: $(TARGET)
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(MANDIR)
	cp -p bin/$(TARGET) $(DESTDIR)$(BINDIR)/$(SERVER)
	chmod 755 $(DESTDIR)$(BINDIR)/$(SERVER)
	cp -p bin/$(TARGET) $(DESTDIR)$(BINDIR)/$(CLIENT)
	chmod 755 $(DESTDIR)$(BINDIR)/$(CLIENT)
	cp -p $(MANPAGE) $(DESTDIR)$(MANDIR)/$(MANPAGE)
	chmod 644 $(DESTDIR)$(MANDIR)/$(MANPAGE)

uninstall:
	$(RM) $(DESTDIR)$(BINDIR)/$(SERVER)
	$(RM) $(DESTDIR)$(BINDIR)/$(CLIENT)
	$(RM) $(DESTDIR)$(MANDIR)/$(MANPAGE)

clean:
	$(RM) $(SERVER) $(CLIENT)

run:
	./bin/zen

.PHONY: all dist install uninstall clean
