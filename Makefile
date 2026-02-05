CC=gcc
CFLAGS=-O2 -Wall -Wextra -std=c11 -Iinclude

IPC_SRCS=src/ipc/semaphores.c src/ipc/ipc_context.c
IPC_OBJS=$(IPC_SRCS:.c=.o)

# Error handler object - used by all binaries (depends on utils.o for logging)
ERROR_HANDLER_OBJ=src/error_handler.o

all: command_center console_manager battleship squadron ui

command_center: src/CC/command_center.o src/ipc/semaphores.o src/ipc/ipc_context.o src/utils.o src/tee/terminal_tee.o src/ipc/ipc_mesq.o src/CC/unit_logic.o src/CC/unit_ipc.o src/CC/unit_stats.o src/CC/unit_size.o src/CC/weapon_stats.o src/CC/scenario.o $(ERROR_HANDLER_OBJ)
	$(CC) $(CFLAGS) -o command_center $^ -lpthread

console_manager: src/CM/console_manager.o src/ipc/ipc_context.o src/ipc/ipc_mesq.o src/ipc/semaphores.o src/utils.o $(ERROR_HANDLER_OBJ)
	$(CC) $(CFLAGS) -o console_manager $^

battleship: src/CC/battleship.o src/ipc/semaphores.o src/ipc/ipc_context.o src/utils.o src/CC/unit_logic.o src/CC/unit_stats.o src/CC/unit_ipc.o src/CC/weapon_stats.o src/ipc/ipc_mesq.o src/CC/unit_size.o $(ERROR_HANDLER_OBJ)
	$(CC) $(CFLAGS) -o battleship $^

squadron: src/CC/squadron.o src/ipc/semaphores.o src/ipc/ipc_context.o src/utils.o src/CC/unit_logic.o src/CC/unit_stats.o src/CC/unit_ipc.o src/CC/weapon_stats.o src/ipc/ipc_mesq.o src/CC/unit_size.o $(ERROR_HANDLER_OBJ)
	$(CC) $(CFLAGS) -o squadron $^ -lm

ui: src/UI/ui_main.o src/UI/ui_map.o src/UI/ui_std.o src/UI/ui_ust.o src/ipc/ipc_context.o src/ipc/semaphores.o src/ipc/ipc_mesq.o src/utils.o $(ERROR_HANDLER_OBJ)
	$(CC) $(CFLAGS) -o ui $^ -lncurses -lpthread

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

src/ipc/%.o: src/ipc/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

src/CC/%.o: src/CC/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

src/CM/%.o: src/CM/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

src/tee/%.o: src/tee/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

src/UI/%.o: src/UI/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f command_center console_manager battleship squadron ui src/*.o src/ipc/*.o src/CC/*.o src/CM/*.o src/tee/*.o src/UI/*.o
