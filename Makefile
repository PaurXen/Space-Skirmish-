CC=gcc
CFLAGS=-O2 -Wall -Wextra -std=c11 -Iinclude

IPC_SRCS=src/ipc/semaphores.c src/ipc/ipc_context.c
IPC_OBJS=$(IPC_SRCS:.c=.o)

all: command_center battleship squadron

command_center: src/command_center.o src/ipc/semaphores.o src/ipc/ipc_context.o src/utils.o src/terminal_tee.o src/ipc/ipc_mesq.o src/unit_logic.o src/unit_ipc.o src/unit_stats.o src/unit_size.o src/weapon_stats.o
	$(CC) $(CFLAGS) -o command_center $^

battleship: src/battleship.o src/ipc/semaphores.o src/ipc/ipc_context.o src/utils.o src/unit_logic.o src/unit_stats.o src/unit_ipc.o src/weapon_stats.o src/ipc/ipc_mesq.o src/unit_size.o
	$(CC) $(CFLAGS) -o battleship $^

squadron: src/squadron.o src/ipc/semaphores.o src/ipc/ipc_context.o src/utils.o src/unit_logic.o src/unit_stats.o src/unit_ipc.o src/weapon_stats.o src/ipc/ipc_mesq.o src/unit_size.o
	$(CC) $(CFLAGS) -o squadron $^ -lm

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

src/ipc/%.o: src/ipc/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f command_center battleship squadron src/*.o src/ipc/*.o
