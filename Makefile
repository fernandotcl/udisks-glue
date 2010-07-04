BIN=udisks-glue
SRCS=$(wildcard src/*.c)
OBJS=$(SRCS:.c=.o)
HEADERS=$(wildcard src/*.h)

CFLAGS+=-DPREFIX=
CFLAGS+=-std=c99 -Wall -Werror
CFLAGS+=$(shell pkg-config --cflags dbus-glib-1)
CFLAGS+=$(shell pkg-config --cflags glib-2.0)
CFLAGS+=$(shell pkg-config --cflags libconfuse)
LDFLAGS+=$(shell pkg-config --libs dbus-glib-1)
LDFLAGS+=$(shell pkg-config --libs glib-2.0)
LDFLAGS+=$(shell pkg-config --libs libconfuse)

.PHONY: all clean distclean
all: $(BIN)

$(BIN): $(OBJS)
	@echo "LD $<" && \
	$(CC) $(LDFLAGS) -o $(BIN) $(OBJS)

src/%.o: src/%.c $(HEADERS)
	@echo "CC $<" && \
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@echo 'CLEAN' && \
	rm -f $(OBJS)

distclean: clean
	@echo 'DISTCLEAN' && \
	rm -f $(BIN)
