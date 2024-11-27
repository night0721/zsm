.POSIX:
.SUFFIXES:

VERSION = 1.0
SERVER = zmr
CLIENT = zen
TARGET = zsm
MANPAGE = $(TARGET).1
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man/man1

LDFLAGS != pkg-config --libs libsodium libnotify ncurses sqlite3
LDFLAGS += -L$(PWD)
INCFLAGS != pkg-config --cflags libsodium libnotify ncurses sqlite3
CFLAGS = -Os -mtune=native -march=native -pipe -s -std=c99 -Wno-pointer-sign -pedantic -Wall -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600 $(INCFLAGS) -lpthread -lluft -L.

SERVERSRC != find src/zmr -name "*.c"
CLIENTSRC != find src/zen -name "*.c"
LIBSRC != find src/lib -name "*.c"
INCLUDE = include

all: $(SERVER) $(CLIENT)

$(SERVER): $(SERVERSRC) $(LIBSRC)
	mkdir -p bin
	$(CC) $(SERVERSRC) $(LIBSRC) -I$(INCLUDE) -I. -o bin/$@ $(CFLAGS) $(LDFLAGS)

$(CLIENT): $(CLIENTSRC) $(LIBSRC)
	mkdir -p bin
	$(CC) $(CLIENTSRC) $(LIBSRC) -I$(INCLUDE) -I. -o bin/$@ $(CFLAGS) $(LDFLAGS)

dist:
	mkdir -p $(TARGET)-$(VERSION)
	cp -R README.md $(MANPAGE) bin/$(SERVER) bin/$(CLIENT) $(TARGET)-$(VERSION)
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
	rm $(DESTDIR)$(BINDIR)/$(SERVER)
	rm $(DESTDIR)$(BINDIR)/$(CLIENT)
	rm $(DESTDIR)$(MANDIR)/$(MANPAGE)

clean:
	rm $(SERVER) $(CLIENT)

.PHONY: all dist install uninstall clean
