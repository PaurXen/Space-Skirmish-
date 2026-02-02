CC=gcc
CFLAGS=-O2 -Wall -Wextra -std=c11 -Iinclude

IPC_SRCS=src/ipc/semaphores.c src/ipc/ipc_context.c
IPC_OBJS=$(IPC_SRCS:.c=.o)

all: launcher command_center console_manager battleship squadron

launcher: src/launcher.o
	$(CC) $(CFLAGS) -o launcher $^

command_center: src/CC/command_center.o src/ipc/semaphores.o src/ipc/ipc_context.o src/utils.o src/CC/terminal_tee.o src/ipc/ipc_mesq.o src/CC/unit_logic.o src/CC/unit_ipc.o src/CC/unit_stats.o src/CC/unit_size.o src/CC/weapon_stats.o
	$(CC) $(CFLAGS) -o command_center $^

console_manager: src/CM/console_manager.o src/ipc/ipc_context.o src/ipc/ipc_mesq.o src/ipc/semaphores.o src/utils.o
	$(CC) $(CFLAGS) -o console_manager $^

battleship: src/CC/battleship.o src/ipc/semaphores.o src/ipc/ipc_context.o src/utils.o src/CC/unit_logic.o src/CC/unit_stats.o src/CC/unit_ipc.o src/CC/weapon_stats.o src/ipc/ipc_mesq.o src/CC/unit_size.o
	$(CC) $(CFLAGS) -o battleship $^

squadron: src/CC/squadron.o src/ipc/semaphores.o src/ipc/ipc_context.o src/utils.o src/CC/unit_logic.o src/CC/unit_stats.o src/CC/unit_ipc.o src/CC/weapon_stats.o src/ipc/ipc_mesq.o src/CC/unit_size.o
	$(CC) $(CFLAGS) -o squadron $^ -lm

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

src/ipc/%.o: src/ipc/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

src/CC/%.o: src/CC/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

src/CM/%.o: src/CM/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f launcher command_center console_manager battleship squadron src/*.o src/ipc/*.o src/CC/*.o src/CM/*.o
