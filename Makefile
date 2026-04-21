# pkg install wlroots libxkbcommon wayland-protocols for FreeBSD

PKG_CONFIG  != which pkg-config
PKG_CFLAGS  != PKG_CONFIG_PATH=/usr/local/libdata/pkgconfig pkg-config --cflags wlroots-0.20 wayland-server xkbcommon libinput
PKG_LIBS    != PKG_CONFIG_PATH=/usr/local/libdata/pkgconfig pkg-config --libs   wlroots-0.20 wayland-server xkbcommon libinput





PKGS = wlroots-0.20 wayland-server xkbcommon libinput


CFLAGS      += -std=c17 -Wall -Wextra -Wno-unused-parameter \
               -D_GNU_SOURCE -DWLR_USE_UNSTABLE \
               ${PKG_CFLAGS}

LDFLAGS     += ${PKG_LIBS}

SRC         = slide.c
BIN         = slide-wl

PREFIX      ?= /usr/local

all: $(BIN)

$(BIN): $(SRC) slide.h config.h
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

clean:
	rm -f $(BIN)

install: $(BIN)
	install -Dm755 $(BIN) $(PREFIX)/bin/$(BIN)

.PHONY: all clean install
