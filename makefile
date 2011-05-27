CC=gcc
CFLAGS=-Wall -Wextra -O2 -pipe
SRC=src/
OUTPUT=runvpn
BIN_INSTALL_PATH=/usr/bin
BASH_COMPLETION_DIR=/etc/bash_completion.d

all:
	$(CC) $(CFLAGS) src/runvpn.c -o $(OUTPUT)

clean:
	rm runvpn

install:
	install -g root -o root -m 4555 --strip runvpn $(BIN_INSTALL_PATH)
	install -g root -o root -m 0644 runvpn.bash_completion $(BASH_COMPLETION_DIR)/runvpn
