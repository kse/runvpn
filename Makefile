CC       = gcc
CFLAGS  ?= -Wall -Wextra -O3 -pipe
OUTPUT   = runvpn

DESTDIR ?= /
INSTALL  = install
PREFIX   = usr
BIN_DIR  = $(PREFIX)/bin
BASH_COMPLETION_DIR = etc/bash_completion.d
OBJECT_FILES		= code/runvpn.o code/vpn.o

program  = runvpn

all:  $(program)

runvpn: $(OBJECT_FILES)
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OUTPUT)
	rm -f code/*.o

pkgclean:
	rm -rf src
	rm -rf build
	rm -rf pkg
	rm -f runvpn-git*.pkg.tar.xz

install:
	$(INSTALL) -D -o root -g root -m 4555 --strip runvpn $(DESTDIR)$(BIN_DIR)/$(OUTPUT)
	$(INSTALL) -D -o root -g root -m 0644 runvpn.bash_completion $(DESTDIR)$(BASH_COMPLETION_DIR)/$(OUTPUT)
