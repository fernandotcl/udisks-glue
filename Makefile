VERSION=$(shell git describe --tags --abbrev=0)

INSTALL=install
PREFIX=/usr
ifeq ($(PREFIX),/usr)
SYSCONFDIR=/etc
else
SYSCONFDIR=$(PREFIX)/etc
endif

BIN=udisks-glue
SRCS=$(wildcard src/*.c)
OBJS=$(SRCS:.c=.o)
HEADERS=$(wildcard src/*.h)

CFLAGS+=-DSYSCONFDIR='"$(SYSCONFDIR)"'
CFLAGS+=-std=c99 -D_GNU_SOURCE -Wall -Werror
CFLAGS+=$(shell pkg-config --cflags dbus-glib-1)
CFLAGS+=$(shell pkg-config --cflags glib-2.0)
CFLAGS+=$(shell pkg-config --cflags libconfuse)
LDFLAGS+=$(shell pkg-config --libs dbus-glib-1)
LDFLAGS+=$(shell pkg-config --libs glib-2.0)
LDFLAGS+=$(shell pkg-config --libs libconfuse)

.PHONY: all install clean distclean
all: $(BIN)

$(BIN): $(OBJS)
	@echo "LD $<" && \
	$(CC) $(LDFLAGS) -o $(BIN) $(OBJS)

src/%.o: src/%.c $(HEADERS)
	@echo "CC $<" && \
	$(CC) $(CFLAGS) -c -o $@ $<

$(BIN).1: man/$(BIN).1.roff
	@echo "NROFF $<" && \
	nroff -t man $< >$(BIN).1

install: all
	$(INSTALL) -d -m 0755 $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -m 0755 $(BIN) $(DESTDIR)$(PREFIX)/bin

clean:
	@echo 'CLEAN' && \
	rm -f $(OBJS)

distclean: clean
	@echo 'DISTCLEAN' && \
	rm -f $(BIN)
