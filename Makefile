CC=gcc
CFLAGS=-O2 -Wall -Wextra -std=c11

SRC=src
BIN=.

all: command_center battleship

command_center: $(SRC)/command_center.c
	$(CC) $(CFLAGS) -o $(BIN)/command_center $(SRC)/command_center.c

battleship: $(SRC)/battleship.c
	$(CC) $(CFLAGS) -o $(BIN)/battleship $(SRC)/battleship.c

clean:
	rm -f command_center battleship
