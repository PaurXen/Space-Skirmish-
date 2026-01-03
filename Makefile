CC=gcc
CFLAGS=-O2 -Wall -Wextra -std=c11 -Iinclude

IPC_SRCS=src/ipc/semaphores.c src/ipc/ipc_context.c
IPC_OBJS=$(IPC_SRCS:.c=.o)

all: command_center battleship

command_center: src/command_center.o $(IPC_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

battleship: src/battleship.o $(IPC_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

src/ipc/%.o: src/ipc/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f command_center battleship src/*.o src/ipc/*.o
