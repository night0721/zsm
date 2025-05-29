.POSIX:

VERSION = 1.0
SERVER = zmr
CLIENT = zen
TARGET = zsm
LIB = libzsm.so
MANPAGE = $(TARGET).1
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man/man1

LDFLAGS != pkg-config --libs libsodium ncurses sqlite3
LDFLAGS += -L$(PWD)
INCFLAGS != pkg-config --cflags libsodium ncurses sqlite3
CFLAGS = -g3 -std=c99 -Wno-pointer-sign -pedantic -Wall -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600 $(INCFLAGS) -lpthread

SERVERSRC != find src/zmr -name "*.c"
CLIENTSRC != find src/zen -name "*.c"
LIBSRC != find src/lib -name "*.c"
INCLUDE = include

$(SERVER): $(SERVERSRC) $(LIBSRC)
	mkdir -p bin
	$(CC) $(SERVERSRC) $(LIBSRC) -I$(INCLUDE) -o bin/$@ $(CFLAGS) $(LDFLAGS)

$(CLIENT): $(CLIENTSRC) $(LIBSRC)
	mkdir -p bin
	$(CC) $(CLIENTSRC) $(LIBSRC) -I$(INCLUDE) -o bin/$@ $(CFLAGS) $(LDFLAGS)

$(LIB): $(LIBSRC)
	mkdir -p bin
	$(CC) $(LIBSRC) -I$(INCLUDE) -I. -fPIC -shared -o bin/$@ $(CFLAGS) $(LDFLAGS)

doc:
	cd doc && typst compile main.typ main.pdf
	cd ..

watch-doc:
	cd doc && typst watch main.typ

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

all: $(SERVER) $(CLIENT)

.PHONY: all dist install uninstall clean
