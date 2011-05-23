CC=gcc
CFLAGS=-Wall -Wextra -O2 -pipe

all:
	$(CC) $(CFLAGS) src/main.c -o runvpn

clean:
	rm runvpn
