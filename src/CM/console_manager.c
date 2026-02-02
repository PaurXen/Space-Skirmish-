#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

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
    char first_word[64];
    int tick_val = -1;
    int args = sscanf(buffer, "%63s %d", first_word, &tick_val);
    
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
        if (tick_val == -1) {
            /* No argument - query current value */
            cmd->cmd = CM_CMD_TICKSPEED_GET;
        } else {
            /* Argument provided - set value */
            cmd->cmd = CM_CMD_TICKSPEED_SET;
            cmd->tick_speed_ms = tick_val;
        }
        return 0;
    } else if (strcmp(first_word, "end") == 0) {
        cmd->cmd = CM_CMD_END;
        return 0;
    } else if (strcmp(first_word, "help") == 0) {
        printf("\nAvailable commands:\n");
        printf("  freeze / f           - Pause simulation\n");
        printf("  unfreeze / uf        - Resume simulation\n");
        printf("  tickspeed [ms] / ts  - Get/set tick speed (0-1000000 ms)\n");
        printf("                         No argument: show current value\n");
        printf("                         With value: set tick speed\n");
        printf("  end                  - End simulation\n");
        printf("  help                 - Show this help\n");
        printf("  quit                 - Exit console manager\n\n");
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
    mq_cm_rep_t reply;
    
    cmd->mtype = MSG_CM_CMD;
    cmd->sender = getpid();
    cmd->req_id = g_next_req_id++;
    
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
