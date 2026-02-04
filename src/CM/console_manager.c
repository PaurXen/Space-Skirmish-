#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <stdarg.h>

#include "error_handler.h"
#include "CM/console_manager.h"
#include "ipc/ipc_context.h"
#include "ipc/ipc_mesq.h"
#include "log.h"

/* Console Manager (CM)
 *
 * Responsibilities:
 *  - Provide terminal interface for user commands
 *  - Send commands to CC via message queue
 *  - Wait for responses before accepting new input
 *  - Parse and validate user input
 */

static volatile sig_atomic_t g_stop = 0;
static uint32_t g_next_req_id = 1;
static int g_ui_output_fd = -1;  // Global for relay output

/* Signal handler */
static void on_term(int sig) {
    (void)sig;
    g_stop = 1;
    printf("\n[CM] Shutting down...\n");
}

/* Relay printf - write to both stdout and UI if connected */
static void relay_printf(const char *format, ...) {
    va_list args1, args2;
    va_start(args1, format);
    
    /* Always write to stdout */
    va_copy(args2, args1);
    vprintf(format, args1);
    fflush(stdout);
    va_end(args1);
    
    /* Also write to UI if connected */
    if (g_ui_output_fd >= 0) {
        char buffer[4096];
        vsnprintf(buffer, sizeof(buffer), format, args2);
        ssize_t written = write(g_ui_output_fd, buffer, strlen(buffer));
        (void)written;  // Suppress unused result warning
    }
    va_end(args2);
}

/* Parse command line input */
static int parse_command(const char *line, mq_cm_cmd_t *cmd) {
    char buffer[256];
    strncpy(buffer, line, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    /* Remove trailing newline */
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len-1] == '\n') {
        buffer[len-1] = '\0';
    }
    
    /* Parse first word */
    char first_word[64], type_arg[64], faction_arg[64];
    int tick_val = -1;
    int x_val = 0, y_val = 0;
    
    int args = sscanf(buffer, "%63s", first_word);
    if (args < 1) {
        return -1;  /* Empty line */
    }
    
    /* Parse command */
    if (strcmp(first_word, "freeze") == 0 || strcmp(first_word, "f") == 0) {
        cmd->cmd = CM_CMD_FREEZE;
        return 0;
    } else if (strcmp(first_word, "unfreeze") == 0 || strcmp(first_word, "uf") == 0) {
        cmd->cmd = CM_CMD_UNFREEZE;
        return 0;
    } else if (strcmp(first_word, "tickspeed") == 0 || strcmp(first_word, "ts") == 0) {
        if (sscanf(buffer, "%*s %d", &tick_val) == 1) {
            /* Argument provided - set value */
            cmd->cmd = CM_CMD_TICKSPEED_SET;
            cmd->tick_speed_ms = tick_val;
        } else {
            /* No argument - query current value */
            cmd->cmd = CM_CMD_TICKSPEED_GET;
        }
        return 0;
    } else if (strcmp(first_word, "spawn") == 0 || strcmp(first_word, "sp") == 0) {
        /* Parse: spawn <type> <faction> <x> <y> */
        args = sscanf(buffer, "%*s %63s %63s %d %d", type_arg, faction_arg, &x_val, &y_val);
        if (args != 4) {
            printf("Usage: spawn <type> <faction> <x> <y>\n");
            printf("Types: carrier/3, destroyer/2, flagship/1, fighter/4, bomber/5, elite/6\n");
            printf("Factions: republic/1, cis/2\n");
            return -1;
        }
        
        /* Parse type (name or number) */
        if (strcmp(type_arg, "carrier") == 0 || strcmp(type_arg, "3") == 0) {
            cmd->spawn_type = 3;  // TYPE_CARRIER
        } else if (strcmp(type_arg, "destroyer") == 0 || strcmp(type_arg, "2") == 0) {
            cmd->spawn_type = 2;  // TYPE_DESTROYER
        } else if (strcmp(type_arg, "flagship") == 0 || strcmp(type_arg, "1") == 0) {
            cmd->spawn_type = 1;  // TYPE_FLAGSHIP
        } else if (strcmp(type_arg, "fighter") == 0 || strcmp(type_arg, "4") == 0) {
            cmd->spawn_type = 4;  // TYPE_FIGHTER
        } else if (strcmp(type_arg, "bomber") == 0 || strcmp(type_arg, "5") == 0) {
            cmd->spawn_type = 5;  // TYPE_BOMBER
        } else if (strcmp(type_arg, "elite") == 0 || strcmp(type_arg, "6") == 0) {
            cmd->spawn_type = 6;  // TYPE_ELITE
        } else {
            printf("Invalid type: %s\n", type_arg);
            return -1;
        }
        
        /* Parse faction (name or number) */
        if (strcmp(faction_arg, "republic") == 0 || strcmp(faction_arg, "1") == 0) {
            cmd->spawn_faction = 1;  // FACTION_REPUBLIC
        } else if (strcmp(faction_arg, "cis") == 0 || strcmp(faction_arg, "2") == 0) {
            cmd->spawn_faction = 2;  // FACTION_CIS
        } else {
            printf("Invalid faction: %s\n", faction_arg);
            return -1;
        }
        
        cmd->spawn_x = x_val;
        cmd->spawn_y = y_val;
        cmd->cmd = CM_CMD_SPAWN;  // For internal tracking
        return 0;
    } else if (strcmp(first_word, "grid") == 0 || strcmp(first_word, "g") == 0) {
        /* Parse optional on/off argument */
        char grid_arg[16];
        if (sscanf(buffer, "%*s %15s", grid_arg) == 1) {
            if (strcmp(grid_arg, "1") == 0 || strcmp(grid_arg, "T") == 0 || 
                strcmp(grid_arg, "on") == 0 || strcmp(grid_arg, "true") == 0) {
                cmd->grid_enabled = 1;
            } else if (strcmp(grid_arg, "0") == 0 || strcmp(grid_arg, "F") == 0 || 
                       strcmp(grid_arg, "off") == 0 || strcmp(grid_arg, "false") == 0) {
                cmd->grid_enabled = 0;
            } else {
                relay_printf("Usage: grid [1/T/0/F]\n");
                return -1;
            }
        } else {
            /* No argument - query current value */
            cmd->grid_enabled = -1;
        }
        cmd->cmd = CM_CMD_GRID;
        return 0;
    } else if (strcmp(first_word, "end") == 0) {
        cmd->cmd = CM_CMD_END;
        return 0;
    } else if (strcmp(first_word, "help") == 0) {
        relay_printf("\nAvailable commands:\n");
        relay_printf("  freeze / f                      - Pause simulation\n");
        relay_printf("  unfreeze / uf                   - Resume simulation\n");
        relay_printf("  tickspeed [ms] / ts             - Get/set tick speed (0-1000000 ms)\n");
        relay_printf("  grid [on|off] / g               - Toggle/set grid display\n");
        relay_printf("  spawn <type> <faction> <x> <y>  - Spawn unit at position\n");
        relay_printf("  sp <type> <faction> <x> <y>     - Alias for spawn\n");
        relay_printf("    Types: carrier, destroyer, flagship, fighter, bomber, elite (or 1-6)\n");
        relay_printf("    Factions: republic, cis (or 1-2)\n");
        relay_printf("  end                             - End simulation\n");
        relay_printf("  help                            - Show this help\n");
        relay_printf("  quit                            - Exit console manager\n\n");
        return -1;  /* Don't send */
    } else if (strcmp(first_word, "quit") == 0 || strcmp(first_word, "exit") == 0) {
        g_stop = 1;
        return -1;  /* Don't send */
    } else {
        relay_printf("Unknown command: %s (type 'help' for available commands)\n", first_word);
        return -1;  /* Don't send */
    }
}

/* Send command and wait for response */
static int send_and_wait(ipc_ctx_t *ctx, mq_cm_cmd_t *cmd) {
    uint32_t req_id = g_next_req_id++;
    
    /* Handle spawn command specially - use MSG_SPAWN like BS does */
    if (cmd->cmd == CM_CMD_SPAWN) {
        mq_spawn_req_t spawn_req;
        spawn_req.mtype = MSG_SPAWN;
        spawn_req.sender = getpid();
        spawn_req.sender_id = 0;  // CM has no unit_id
        spawn_req.pos.x = cmd->spawn_x;
        spawn_req.pos.y = cmd->spawn_y;
        spawn_req.utype = cmd->spawn_type;
        spawn_req.faction = cmd->spawn_faction;
        spawn_req.req_id = req_id;
        spawn_req.commander_id = 0;  // No commander for CM spawns
        
        printf("[CM] Sending spawn request: type=%d faction=%d pos=(%d,%d)\n",
               spawn_req.utype, spawn_req.faction, spawn_req.pos.x, spawn_req.pos.y);
        
        if (mq_send_spawn(ctx->q_req, &spawn_req) < 0) {
            perror("[CM] Failed to send spawn request");
            return -1;
        }
        
        printf("[CM] Spawn request sent, waiting for response...\n");
        
        /* Wait for spawn reply */
        mq_spawn_rep_t spawn_reply;
        int ret = mq_try_recv_reply(ctx->q_rep, &spawn_reply);
        
        /* Blocking wait for reply */
        while (ret == 0 && !g_stop) {
            //usleep(10000);  // 10ms
            ret = mq_try_recv_reply(ctx->q_rep, &spawn_reply);
        }
        
        if (ret < 0) {
            perror("[CM] Failed to receive spawn reply");
            return -1;
        }
        
        if (g_stop) return -1;
        
        /* Check correlation ID */
        if (spawn_reply.req_id != req_id) {
            fprintf(stderr, "[CM] Spawn reply ID mismatch (expected %u, got %u)\n",
                    req_id, spawn_reply.req_id);
            return -1;
        }
        
        /* Display result */
        if (spawn_reply.status == 0) {
            printf("[CM] ✓ Success: Spawned unit %u at (%d,%d) pid=%d\n",
                   spawn_reply.child_unit_id, spawn_req.pos.x, spawn_req.pos.y,
                   spawn_reply.child_pid);
        } else {
            printf("[CM] ✗ Error: Spawn failed (status=%d)\n", spawn_reply.status);
        }
        
        return spawn_reply.status;
    }
    
    /* For non-spawn commands, use regular CM command protocol */
    mq_cm_rep_t reply;
    
    cmd->mtype = MSG_CM_CMD;
    cmd->sender = getpid();
    cmd->req_id = req_id;
    
    /* Send command */
    if (mq_send_cm_cmd(ctx->q_req, cmd) < 0) {
        perror("[CM] Failed to send command");
        return -1;
    }
    
    relay_printf("[CM] Command sent, waiting for response...\n");
    
    /* Wait for response (blocking) */
    int ret = mq_recv_cm_reply_blocking(ctx->q_rep, &reply);
    if (ret < 0) {
        perror("[CM] Failed to receive reply");
        return -1;
    }
    
    /* Check correlation ID */
    if (reply.req_id != cmd->req_id) {
        relay_printf("[CM] Reply ID mismatch (expected %u, got %u)\n", 
                cmd->req_id, reply.req_id);
        return -1;
    }
    
    /* Display result */
    if (reply.status == 0) {
        relay_printf("[CM] ✓ Success: %s\n", reply.message);
        /* For TICKSPEED_GET, also show the value clearly */
        if (cmd->cmd == CM_CMD_TICKSPEED_GET) {
            relay_printf("[CM] Tick speed: %d ms\n", reply.tick_speed_ms);
        }
    } else {
        relay_printf("[CM] ✗ Error: %s (status=%d)\n", reply.message, reply.status);
    }
    
    return reply.status;
}



int main(int argc, char **argv) {
    ipc_ctx_t ctx;
    
    /* Initialize logging */
    log_init("CM", 0);
    LOGI("[CM] Console Manager starting (PID %d)...", getpid());
    
    /* Set up signal handlers */
    signal(SIGINT, on_term);
    signal(SIGTERM, on_term);
    
    printf("[CM] Console Manager starting (PID %d)...\n", getpid());
    
    /* Attach to existing IPC */
    const char *ftok_path = (argc > 1) ? argv[1] : "./ipc.key";
    
    if (ipc_attach(&ctx, ftok_path) < 0) {
        fprintf(stderr, "[CM] Failed to attach to IPC (is CC running?)\n");
        LOGE("[CM] Failed to attach to IPC");
        return 1;
    }
    
    LOGI("[CM] Connected to IPC (qreq=%d, qrep=%d)", ctx.q_req, ctx.q_rep);
    printf("[CM] Connected to IPC (qreq=%d, qrep=%d)\n", ctx.q_req, ctx.q_rep);
    
    /* Create FIFOs for UI communication */
    const char *cm_to_ui = "/tmp/skirmish_cm_to_ui.fifo";
    const char *ui_to_cm = "/tmp/skirmish_ui_to_cm.fifo";
    
    unlink(cm_to_ui);
    unlink(ui_to_cm);
    
    if (mkfifo(cm_to_ui, 0600) < 0 && errno != EEXIST) {
        perror("[CM] mkfifo cm_to_ui");
    }
    if (mkfifo(ui_to_cm, 0600) < 0 && errno != EEXIST) {
        perror("[CM] mkfifo ui_to_cm");
    }
    
    int ui_input_fd = -1;
    int ui_output_fd = -1;
    
    /* Try to open UI FIFOs in non-blocking mode */
    ui_output_fd = open(cm_to_ui, O_WRONLY | O_NONBLOCK);
    if (ui_output_fd >= 0) {
        printf("[CM] UI connected!\n");
        g_ui_output_fd = ui_output_fd;
        ui_input_fd = open(ui_to_cm, O_RDONLY | O_NONBLOCK);
    } else if (errno == ENXIO) {
        printf("[CM] No UI detected, using terminal mode\n");
    }
    
    relay_printf("\n=== Space Skirmish Console Manager ===\n");
    relay_printf("Type 'help' for available commands\n\n");
    relay_printf("CM> ");
    fflush(stdout);
    
    /* Main command loop - select between stdin and UI */
    char line[256];
    while (!g_stop) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        
        int maxfd = STDIN_FILENO;
        if (ui_input_fd >= 0) {
            FD_SET(ui_input_fd, &readfds);
            if (ui_input_fd > maxfd) maxfd = ui_input_fd;
        }
        
        struct timeval tv = {1, 0};  // 1 second timeout
        int ret = select(maxfd + 1, &readfds, NULL, NULL, &tv);
        
        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("[CM] select");
            break;
        }
        
        if (ret == 0) {
            /* Timeout - try to reconnect UI if disconnected */
            if (ui_output_fd < 0) {
                ui_output_fd = open(cm_to_ui, O_WRONLY | O_NONBLOCK);
                if (ui_output_fd >= 0) {
                    printf("[CM] UI connected!\n");
                    g_ui_output_fd = ui_output_fd;
                    ui_input_fd = open(ui_to_cm, O_RDONLY | O_NONBLOCK);
                }
            }
            continue;
        }
        
        /* Check stdin */
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            if (fgets(line, sizeof(line), stdin) == NULL) {
                break;
            }
            
            mq_cm_cmd_t cmd;
            memset(&cmd, 0, sizeof(cmd));
            
            if (parse_command(line, &cmd) == 0) {
                send_and_wait(&ctx, &cmd);
                
                if (cmd.cmd == CM_CMD_END) {
                    relay_printf("[CM] Simulation ended. Exiting...\n");
                    break;
                }
            }
            
            /* Print prompt for next command */
            relay_printf("CM> ");
            fflush(stdout);
        }
        
        /* Check UI FIFO */
        if (ui_input_fd >= 0 && FD_ISSET(ui_input_fd, &readfds)) {
            ssize_t n = read(ui_input_fd, line, sizeof(line) - 1);
            if (n <= 0) {
                relay_printf("[CM] UI disconnected\n");
                close(ui_input_fd);
                close(ui_output_fd);
                ui_input_fd = -1;
                ui_output_fd = -1;
                g_ui_output_fd = -1;
            } else {
                line[n] = '\0';
                line[strcspn(line, "\n")] = '\0';
                
                if (line[0] != '\0') {
                    mq_cm_cmd_t cmd;
                    memset(&cmd, 0, sizeof(cmd));
                    
                    if (parse_command(line, &cmd) == 0) {
                        send_and_wait(&ctx, &cmd);
                        
                        if (cmd.cmd == CM_CMD_END) {
                            relay_printf("[CM] Simulation ended. Exiting...\n");
                            break;
                        }
                    }
                    
                    /* Don't print prompt for UI commands - they don't see it */
                }
            }
        }
    }
    
    /* Cleanup */
    if (ui_output_fd >= 0) close(ui_output_fd);
    if (ui_input_fd >= 0) close(ui_input_fd);
    unlink(cm_to_ui);
    unlink(ui_to_cm);
    
    ipc_detach(&ctx);
    printf("[CM] Console Manager exiting.\n");
    
    return 0;
}
