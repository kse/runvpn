CC       = gcc
CFLAGS  ?= -Wall -Wextra -O2 -pipe
OUTPUT   = runvpn

INSTALL  = install
PREFIX   = /usr/local
BIN_DIR  = $(PREFIX)/bin
BASH_COMPLETION_DIR = /etc/bash_completion.d
OBJECT_FILES		= src/runvpn.o src/vpn.o

program  = runvpn

all:  $(program)

runvpn: $(OBJECT_FILES)
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f runvpn
	rm -f src/*.o

install:
	$(INSTALL) -o root -g root -m 4555 --strip runvpn $(DESTDIR)$(BIN_DIR)
	$(INSTALL) -o root -g root -m 0644 runvpn.bash_completion $(DESTDIR)$(BASH_COMPLETION_DIR)/runvpn
