CC=gcc
CFLAGS=-Wall -Wextra -O2 -pipe
SRC=src/
OUTPUT=runvpn

all:
	$(CC) $(CFLAGS) src/runvpn.c -o $(OUTPUT)

clean:
	rm runvpn
