CC=gcc
CFLAGS=-I.

shell: shell.o
	$(CC) -o shell shell.o $(CFLAGS)
