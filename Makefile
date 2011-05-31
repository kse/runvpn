CC       = gcc
CFLAGS  ?= -Wall -Wextra -O2 -pipe
OUTPUT   = runvpn

INSTALL  = install
PREFIX   = /usr
BIN_DIR  = $(PREFIX)/bin
BASH_COMPLETION_DIR = /etc/bash_completion.d

all:
	$(CC) $(CFLAGS) src/runvpn.c -o $(OUTPUT)

clean:
	rm -f runvpn

install:
	$(INSTALL) -o root -g root -m 4555 --strip runvpn $(DESTDIR)$(BIN_DIR)
	$(INSTALL) -o root -g root -m 0644 runvpn.bash_completion $(DESTDIR)$(BASH_COMPLETION_DIR)/runvpn
