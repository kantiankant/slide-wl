# FreeBSD's wlroots package does not ship the protocol XMLs. Vendor the XML
# into protocols/ before building:
#
#   mkdir -p protocols
#   fetch -o protocols/wlr-layer-shell-unstable-v1.xml \
#     https://raw.githubusercontent.com/swaywm/wlroots/master/protocol/wlr-layer-shell-unstable-v1.xml

PKG_CFLAGS  != PKG_CONFIG_PATH=/usr/local/libdata/pkgconfig pkg-config --cflags wlroots-0.20 wayland-server xkbcommon libinput
PKG_LIBS    != PKG_CONFIG_PATH=/usr/local/libdata/pkgconfig pkg-config --libs   wlroots-0.20 wayland-server xkbcommon libinput

CFLAGS      += -std=c17 -Wall -Wextra -Wno-unused-parameter \
               -D_GNU_SOURCE -DWLR_USE_UNSTABLE \
               -I. \
               ${PKG_CFLAGS}

LDFLAGS     += ${PKG_LIBS}

SRC         = slide.c
BIN         = slide-wl
PREFIX      ?= /usr/local

all: $(BIN)

wlr-layer-shell-unstable-v1-protocol.h: protocols/wlr-layer-shell-unstable-v1.xml
	wayland-scanner server-header protocols/wlr-layer-shell-unstable-v1.xml wlr-layer-shell-unstable-v1-protocol.h

$(BIN): wlr-layer-shell-unstable-v1-protocol.h $(SRC) slide.h config.h
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

clean:
	rm -f $(BIN) wlr-layer-shell-unstable-v1-protocol.h

install: $(BIN)
	install -Dm755 $(BIN) $(PREFIX)/bin/$(BIN)

.PHONY: all clean install
