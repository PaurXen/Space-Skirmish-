#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "error_handler.h"
#include "CM/console_manager.h"
#include "ipc/ipc_context.h"
#include "ipc/ipc_mesq.h"

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

/* Signal handler */
static void on_term(int sig) {
    (void)sig;
    g_stop = 1;
    printf("\n[CM] Shutting down...\n");
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
    } else if (strcmp(first_word, "end") == 0) {
        cmd->cmd = CM_CMD_END;
        return 0;
    } else if (strcmp(first_word, "help") == 0) {
        printf("\nAvailable commands:\n");
        printf("  freeze / f                      - Pause simulation\n");
        printf("  unfreeze / uf                   - Resume simulation\n");
        printf("  tickspeed [ms] / ts             - Get/set tick speed (0-1000000 ms)\n");
        printf("  spawn <type> <faction> <x> <y>  - Spawn unit at position\n");
        printf("  sp <type> <faction> <x> <y>     - Alias for spawn\n");
        printf("    Types: carrier, destroyer, flagship, fighter, bomber, elite (or 1-6)\n");
        printf("    Factions: republic, cis (or 1-2)\n");
        printf("  end                             - End simulation\n");
        printf("  help                            - Show this help\n");
        printf("  quit                            - Exit console manager\n\n");
        return -1;  /* Don't send */
    } else if (strcmp(first_word, "quit") == 0 || strcmp(first_word, "exit") == 0) {
        g_stop = 1;
        return -1;  /* Don't send */
    } else {
        printf("Unknown command: %s (type 'help' for available commands)\n", first_word);
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
            usleep(10000);  // 10ms
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
    
    printf("[CM] Command sent, waiting for response...\n");
    
    /* Wait for response (blocking) */
    int ret = mq_recv_cm_reply_blocking(ctx->q_rep, &reply);
    if (ret < 0) {
        perror("[CM] Failed to receive reply");
        return -1;
    }
    
    /* Check correlation ID */
    if (reply.req_id != cmd->req_id) {
        fprintf(stderr, "[CM] Reply ID mismatch (expected %u, got %u)\n", 
                cmd->req_id, reply.req_id);
        return -1;
    }
    
    /* Display result */
    if (reply.status == 0) {
        printf("[CM] ✓ Success: %s\n", reply.message);
        /* For TICKSPEED_GET, also show the value clearly */
        if (cmd->cmd == CM_CMD_TICKSPEED_GET) {
            printf("[CM] Tick speed: %d ms\n", reply.tick_speed_ms);
        }
    } else {
        printf("[CM] ✗ Error: %s (status=%d)\n", reply.message, reply.status);
    }
    
    return reply.status;
}



int main(int argc, char **argv) {
    ipc_ctx_t ctx;
    
    /* Set up signal handlers */
    signal(SIGINT, on_term);
    signal(SIGTERM, on_term);
    
    printf("[CM] Console Manager starting (PID %d)...\n", getpid());
    
    /* Attach to existing IPC */
    const char *ftok_path = (argc > 1) ? argv[1] : "./ipc.key";
    
    if (ipc_attach(&ctx, ftok_path) < 0) {
        fprintf(stderr, "[CM] Failed to attach to IPC (is CC running?)\n");
        return 1;
    }
    
    printf("[CM] Connected to IPC (qreq=%d, qrep=%d)\n", ctx.q_req, ctx.q_rep);
    printf("\n=== Space Skirmish Console Manager ===\n");
    printf("Type 'help' for available commands\n\n");
    
    /* Main command loop */
    char line[256];
    while (!g_stop) {
        printf("CM> ");
        fflush(stdout);
        
        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;  /* EOF or error */
        }
        
        mq_cm_cmd_t cmd;
        memset(&cmd, 0, sizeof(cmd));
        
        if (parse_command(line, &cmd) == 0) {
            /* Valid command parsed */
            send_and_wait(&ctx, &cmd);
            
            /* Check if it was an END command */
            if (cmd.cmd == CM_CMD_END) {
                printf("[CM] Simulation ended. Exiting...\n");
                break;
            }
        }
    }
    
    /* Cleanup */
    ipc_detach(&ctx);
    printf("[CM] Console Manager exiting.\n");
    
    return 0;
}
